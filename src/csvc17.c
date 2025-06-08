#include "csvc17.h"
#include <assert.h>
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

typedef struct csvx_t csvx_t;
struct csvx_t {
  void *ctx;           // user context
  int qte, esc, delim; // special chars

  csv_feed_t *feed;     // feeder
  csv_perrow_t *perrow; // per-row callback
  ebuf_t ebuf;          // error buffer

  int lineno; // current line number
  int rowno;  // current row number
  int colno;  // current column number

  bool eof; // true if feed() signaled EOF

  struct {
    char *ptr; // buf of size max where [bot..top) is valid
    int bot, top, max;
  } buf;

  struct {
    csv_value_t *ptr;
    int top, max;
  } value;
};

/*
 *  Format an error into ebuf[]. Always return -1.
 */
static int RETERROR(csvx_t *cb, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char *p = cb->ebuf.ptr;
  char *q = p + cb->ebuf.len;
  snprintf(p, q - p, "(line %d, row %d, col %d)", cb->lineno, cb->rowno,
           cb->colno);
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
  max = max * 1.5 + 1024;
#else
  max = max + 1024;
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

static int run(csvx_t *cb) {
  int N;
  if (!cb->eof) {
    DO(ensure_buf(cb));
    char *p = cb->buf.ptr + cb->buf.top;
    char *q = cb->buf.ptr + cb->buf.max;
    N = cb->feed(cb->ctx, p, q - p);
    if (N < 0) {
      return -1;
    }
    cb->eof = (N == 0);
    cb->buf.top += N;
  }

  while (1) {
    // break if buffer is empty
    N = cb->buf.top - cb->buf.bot;
    if (N == 0) {
      return 0;
    }
    // get ready for a new value
    DO(ensure_value(cb));
  }

  return 0;
}

void csv_run(csv_t *csv) {
  csvx_t *cb = csv->__internal;
  cb->ebuf.ptr = csv->errmsg;
  cb->ebuf.len = sizeof(csv->errmsg);

  while (csv->ok && !(cb->eof && cb->buf.bot == cb->buf.top)) {
    csv->ok = (run(cb) == 0);
  }
}

csv_t csv_open(void *ctx, int qte, int esc, int delim, csv_feed_t *feed,
               csv_perrow_t *perrow) {
  csv_t ret = {0};
  csvx_t *cb = calloc(1, sizeof(*cb));
  if (!cb) {
    snprintf(ret.errmsg, sizeof(ret.errmsg), "%s", "out of memory");
    return ret;
  }
  ret.__internal = cb;

  cb->ctx = ctx;
  cb->qte = qte;
  cb->esc = esc;
  cb->delim = delim;
  cb->feed = feed;
  cb->perrow = perrow;

  ret.ok = true;
  return ret;
}

void csv_close(csv_t *csv) {
  if (csv) {
    csvx_t *cb = csv->__internal;
    if (cb) {
      free(cb->buf.ptr);
      free(cb->value.ptr);
    }
  }
}
