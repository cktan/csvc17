#include "csv.h"
#include "scan.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

struct csv_t {
  scan_t scan; // place here for alignment
  void *context;
  char qte, esc, delim;
  csv_notify_t *notifyfn;

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

    // ch is esc and esc != qte: always continue in QUOTED state
    // regardless of eq, ee, or ex.
    assert(ch == esc);
    char ch1 = (pp + 1 == scan_peek(scan) ? pp[1] : 0);
    if (ch1 == qte || ch1 == esc) {
      // if eq or ee, eat the escaped char.
      pp = scan_pop(scan);
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

int csv_feed(csv_t *csv, const char *buf, int buflen, csv_status_t *status) {
  status->rowno = 0;
  status->rowpos = 0;
  status->fldno = 0;
  status->fldpos = 0;
  status->errmsg[0] = 0;

  scan_reset(&csv->scan, buf, buflen);

  while (0 == onerow(csv, status)) {
    if (csv->notifyfn(csv->context, csv->vtop, csv->value)) {
      return perr(status, "notify failed");
    }
  }

  return status->rowno > 1 ? status->rowpos : -1;
}

csv_t *csv_open(void *context, int qte, int esc, int delim,
                csv_notify_t *notifyfn) {
  csv_t *csv;

  if (posix_memalign((void **)&csv, 32, sizeof(*csv))) {
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
