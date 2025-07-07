/* Copyright (c) 2024-2025, CK Tan.
 * https://github.com/cktan/csvc17/blob/main/LICENSE
 */
#include "csvc17.h"
#include "scan.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 *  Unquote a value and return a NUL-terminated string.
 *  This will modify memory area value.ptr[0 .. len+1].
 */
static void unquote(scan_t *scan, csv_value_t *value, const csv_config_t *conf);

#define DO(x)                                                                  \
  if (x)                                                                       \
    return -1;                                                                 \
  else                                                                         \
    (void)0

// Error buffer
typedef struct ebuf_t ebuf_t;
struct ebuf_t {
  char *ptr;
  int len;
};

// Keeps track of counters
typedef struct status_t status_t;
struct status_t {
  int64_t lineno; // current line number
  int64_t rowno;  // current row number
  // note: current column number is (csvx_t::value.top + 1)
};

// Control block
typedef struct csvx_t csvx_t;
struct csvx_t {
  ebuf_t ebuf; // error buffer
  bool eof;    // true if feed() signaled EOF

  status_t status;
  csv_config_t conf;

  // buf[] stores the raw text to be parsed. Feed() will
  // copy into the buf[].
  struct {
    char *ptr; // buf of size max where [bot..top) is valid
    int bot, top, max;
  } buf;

  // value[] stores each value of a row. Appended by onerow().
  // Will be sent to perrow().
  struct {
    csv_value_t *ptr; // value[0..top) are valid
    int top, max;
  } value;

  // This is a hack for csv_parse_file().
  FILE *fp; // file ptr if not NULL
};

// True if EOF and buffer is empty
static inline bool finished(const csvx_t *cb) {
  return cb->eof && (cb->buf.bot == cb->buf.top);
}

// Read from a file into buf[]. Used by csv_parse_file().
static int read_file(void *context, char *buf, int bufsz, char *errmsg,
                     int errsz) {
  FILE *fp = (FILE *)context;
  int N = fread(buf, 1, bufsz, fp);
  if (ferror(fp)) {
    snprintf(errmsg, errsz, "%s", "cannot read file");
    return -1;
  }
  return N;
}

/*
 *  Format an error into ebuf[]. Always return -1.
 */
static int RETERROR(csvx_t *cb, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char *p = cb->ebuf.ptr;
  char *q = p + cb->ebuf.len;
  snprintf(p, q - p, "(line %" PRId64 ", row %" PRId64 ", col %d)",
           cb->status.lineno, cb->status.rowno, cb->value.top + 1);
  p += strlen(p);
  vsnprintf(p, q - p, fmt, args);
  return -1;
}

//////////////////
// grow cb->value[]
static int expand_value(csvx_t *cb) {
  int max = cb->value.max;
#ifndef NDEBUG
  max = max * 1.5 + 10;
#else
  max = max + 1;
#endif
  if (max < 0) {
    return RETERROR(cb, "%s", "buffer overflow");
  }

  csv_value_t *newval =
      (csv_value_t *)realloc(cb->value.ptr, max * sizeof(*newval));
  if (!newval) {
    return RETERROR(cb, "%s", "out of memory");
  }
  cb->value.ptr = newval;
  cb->value.max = max;
  assert(cb->value.top < cb->value.max);
  return 0;
}

//////////////////
// make sure csv->value[] can accomodate at least one value
static inline int ensure_value(csvx_t *cb) {
  if (cb->value.top >= cb->value.max) {
    DO(expand_value(cb));
  }
  return 0;
}

//////////////////
// squeeze or grow cb->buf[]
static int ensure_buf(csvx_t *cb) {
  int N = cb->buf.top - cb->buf.bot;
  // first, see if a squeeze is sufficient
  if (cb->buf.bot) {
    memmove(cb->buf.ptr, cb->buf.ptr + cb->buf.bot, N);
    cb->buf.bot = 0;
    cb->buf.top = N;
    return 0;
  }

  // grow buf[]
  if (cb->buf.max == cb->conf.maxbufsz) {
    return RETERROR(cb, "%s",
                    "max row size is larger than maxbufsz of %d bytes",
                    cb->conf.maxbufsz);
  }
  int64_t max = cb->buf.max;
  max = (0 == max ? cb->conf.initbufsz : max * 1.5);
  if (max > cb->conf.maxbufsz) {
    max = cb->conf.maxbufsz;
  }

  // 16-byte aligned for SIMD
  void *newbuf = aligned_alloc(16, max);
  if (!newbuf) {
    return RETERROR(cb, "%s", "out of memory");
  }

  memcpy(newbuf, cb->buf.ptr, N);
  free(cb->buf.ptr);
  cb->buf.ptr = (char *)newbuf;
  cb->buf.max = max;
  return 0;
}

