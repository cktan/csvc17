#include "csv.h"
#include "scan.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

struct csv_t {
  void *context;
  char qte, esc, delim;
  csv_notify_t *notifyfn;
  char **val;
  int *len;
  int vtop;
  int vmax;
  scan_t scan;
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

  char **newval = realloc(csv->val, max * sizeof(*newval));
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
   +------ [ENDLINE] -------+

*/

static int onerow(csv_t *csv, csv_status_t *status) {
  scan_t *const scan = &csv->scan;
  const char esc = csv->esc;
  const char qte = csv->qte;
  const char delim = csv->delim;
  status->pos = 0;

  // p points to start of value; pp points to the current special char
  char *p = scan->p;
  char *pp = 0;
  char ch; // char at *pp

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
    goto ENDLINE;
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

  if (ch == '\n') {
    status->line++;
    status->line_pos = pp + 1 - scan->orig;
    goto QUOTED;
  }
  if (ch == delim) {
    goto QUOTED;
  }

  assert(ch == 0);
  return perr(status, "unterminated quote");

ENDVAL:
  // record the val
  csv->val[csv->vtop] = p;
  csv->len[csv->vtop] = pp - p;
  csv->vtop++;

  // start next val
  p = pp + 1;
  status->pos = p - scan->orig;
  goto STARTVAL;

ENDLINE:
  // record the val
  csv->val[csv->vtop] = p;
  csv->len[csv->vtop] = pp - p;
  csv->vtop++;

  status->line++;
  status->line_pos = pp + 1 - scan->orig;
  status->row++;
  status->row_pos = pp + 1 - scan->orig;
  return 0;
}

int csv_feed(csv_t *csv, char *buf, int buflen, csv_status_t *status) {
  status->line = 1;
  status->line_pos = 0;
  status->row = 1;
  status->row_pos = 0;
  status->pos = 0;
  status->errmsg[0] = 0;

  scan_t *scan = &csv->scan;
  scan_reset(&csv->scan, buf, buflen);

  while (scan->p < scan->q) {
    if (onerow(csv, status)) {
      status->pos = scan->prev_p - scan->orig;
      return -1;
    }
  }

  return 0;
}

csv_t *csv_open(void *context, int qte, int esc, int delim,
                csv_notify_t *notifyfn) {
  csv_t *csv = calloc(1, sizeof(*csv));
  if (!csv) {
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
