#include "csv.h"
#include "scan.h"
#include "unquote.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

struct csv_t {
  void *context;
  char qte, esc, delim;
  csv_notify_t *notifyfn;

  scan_t *scan;   // used by onerow()
  unquote_t *unq; // used by csv_unquote()

  csv_value_t *value;
  int vtop;
  int vmax;
};

static int perr(csv_status_t *status, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf(status->errmsg, sizeof(status->errmsg), fmt, args);
  va_end(args);
  return -1;
}

// grow csv->value[]
static int expand_value(csv_t *csv) {
  int max = csv->vmax;
#ifndef NDEBUG
  max = max * 1.5 + 10;
#else
  max = max + 1;
#endif

  csv_value_t *newval = realloc(csv->value, max * sizeof(*newval));
  if (!newval) {
    return -1;
  }
  csv->value = newval;
  csv->vmax = max;
  assert(csv->vtop + 1 < csv->vmax);
  return 0;
}

// make sure csv->value[] can accomodate at least one value
static inline int ensure_value(csv_t *csv, csv_status_t *status) {
  if (csv->vtop + 1 >= csv->vmax) {
    if (expand_value(csv)) {
      return perr(status, "out of memory");
    }
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

static int onerow(csv_t *csv, csv_status_t *status) {
  scan_t *const scan = csv->scan;
  const char esc = csv->esc;
  const char qte = csv->qte;
  const char delim = csv->delim;

  // p points to start of value; pp points to the current special char
  const char *p = scan->p;
  const char *pp = 0;
  char ch; // char at *pp

  csv->vtop = 0;
  status->rowno++;
  status->rowpos = p - scan->orig;
  status->fldno = 0;
  status->fldpos = 0;

  csv_value_t *value;

STARTVAL:
  if (ensure_value(csv, status)) {
    return -1;
  }
  status->fldno++;
  status->fldpos = p - scan->orig;
  value = &csv->value[csv->vtop++];
  value->ptr = p;
  value->len = 0;
  value->quoted = 0;
  goto UNQUOTED;

UNQUOTED:
  pp = scan_pop(scan);
  // ch in [0, \n, delim, qte, or esc]
  ch = pp ? *pp : 0;
  if (ch == qte) {
    value->quoted = 1;
    goto QUOTED;
  }
  if (ch == delim) {
    goto ENDVAL;
  }
  if (ch == '\n') {
    goto ENDROW;
  }
  if (ch == esc) {
    goto UNQUOTED;
  }
  assert(ch == 0);
  return perr(status, "unterminated row");

QUOTED:
  pp = scan_pop(scan);
  // ch in [0, \n, delim, qte, or esc]
  ch = pp ? *pp : 0;

  if (ch == qte || ch == esc) {
    // handle the normal case that esc == qte.
    if (qte == esc) {
      char ch1 = (pp + 1 == scan_peek(scan) ? pp[1] : 0);
      if (ch1 == qte) {
        // if qq: pop and continue in QUOTED state.
        pp = scan_pop(scan);
        goto QUOTED;
      }
      // if qx: just exited quote. continue in UNQUOTED state.
      goto UNQUOTED;
    }

    // case when esc != qte

    if (ch == qte) {
      // the quote was not escaped, so we exit QUOTED state.
      goto UNQUOTED;
    }

    assert(ch == esc);
    // here, ch is esc and we have either eq, ee, or ex.
    // for eq or ee, escape one char and continue in QUOTED state.
    // for ex, do nothing and continue in QUOTED state.
    char ch1 = (pp + 1 == scan_peek(scan) ? pp[1] : 0);
    if (ch1 == qte || ch1 == esc) {
      // for eq or ee, eat the escaped char.
      pp = scan_pop(scan);
      goto QUOTED;
    }
    goto QUOTED;
  }

  if (!ch) {
    return perr(status, "unterminated quote");
  }

  // still in quote
  assert(ch == '\n' || ch == delim);
  goto QUOTED;

ENDVAL:
  // record the val
  value->len = pp - p;

  // start next val
  p = pp + 1;

  goto STARTVAL;

ENDROW:
  // record the val
  value->len = pp - p;

  // handle \r\n
  if (value->len && value->ptr[value->len - 1] == '\r') {
    value->len--;
  }

  // eat the newline
  scan->p++;
  return 0;
}

int64_t csv_feed(csv_t *csv, const char *buf, int64_t buflen,
                 csv_status_t *status) {
  if (buflen <= 0) {
    if (buflen < 0) {
      return perr(status, "negative buflen");
    }
    return 0;
  }

  status->rowno = 0;
  status->rowpos = 0;
  status->fldno = 0;
  status->fldpos = 0;
  status->errmsg[0] = 0;

  scan_reset(csv->scan, buf, buflen);

  while (0 == onerow(csv, status)) {
    if (csv->notifyfn(csv->context, csv->vtop, csv->value, csv)) {
      return perr(status, "notify failed");
    }
  }

  if (status->rowno <= 1) {
    return perr(status, "unterminated row");
  }

  return status->rowpos;
}

csv_t *csv_open(void *context, int qte, int esc, int delim,
                csv_notify_t *notifyfn) {
  csv_t *csv;
  csv = calloc(1, sizeof(*csv));
  if (!csv) {
    return NULL;
  }

  if (posix_memalign((void **)&csv->scan, 32, sizeof(*csv->scan))) {
    csv_close(csv);
    return NULL;
  }

  if (posix_memalign((void **)&csv->unq, 32, sizeof(*csv->unq))) {
    csv_close(csv);
    return NULL;
  }

  if (!esc) {
    esc = qte;
  }

  csv->context = context;
  csv->qte = qte;
  csv->esc = esc;
  csv->delim = delim;
  csv->notifyfn = notifyfn;

  scan_init(csv->scan, qte, esc, delim);
  unquote_init(csv->unq, qte, esc);

  return csv;
}

void csv_close(csv_t *csv) {
  if (csv) {
    free(csv->scan);
    free(csv->unq);
    free(csv->value);
    free(csv);
  }
}

static inline int move(char *dest, char *src, int len) {
  if (dest != src) {
    memmove(dest, src, len);
  }
  return len;
}

int csv_unquote(csv_t *csv, char *buf, int buflen, char **val, int *vlen) {
  unquote_t *unq = csv->unq;
  const char esc = csv->esc;
  const char qte = csv->qte;
  char *dest = buf;
  char *p = buf;
  char *q = buf + buflen;
  char *pp;
  char ch; // char at *pp
  *val = dest;

  /* fast path for buf is "xxxxx". this will avoid any copying for
   * the case when there is nothing special inside the quote.
   */
  if (buflen >= 2 && buf[0] == '"' && buf[buflen - 1] == '"') {
    buf++;
    buflen--;
    unquote_reset(unq, buf, buflen);
    p++;
    dest++;
    *val = dest;
    goto QUOTED;
  }

  unquote_reset(unq, buf, buflen);

UNQUOTED:
  pp = unquote_pop(unq);
  ch = pp ? *pp : 0;
  if (ch == qte) {
    dest += move(dest, p, pp - p);
    p = pp + 1;
    goto QUOTED;
  }
  dest += move(dest, p, q - p);
  p = q;
  goto DONE;

QUOTED:
  pp = unquote_pop(unq);
  ch = pp ? *pp : 0;
  if (!ch) {
    return -1; // unterminated quote
  }

  // copy all before qte or esc
  dest += move(dest, p, pp - p);
  p = pp;

  assert(ch == qte || ch == esc);

  if (qte == esc) {
    assert(ch == qte);
    char ch1 = (pp + 1 == unquote_peek(unq) ? pp[1] : 0);
    // ch == qte && ch1 in [0, qte].
    if (ch1 == qte) {
      // if qq: pop and continue in QUOTED state.
      pp = unquote_pop(unq);
      *dest++ = '"'; // escaped a qte
      p = pp + 1;
      goto QUOTED;
    }
    assert(ch1 == 0);
    p++; // move past the end-quote.
    goto UNQUOTED;
  }

  // case esc != qte
  // ch in [qte, esc]

  if (ch == qte) {
    // the quote was not escaped, so we exit QUOTED state.
    p++; // move past the end-quote.
    goto UNQUOTED;
  }

  // here, ch is esc and we have either eq, ee, or ex.
  // for eq or ee, escape one char and continue in QUOTED state.
  // for ex, do nothing and continue in QUOTED state.

  assert(ch == esc);
  char ch1 = (pp + 1 == unquote_peek(unq) ? pp[1] : 0);
  if (ch1 == qte || ch1 == esc) {
    // if eq or ee, eat the escaped char.
    pp = unquote_pop(unq);
    *dest++ = ch1;
    p = pp + 1;
    goto QUOTED;
  }

  // nothing was escaped. continue in QUOTED state.
  goto QUOTED;

DONE:
  *vlen = dest - *val;
  return 0;
}

struct csv_filescan_t {
  csv_t *csv;
  int fd;
  const char *data;
  int64_t datalen;
  char *tmp;
};

csv_filescan_t *csv_filescan_open(const char *path, void *context, int qte,
                                  int esc, int delim, csv_notify_t *notifyfn,
                                  csv_status_t *status) {

  memset(status, 0, sizeof(*status));

  csv_filescan_t *fs = calloc(1, sizeof(*fs));
  if (!fs) {
    perr(status, "out of memory");
    goto bail;
  }
  fs->fd = -1;

  // Open the file
  fs->fd = open(path, O_RDONLY);
  if (fs->fd == -1) {
    perr(status, "cannot open file: %s", strerror(errno));
    goto bail;
  }

  // Get the file size
  struct stat st;
  if (fstat(fs->fd, &st) == -1) {
    perr(status, "cannot stat file: %s", strerror(errno));
    goto bail;
  }

  fs->datalen = st.st_size;

  // Memory-map the file
  fs->data = mmap(NULL, fs->datalen, PROT_READ, MAP_PRIVATE, fs->fd, 0);
  if (fs->data == MAP_FAILED) {
    perr(status, "mmap failed: %s", strerror(errno));
    goto bail;
  }

  // Provide a hint to the system that the memory will be accessed sequentially
  if (madvise((void *)fs->data, fs->datalen, MADV_SEQUENTIAL) == -1) {
    perr(status, "madvice failed: %s", strerror(errno));
    goto bail;
  }

  // Close the file descriptor; the mapping remains valid until munmap()
  close(fs->fd);
  fs->fd = -1;

  fs->csv = csv_open(context, qte, esc, delim, notifyfn);
  if (!fs->csv) {
    perr(status, "out of memory");
    goto bail;
  }

  return fs;

bail:
  csv_filescan_close(fs);
  return NULL;
}

void csv_filescan_close(csv_filescan_t *fs) {
  if (!fs) {
    return;
  }

  if (fs->fd >= 0) {
    close(fs->fd);
  }

  if (fs->data) {
    munmap((void *)fs->data, fs->datalen);
  }

  csv_close(fs->csv);
  free(fs->tmp);
  free(fs);
}

int csv_filescan_run(csv_filescan_t *fs, csv_status_t *status) {
  int64_t n = csv_feed(fs->csv, fs->data, fs->datalen, status);
  if (n < 0) {
    return -1;
  }
  if (n < fs->datalen) {
    // append a '\n' to remainder and retry
    int64_t remainder = fs->datalen - n;
    if (fs->tmp) {
      free(fs->tmp);
      fs->tmp = 0;
    }
    fs->tmp = malloc(remainder + 1);
    if (!fs->tmp) {
      return perr(status, "out of memory");
    }
    memcpy(fs->tmp, fs->data + n, remainder);
    fs->tmp[remainder] = '\n';
    int m = csv_feed(fs->csv, fs->tmp, remainder + 1, status);
    if (m < 0) {
      return -1;
    }
    n += m - 1;
  }
  assert(n == fs->datalen);
  if (n != fs->datalen) {
    return perr(status, "internal error");
  }
  return 0;
}
