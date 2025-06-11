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

typedef struct status_t status_t;
struct status_t {
  int lineno; // current line number
  int rowno;  // current row number
  int colno;  // current column number
};

typedef struct scan_t scan_t;
struct scan_t {
  char *p;
  char *q;
  int qte, esc, delim; // special chars
  
};


typedef struct csvx_t csvx_t;
struct csvx_t {
  void *ctx;           // user context
  csv_feed_t *feed;     // feeder
  csv_perrow_t *perrow; // per-row callback
  ebuf_t ebuf;          // error buffer
  bool eof;  // true if feed() signaled EOF

  scan_t scan;
  status_t status;

  struct {
    char *ptr; // buf of size max where [bot..top) is valid
    int bot, top, max;
    int finbyte; // value of ptr[top-1]
  } buf;

  struct {
    csv_value_t *ptr;
    int top, max;
  } value;
};

// True if EOF and buffer is empty
static inline bool finished(const csvx_t* cb) {
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

/*
 *  Format an error into ebuf[]. Always return -1.
 */
static int RETERROR(ebuf_t ebuf, const status_t* status, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char *p = ebuf.ptr;
  char *q = p + ebuf.len;
  snprintf(p, q - p, "(line %d, row %d, col %d)", status->lineno, status->rowno,
           status->colno);
  p += strlen(p);
  vsnprintf(p, q - p, fmt, args);
  return -1;
}

//////////////////
// grow cb->value[]
static int expand_value(csvx_t *cb, const status_t* status) {
  int max = cb->value.max;
#ifndef NDEBUG
  max = max * 1.5 + 10;
#else
  max = max + 1;
#endif
  if (max < 0) {
    return RETERROR(cb->ebuf, status, "%s", "buffer overflow");
  }

  csv_value_t *newval = realloc(cb->value.ptr, max * sizeof(*newval));
  if (!newval) {
    return RETERROR(cb->ebuf, status, "%s", "out of memory");
  }
  cb->value.ptr = newval;
  cb->value.max = max;
  assert(cb->value.top < cb->value.max);
  return 0;
}

//////////////////
// make sure csv->value[] can accomodate at least one value
static inline int ensure_value(csvx_t *cb, const status_t* status) {
  if (cb->value.top >= cb->value.max) {
    DO(expand_value(cb, status));
  }
  return 0;
}

//////////////////
// squeeze or grow cb->buf[]
static int ensure_buf(csvx_t *cb, const status_t* status) {
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
    return RETERROR(cb->ebuf, status, "%s", "buffer overflow");
  }

  // 16-byte aligned for SIMD
  void *newbuf = aligned_alloc(16, max);
  if (!newbuf) {
    return RETERROR(cb->ebuf, status, "%s", "out of memory");
  }

  memcpy(newbuf, cb->buf.ptr, N);
  free(cb->buf.ptr);
  cb->buf.ptr = newbuf;
  cb->buf.max = max;
  return 0;
}


// fill cb->buf[]. Return 0 on success, -1 otherwise.
static int fill_buf(csvx_t* cb, const status_t* status) {
  assert(!cb->eof);
  DO(ensure_buf(cb, status));
  char *p = cb->buf.ptr + cb->buf.top;
  char *q = cb->buf.ptr + cb->buf.max;
  int N = q - p;
  // reserve 1 byte to add a \n if last row not terminated properly
  N = cb->feed(cb->ctx, p, N - 1, cb->ebuf.ptr, cb->ebuf.len);
  if (N < 0) {
    return -1;
  }
  cb->eof = (N == 0);
  cb->buf.top += N;
  cb->buf.finbyte =
    (cb->buf.bot < cb->buf.top ? cb->buf.ptr[cb->buf.top - 1] : 0);
  if (cb->eof && cb->buf.finbyte != '\n') {
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
// Scan one row. Return #bytes consumed, 0 if no row, -1 on error.
static int onerow(scan_t* scan) {
  return 0;
}


void csv_parse(csv_t *csv) {
  csvx_t *cb = csv->__internal;
  cb->ebuf.ptr = csv->errmsg;
  cb->ebuf.len = sizeof(csv->errmsg);
  csv->ok = false;

  scan_t scan = {0};
  while (!finished(cb)) {
    status_t status = cb->status;
    int N;
    if (!cb->eof) {
      if (fill_buf(cb, &status)) {
	return;
      }
      scan.p = cb->buf.ptr + cb->buf.bot;
      scan.q = cb->buf.ptr + cb->buf.top;
    }
    
    for (;;) {
      N = onerow(&scan);
      if (N < 0) {
	return;
      }
      if (N == 0) {
	// data in cb->buf[] is not enough to fill one row
	break;
      }
      cb->buf.bot += N;
    }
  }

  csv->ok = true;
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
  cb->feed = feed;
  cb->perrow = perrow;
  cb->scan.qte = qte;
  cb->scan.esc = esc;
  cb->scan.delim = delim;
  
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