///////////////
// fill cb->buf[]. Return 0 on success, -1 otherwise.
static int fill_buf(csvx_t *cb, void *context, csv_feed_t *feed) {
  assert(!cb->eof);
  DO(ensure_buf(cb));
  char *p = cb->buf.ptr + cb->buf.top;
  char *q = cb->buf.ptr + cb->buf.max;
  if (cb->fp) {
    // HACK: this is a hack for csv_parse_file.
    // If cb->fp is set, use this as context to call read_file().
    assert((void *)feed == (void *)read_file);
    context = cb->fp;
  }
  int N = q - p;
  // reserve 1 byte to add a \n if last row not terminated properly
  N = feed(context, p, N - 1, cb->ebuf.ptr, cb->ebuf.len);
  if (N < 0) {
    return -1;
  }
  cb->eof = (N == 0);
  cb->buf.top += N;

  // value of last byte in buf[]
  int finbyte = (cb->buf.bot < cb->buf.top ? cb->buf.ptr[cb->buf.top - 1] : 0);

  // if at EOF and last byte is not \n, then: add a newline
  if (cb->eof && finbyte && finbyte != '\n') {
    cb->buf.ptr[cb->buf.top++] = '\n';
  }
  return 0;
}

/*
  e: escape
  q: quote

                  +--------- q ---------+
                  |                     |
                  v                     |
[STARTVAL] ----> [UNQUOTED] --- q ---> [QUOTED] ----------+
   ^               |    \                  ^              |
   |               |     \                 |              |
   |             delim    \                +-- eq or ee --+
   |               |       \
   |               v       newline
   +----------- [ENDVAL]    |
   |                        |
   +------- [ENDROW]<-------+

*/
// Scan one row. Return 1 on success, 0 if there are not enough data
// to make a row, or -1 on error.  This call will corrupt cb->status
// on error.
static int onerow(scan_t *scan, csvx_t *cb) {
  const char esc = cb->conf.esc;
  const char qte = cb->conf.qte;
  const char delim = cb->conf.delim;

  // p points to start of value; pp points to the current special char
  const char *p = 0;
  const char *pp = 0;
  char ch; // char at *pp

  cb->value.top = 0;
  cb->status.rowno++;
  cb->status.lineno++;

STARTVAL:
  p = scan->p;
  if (p >= scan->q) {
    return 0;
  }

  csv_value_t value;
  memset(&value, 0, sizeof(value));
  value.ptr = (char *)p;
  goto UNQUOTED;

UNQUOTED:
  pp = scan_next(scan);
  ch = pp ? *pp : 0;
  // ch in [0, \n, delim, qte, or esc]
  if (ch == 0) {
    // out of data...
    assert(ch == 0);
    if (cb->eof) {
      return RETERROR(cb, "%s", "unterminated row");
    }
    return 0;
  }
  
  if (ch == qte) 
    goto QUOTED;
  if (ch == delim) 
    goto ENDVAL;
  if (ch == '\n') 
    goto ENDROW;
  if (ch == esc) 
    goto UNQUOTED; // esc in an unquoted field: ignore.

  assert(0);
  return RETERROR(cb, "%s", "internal error");

QUOTED:
  value.quoted = 1;
  pp = scan_next(scan);
  ch = pp ? *pp : 0;
  // ch in [0, \n, delim, qte, or esc]
  if (ch == 0) {
    // out of data...
    assert(ch == 0);
    if (cb->eof) {
      return RETERROR(cb, "%s", "unterminated quote");
    }
    return 0;
  }
  if (ch == '\n') {
    cb->status.lineno++;
    goto QUOTED;
  }
  if (ch == delim) {
    goto QUOTED;
  }

  // eq or ee: escape the next char
  if (ch == esc && (scan_match(scan, esc) || scan_match(scan, qte))) {
    (void) scan_next(scan);  // escaped
    goto QUOTED;
  }

  // q: exit QUOTED state
  if (ch == qte) 
    goto UNQUOTED; 

  // ex: continue in QUOTED state
  if (ch == esc) 
    goto QUOTED;

  assert(0);
  return RETERROR(cb, "%s", "internal error");

ENDVAL:
  // record the val
  value.len = pp - p;
  DO(ensure_value(cb));
  cb->value.ptr[cb->value.top++] = value;

  // start next val
  p = pp + 1;

  goto STARTVAL;

ENDROW:
  // handle \r\n
  value.len = pp - p;
  if (value.len && value.ptr[value.len - 1] == '\r') {
    value.len--;
  }

  // record the val
  DO(ensure_value(cb));
  cb->value.ptr[cb->value.top++] = value;

  return 1;
}

