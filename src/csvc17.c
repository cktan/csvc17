/* Copyright (c) 2024-2025, CK Tan.
 * https://github.com/cktan/csvc17/blob/main/LICENSE
 */
#include "csvc17.h"
#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DO(x)                                                                  \
  if (x)                                                                       \
    return -1;                                                                 \
  else                                                                         \
    (void)0

typedef struct ebuf_t ebuf_t;
struct ebuf_t {
  char *ptr;
  int len;
};

typedef struct status_t status_t;
struct status_t {
  int64_t lineno; // current line number
  int64_t rowno;  // current row number
  // note: current column number is (csvx_t::value.top + 1)
};

typedef struct scan_t scan_t;
struct scan_t {
  char *p;
  char *q;
  int qte, esc, delim; // special chars
};

typedef struct csvx_t csvx_t;
struct csvx_t {
  ebuf_t ebuf;         // error buffer
  int qte, esc, delim; // special chars
  bool eof;            // true if feed() signaled EOF

  status_t status;

  struct {
    char *ptr; // buf of size max where [bot..top) is valid
    int bot, top, max;
  } buf;

  struct {
    csv_value_t *ptr;
    int top, max;
  } value;

  FILE *fp; // file ptr if any
};

static scan_t scan_reset(const csvx_t *cb) {
  scan_t scan = {0};
  scan.qte = cb->qte;
  scan.esc = cb->esc;
  scan.delim = cb->delim;
  scan.p = cb->buf.ptr + cb->buf.bot;
  scan.q = cb->buf.ptr + cb->buf.top;
  return scan;
}

// True if EOF and buffer is empty
static inline bool finished(const csvx_t *cb) {
  return cb->eof && (cb->buf.bot == cb->buf.top);
}

/* get ptr to the next special char */
static char *scan_next(scan_t *sc) {
  for (; sc->p < sc->q; sc->p++) {
    int ch = *sc->p;
    if (ch == sc->delim || ch == '\n' || ch == sc->qte || ch == sc->esc) {
      return sc->p++;
    }
  }
  return NULL;
}

/* get ptr to the current char */
static char *scan_peek(scan_t *sc) { return (sc->p < sc->q) ? sc->p : NULL; }

static int read_file(void *context, char *buf, int bufsz, char *errmsg,
                     int errsz) {
  FILE *fp = context;
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

  csv_value_t *newval = realloc(cb->value.ptr, max * sizeof(*newval));
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
  int max = cb->buf.max;
#ifndef NDEBUG
  max = max * 1.5 + 2048;
#else
  max = max + 128;
#endif
  if (max < 0) {
    return RETERROR(cb, "%s", "buffer overflow");
  }

  // 16-byte aligned for SIMD
  void *newbuf = aligned_alloc(16, max);
  if (!newbuf) {
    return RETERROR(cb, "%s", "out of memory");
  }

  memcpy(newbuf, cb->buf.ptr, N);
  free(cb->buf.ptr);
  cb->buf.ptr = newbuf;
  cb->buf.max = max;
  return 0;
}

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
   +------- [ENDROW] -------+

