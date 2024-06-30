#include "csv.h"
#include "scan.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

struct csv_t {
  scan_t scan;  // place here for alignment
  void *context;
  char qte, esc, delim;
  csv_notify_t *notifyfn;
  const char **val;
  int *len;
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

static int expand_val(csv_t *csv) {
  int max = csv->vmax;
#ifndef NDEBUG
  max = max * 1.5 + 10;
#else
  max = max + 1;
#endif

  const char **newval = realloc(csv->val, max * sizeof(*newval));
  if (!newval) {
    return -1;
  }
  csv->val = newval;

  int *newlen = realloc(csv->len, max * sizeof(*newlen));
  if (!newlen) {
    return -1;
  }
  csv->len = newlen;

  csv->vmax = max;
  assert(csv->vtop + 1 < csv->vmax);
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
  scan_t *const scan = &csv->scan;
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
  status->fldno = csv->vtop + 1;

STARTVAL:
  // reserve space for new val
  if (csv->vtop + 1 >= csv->vmax) {
    if (expand_val(csv)) {
      return perr(status, "out of memory");
    }
  }
  goto UNQUOTED;

UNQUOTED:
  pp = scan_pop(scan);
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
    if (esc == qte) {
      char ch1 = (pp + 1 == scan_peek(scan) ? pp[1] : 0);
      if (ch1 == qte) {
        // if qq: pop and continue in QUOTED state.
        pp = scan_pop(scan);
        goto QUOTED;
      }
      goto UNQUOTED;
    }
    // case when esc != qte
    if (ch == esc) {
      char ch1 = (pp + 1 == scan_peek(scan) ? pp[1] : 0);
      if (ch1 == qte || ch1 == esc) {
        // if eq or ee, pop and continue in QUOTED state.
        pp = scan_pop(scan);
        goto QUOTED;
      }
      if (esc != qte) {
        // ignore esc and continue in QUOTED state.
        goto QUOTED;
      }
    }
    assert(ch == qte);
    goto UNQUOTED;
  }

  // still in quote
  if (ch == '\n' || ch == delim) {
    goto QUOTED;
  }

  assert(ch == 0);
  return perr(status, "unterminated quote");

ENDVAL:
  // record the val
  csv->val[csv->vtop] = p;
  csv->len[csv->vtop] = pp - p;
  csv->vtop++;
  status->fldno = csv->vtop + 1;

  // start next val
  p = pp + 1;
  goto STARTVAL;

ENDROW:
  // record the val
  csv->val[csv->vtop] = p;
  csv->len[csv->vtop] = pp - p;
  csv->vtop++;
  status->fldno = csv->vtop + 1;
  scan->p++;
  return 0;
}

int csv_feed(csv_t *csv, const char *buf, int buflen, csv_status_t *status) {
  status->rowno = 0;
  status->rowpos = 0;
  status->fldno = 0;
  status->errmsg[0] = 0;

  scan_reset(&csv->scan, buf, buflen);

  while (0 == onerow(csv, status));

  return status->rowno > 1 ? status->rowpos : -1;
}

csv_t *csv_open(void *context, int qte, int esc, int delim,
                csv_notify_t *notifyfn) {
  csv_t *csv;

  if (posix_memalign((void**) &csv, 32, sizeof(*csv))) {
    return 0;
  }

  if (!esc) {
    esc = qte;
  }

  csv->context = context;
  csv->qte = qte;
  csv->esc = esc;
  csv->delim = delim;
  csv->notifyfn = notifyfn;

  scan_init(&csv->scan, qte, esc, delim);

  return csv;
}

void csv_close(csv_t *scan) { free(scan); }