int csv_parse(csv_t *csv, void *context, csv_feed_t *feed,
              csv_perrow_t *perrow) {
  if (!csv->ok) {
    assert(csv->errmsg[0]);
    return -1;
  }
  csv->ok = false;
  csv->errmsg[0] = 0;

  csvx_t *cb = (csvx_t *)csv->__internal;
  cb->ebuf.ptr = csv->errmsg;
  cb->ebuf.len = sizeof(csv->errmsg);

  // Set up the scan on rows. Special chars are qte, esc, delim, and newline.
  char accept[5] = {0};
  {
    int i = 0;
    accept[i++] = cb->conf.qte;
    accept[i++] = cb->conf.delim;
    accept[i++] = '\n';
    accept[i++] = (cb->conf.qte != cb->conf.esc) ? cb->conf.esc : 0;
  }
  scan_t scan_row = scan_init(accept);

  // Set up the scan for unquote. Special chars are qte and esc only.
  {
    int i = 0;
    accept[i++] = cb->conf.qte;
    accept[i++] = (cb->conf.qte != cb->conf.esc) ? cb->conf.esc : 0;
    accept[i++] = 0;
  }
  scan_t scan_unquote = scan_init(accept);

  // keep scanning until EOF
  while (!finished(cb)) {
    int N;
    if (!cb->eof) {
      // Get more data from source
      if (fill_buf(cb, context, feed)) {
        goto bail;
      }
      assert(cb->buf.bot <= cb->buf.top);
    }

    // Set up a scan of the cb->buf[]
    scan_reset(&scan_row, cb->buf.ptr + cb->buf.bot, cb->buf.top - cb->buf.bot);
    assert(scan_row.p <= scan_row.q);

    // Scan buf[] row by row
    for (;;) {
      status_t saved_status = cb->status;
      const char *saved_p = scan_row.p;

      // Get one row
      N = onerow(&scan_row, cb);
      if (N < 0) {
        goto bail;
      }
      if (N == 0) {
        // Insufficient data in cb->buf[] to fill one row
        cb->status = saved_status; // rollback the status
        break;
      }
      assert(N == 1);

      // Advance the buffer
      cb->buf.bot += scan_row.p - saved_p;

      // Unquote the values
      if (cb->conf.unquote_values) {
        for (int i = 0; i < cb->value.top; i++) {
          unquote(&scan_unquote, &cb->value.ptr[i], &cb->conf);
        }
      }

      // Invoke the callback to process the current row
      if (perrow(context, cb->value.top, cb->value.ptr, cb->status.lineno,
                 cb->status.rowno, cb->ebuf.ptr, cb->ebuf.len)) {
        // Make up an error message if user did not supply one
        if (!cb->ebuf.ptr[0]) {
          RETERROR(cb, "%s", "perrow callback failed");
        }
        goto bail;
      }
    }
  }

  csv->ok = true;
  return 0;

bail:
  assert(csv->errmsg[0]);
  csv->ok = false;
  return -1;
}

csv_t csv_open(const csv_config_t* conf) {
  csv_t ret;
  memset(&ret, 0, sizeof(ret));
  csvx_t *cb = (csvx_t *)calloc(1, sizeof(*cb));
  if (!cb) {
    snprintf(ret.errmsg, sizeof(ret.errmsg), "%s", "out of memory");
    return ret;
  }
  ret.__internal = cb;

  cb->conf = conf ? *conf : csv_default_config();
  ret.ok = true;
  return ret;
}

void csv_close(csv_t *csv) {
  if (csv) {
    if (csv->__internal) {
      csvx_t *cb = (csvx_t *)csv->__internal;
      free(cb->buf.ptr);
      free(cb->value.ptr);
      if (cb->fp) {
        fclose(cb->fp);
      }
      free(csv->__internal);
      csv->__internal = NULL;
    }
  }
}