*/
// Scan one row. Return #rows scanned, or -1 on error.
// This call will corrupt cb->status on error.
static int onerow(scan_t *scan, csvx_t *cb) {
  const char esc = cb->esc;
  const char qte = cb->qte;
  const char delim = cb->delim;

  // p points to start of value; pp points to the current special char
  const char *p = scan->p;
  const char *pp = 0;
  char ch; // char at *pp

  if (scan->p == scan->q) {
    return 0;
  }

  cb->value.top = 0;
  cb->status.rowno++;
  cb->status.lineno++;

STARTVAL:
  csv_value_t value = {0};
  value.ptr = (char *)p;
  goto UNQUOTED;

UNQUOTED:
  pp = scan_next(scan);
  // ch in [0, \n, delim, qte, or esc]
  ch = pp ? *pp : 0;
  if (ch == qte) {
    goto QUOTED;
  }
  if (ch == delim) {
    goto ENDVAL;
  }
  if (ch == '\n') {
    goto ENDROW;
  }
  if (ch == esc) {
    goto UNQUOTED; // esc in an unquoted field: ignore.
  }
  assert(ch == 0);
  if (cb->eof) {
    return RETERROR(cb, "%s", "unterminated row");
  }
  return 0;

QUOTED:
  value.quoted = 1;
  pp = scan_next(scan);
  // ch in [0, \n, delim, qte, or esc]
  ch = pp ? *pp : 0;

  if (ch == qte || ch == esc) {

    // CASE WHEN esc == qte
    if (qte == esc) {
      pp = scan_peek(scan);
      // If two quotes: pop and continue in QUOTED state.
      if (pp && *pp == qte) {
        pp = scan_next(scan);
        goto QUOTED;
      }
      // If one quote: exit QUOTED state, continue in UNQUOTED state.
      goto UNQUOTED;
    }

    // CASE WHEN esc != qte
    if (ch == qte) {
      // the quote was not escaped, so we exit QUOTED state.
      goto UNQUOTED;
    }

    assert(ch == esc);
    // here, ch is esc and we have either eq, ee, or ex.
    // for eq or ee, escape one char and continue in QUOTED state.
    // for ex, do nothing and continue in QUOTED state.
    pp = scan_peek(scan);
    if (pp && (*pp == qte || *pp == esc)) {
      // for eq or ee, eat the escaped char.
      pp = scan_next(scan);
      goto QUOTED;
    }
    goto QUOTED;
  }

  if (!ch) {
    if (cb->eof) {
      return RETERROR(cb, "%s", "unterminated quote");
    }
    return 0;
  }

  // still in quote
  if (ch == '\n') {
    cb->status.lineno++;
  }
  assert(ch == '\n' || ch == delim);
  goto QUOTED;

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

csv_t *csv_parse(csv_t *csv, void *context, csv_feed_t *feed,
                 csv_perrow_t *perrow) {
  if (!csv->ok) {
    assert(csv->errmsg[0]);
    return csv;
  }
  csv->ok = false;
  csv->errmsg[0] = 0;

  csvx_t *cb = csv->__internal;
  cb->ebuf.ptr = csv->errmsg;
  cb->ebuf.len = sizeof(csv->errmsg);

  while (!finished(cb)) {
    int N;
    if (!cb->eof) {
      if (fill_buf(cb, context, feed)) {
        assert(csv->errmsg[0]);
        return csv;
      }
      assert(cb->buf.bot <= cb->buf.top);
    }

    // set up a scan of the cb->buf[]
    scan_t scan = scan_reset(cb);
    assert(scan.p <= scan.q);

    // Scan row by row
    for (;;) {
      status_t saved_status = cb->status;
      char *saved_p = scan.p;
      N = onerow(&scan, cb);
      if (N < 0) {
        assert(csv->errmsg[0]);
        return csv;
      }
      if (N == 0) {
        // data in cb->buf[] is not enough to fill one row
        cb->status = saved_status; // rollback the status
        break;
      }
      assert(N == 1);
      cb->buf.bot += scan.p - saved_p;

      if (perrow(context, cb->value.top, cb->value.ptr, cb->status.lineno,
                 cb->status.rowno, cb->ebuf.ptr, cb->ebuf.len)) {
        if (!cb->ebuf.ptr[0]) {
          RETERROR(cb, "%s", "perrow callback failed");
        }
        return csv;
      }
    }
  }

  csv->ok = true;
  return csv;
}

csv_t csv_open(int qte, int esc, int delim) {
  csv_t ret = {0};
  csvx_t *cb = calloc(1, sizeof(*cb));
  if (!cb) {
    snprintf(ret.errmsg, sizeof(ret.errmsg), "%s", "out of memory");
    return ret;
  }
  ret.__internal = cb;

  cb->qte = qte;
  cb->esc = esc;
  cb->delim = delim;

  ret.ok = true;
  return ret;
}

void csv_close(csv_t *csv) {
  if (csv) {
    csvx_t *cb = csv->__internal;
    if (cb) {
      free(cb->buf.ptr);
      free(cb->value.ptr);
      if (cb->fp) {
        fclose(cb->fp);
      }
      free(cb);
    }
  }
}

csv_t *csv_parse_file(csv_t *csv, FILE *fp, void *context,
                      csv_perrow_t *perrow) {
  if (!csv->ok) {
    assert(csv->errmsg[0]);
    return csv;
  }
  csvx_t *cb = csv->__internal;
  cb->ebuf.ptr = csv->errmsg;
  cb->ebuf.len = sizeof(csv->errmsg);

  if (cb->fp) {
    RETERROR(cb, "%s", "busy file handle");
    return csv;
  }

  cb->fp = fp;
  return csv_parse(csv, context, read_file, perrow);
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
 */
CSV_EXTERN char *csv_unquote(csv_value_t value, int qte, int esc) {
  char *p = value.ptr;
  char *q = p + value.len;
  *q = 0;

  // if value is not quoted, just return it.
  if (!value.quoted) {
    return p;
  }

  // fast path for "xxxx", where x != esc
  if (p[0] == qte && q[-1] == qte) {
    if (!memchr(p + 1, esc, q - p - 2)) {
      q[-1] = 0;
      return p + 1;
    }
  }

  char *begin = p;
UNQUOTED:
  if (p == q) {
    goto DONE;
  }
  if (*p != qte) {
    goto UNQUOTED;
  }
  // shift down
  memmove(p, p + 1, q - p);
  q--;
QUOTED:
  assert(p < q);
  if (p + 1 < q && p[0] == esc && (p[1] == esc || p[1] == qte)) {
    // shift down
    memmove(p, p + 1, q - p);
    q--;
    p++;
    goto QUOTED;
  }
  if (*p == qte) {
    memmove(p, p + 1, q - p);
    q--;
    goto UNQUOTED;
  }
  goto QUOTED;

DONE:
  *p = 0;
  return begin;
}