int csv_parse_file(csv_t *csv, FILE *fp, void *context, csv_perrow_t *perrow) {
  /* Note: we own fp now. Make sure it closed here or
   * in csv_close(). */
  if (!csv->ok) {
    assert(csv->errmsg[0]);
    fclose(fp);
    return -1;
  }
  csvx_t *cb = (csvx_t *)csv->__internal;
  cb->ebuf.ptr = csv->errmsg;
  cb->ebuf.len = sizeof(csv->errmsg);

  if (cb->fp) {
    fclose(cb->fp);
    cb->fp = 0;
  }

  cb->fp = fp;
  return csv_parse(csv, context, read_file, perrow);
}

int csv_parse_file_ex(csv_t *csv, const char *path, void *context,
                      csv_perrow_t *perrow) {
  FILE *fp = fopen(path, "r");
  if (!fp) {
    snprintf(csv->errmsg, sizeof(csv->errmsg), "fopen failed - %s",
             strerror(errno));
    csv->ok = false;
    return -1;
  }

  return csv_parse_file(csv, fp, context, perrow);
}

/*
  e: escape
  q: quote

                  +--------- q ---------+
                  |                     |
                  v                     |
[STARTVAL] ----> [UNQUOTED] --- q ---> [QUOTED] ----------+
                                           ^              |
                                           |              |
                                           +-- eq or ee --+
*/
/**
 *  Unquote a value and return a NUL-terminated string.
 *  This will modify memory area value.ptr[0 .. len+1].
 */
static void unquote(scan_t *scan, csv_value_t *value,
                    const csv_config_t *conf) {
  int qte = conf->qte;
  int esc = conf->esc;
  int nullsz = strlen(conf->nullstr);
  char *p = value->ptr;
  char *q = p + value->len;
  *q = 0;
  // p is now a NUL terminated string

  // if value is not quoted, just return it.
  if (!value->quoted) {
    // check for NULL
    if (value->len == nullsz &&
        0 == memcmp(value->ptr, conf->nullstr, nullsz)) {
      value->ptr = 0;
      value->len = 0;
    }
    return;
  }

  // fast path for "xxxx", where x != esc
  if (p[0] == qte && q[-1] == qte) {
    if (!memchr(p + 1, esc, q - p - 2)) {
      p++;
      *--q = 0;
      value->ptr = p;
      value->len = q - p;
      value->quoted = false;
      return;
    }
  }

  char *begin = p;
  char *pp;
  scan_reset(scan, p, q - p);
UNQUOTED:
  if (p == q) {
    goto DONE;
  }
  pp = (char *)scan_next(scan);
  if (!pp) {
    p = q;
    goto DONE;
  }
  // q
  if (*pp == qte) {
    // shift down, and go into QUOTED mode
    memmove(pp, pp + 1, q - pp);
    q--;
    assert(*q == 0);
    p = pp;
    scan_reset(scan, p, q - p);
    goto QUOTED;
  }
  // ignore this char
  p = pp + 1;
  goto UNQUOTED;

QUOTED:
  assert(p < q);
  pp = (char *)scan_next(scan);
  assert(pp);
  // eq or ee
  if (pp[0] == esc && (pp[1] == esc || pp[1] == qte)) {
    // shift down
    memmove(pp, pp + 1, q - pp);
    q--;
    p = pp + 1;
    scan_reset(scan, p, q - p);
    goto QUOTED;
  }
  // q
  if (*pp == qte) {
    memmove(pp, pp + 1, q - pp);
    q--;
    p = pp;
    scan_reset(scan, p, q - p);
    goto UNQUOTED;
  }
  // ignore this char
  p = pp + 1;
  goto QUOTED;

DONE:
  *p = 0;
  value->ptr = begin;
  value->len = p - begin;
  value->quoted = false;
  return;
}

csv_config_t csv_default_config(void) {
  csv_config_t conf;
  memset(&conf, 0, sizeof(conf));
  conf.unquote_values = true;
  conf.qte = '"';
  conf.esc = '"';
  conf.delim = ',';
  conf.initbufsz = 1024 * 4;          // 4KB
  conf.maxbufsz = 1024 * 1024 * 1024; // 1GB
  return conf;
}

// Read an int (without signs) from the string p. Return #bytes consumed, i.e.,
// 0 on failure.
static int read_int(const char *p, int *ret) {
  const char *pp = p;
  int val = 0;
  for (; isdigit(*p); p++) {
    val = val * 10 + (*p - '0');
    if (val < 0) {
      return 0; // overflowed
    }
  }
  *ret = val;
  return p - pp;
}

// Read a date from p[].  Return #bytes consumed, i.e., 0 on failure.
static int read_date(const char *p, int *year, int *month, int *day) {
  const char *pp = p;
  int n = read_int(p, year);
  if (n != 4 || p[4] != '-') {
    return 0;
  }
  n = read_int(p += 5, month);
  if (n != 2 || p[2] != '-') {
    return 0;
  }
  n = read_int(p += 3, day);
  if (n != 2) {
    return 0;
  }
  p += 2;
  return p - pp;
}

// Read a time as HH:MM:SS.subsec from p[]. Return #bytes consumed.
static int read_time(const char *p, int *hour, int *minute, int *second,
                     int *usec) {
  const char *pp = p;
  int n;
  *hour = *minute = *second = *usec = 0;
  n = read_int(p, hour);
  if (n != 2 || p[2] != ':') {
    return 0;
  }
  n = read_int(p += 3, minute);
  if (n != 2 || p[2] != ':') {
    return 0;
  }
  n = read_int(p += 3, second);
  if (n != 2) {
    return 0;
  }
  p += 2;
  if (*p != '.') {
    return p - pp;
  }
  p++; // skip the period
  int micro_factor = 100000;
  while (isdigit(*p) && micro_factor) {
    *usec += (*p - '0') * micro_factor;
    micro_factor /= 10;
    p++;
  }
  return p - pp;
}

// Reads a timezone from p[]. Return #bytes consumed.
static int read_tzone(const char *p, char *tzsign, int *tzhour, int *tzminute) {
  const char *pp = p;
  *tzhour = *tzminute = 0;
  *tzsign = '+';
  // look for Zulu
  if (*p == 'Z' || *p == 'z') {
    return 1;
  }

  *tzsign = *p++;
  if (!(*tzsign == '+' || *tzsign == '-')) {
    return 0;
  }

  // look for HH:MM
  int n;
  n = read_int(p, tzhour);
  if (n != 2 || p[2] != ':') {
    return 0;
  }
  n = read_int(p += 3, tzminute);
  if (n != 2) {
    return 0;
  }
  p += 2;
  return p - pp;
}

// Parse YYYY-MM-DD
int csv_parse_ymd(const char *s, int *year, int *month, int *day) {
  int n = read_date(s, year, month, day);
  if (!n || s[n]) {
    return -1;
  }
  return 0;
}

// Parse M/D/YYYY
int csv_parse_mdy(const char *s, int *year, int *month, int *day) {
  int n = read_int(s, month);
  if (!n || s[n] != '/') {
    return -1;
  }
  n = read_int(s += n + 1, day);
  if (!n || s[n] != '/') {
    return -1;
  }
  n = read_int(s += n + 1, year);
  if (!n || s[n]) {
    return -1;
  }
  return 0;
}

// Parse HH:MM:SS.subsec
int csv_parse_time(const char *s, int *hour, int *minute, int *second,
                   int *usec) {
  int n = read_time(s, hour, minute, second, usec);
  if (!n || s[n]) {
    return -1;
  }
  return 0;
}

// Parse date time
int csv_parse_timestamp(const char *s, int *year, int *month, int *day,
                        int *hour, int *minute, int *second, int *usec) {
  int n = read_date(s, year, month, day);
  if (!n || !(s[n] == ' ' || s[n] == 'T')) {
    return -1;
  }
  n = read_time(s += n + 1, hour, minute, second, usec);
  if (!n || s[n]) {
    return -1;
  }
  return 0;
}

// Parse date time tzone
int csv_parse_timestamptz(const char *s, int *year, int *month, int *day,
                          int *hour, int *minute, int *second, int *usec,
                          char *tzsign, int *tzhour, int *tzminute) {
  int n = read_date(s, year, month, day);
  if (!n || !(s[n] == ' ' || s[n] == 'T')) {
    return -1;
  }
  n = read_time(s += n + 1, hour, minute, second, usec);
  if (!n) {
    return -1;
  }
  n = read_tzone(s += n, tzsign, tzhour, tzminute);
  if (!n || s[n]) {
    return -1;
  }
  return 0;
}
