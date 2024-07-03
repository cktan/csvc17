#pragma once

#include <stdint.h>

typedef struct csv_t csv_t;

typedef struct csv_value_t csv_value_t;
struct csv_value_t {
  // NOTE: ptr is only valid inside notifyfn()
  const char *ptr; // points to start of value
  int len;         // len of value
  int quoted;      // flag if quoted
};

typedef int csv_notify_t(void *context, int nvalue, const csv_value_t value[],
                         csv_t *csv);

typedef struct csv_status_t csv_status_t;
struct csv_status_t {
  int fldno;      // field number in row starting at 1
  int fldpos;     // position of current field's first char
  int rowno;      // current row number
  int64_t rowpos; // position of current row's first char
  char errmsg[200];
};

typedef struct csv_t csv_t;

/**
 *  Open a csv scan. Returns NULL if out-of-memory.
 */
csv_t *csv_open(void *context, int qte, int esc, int delim,
                csv_notify_t *notifyfn);

/**
 *  Release resources used by csv.
 */
void csv_close(csv_t *csv);

/**
 *  Scan buf[] and call notify() for each row.
 *  Returns #bytes processed in buf[], or -1 on error.
 *
 *  Note that each row, INCLUDING THE LAST ROW, must end with \n.
 */
int64_t csv_feed(csv_t *csv, const char *buf, int64_t buflen,
                 csv_status_t *status);

/**
 *  Provided for the notify function to treat quoted
 *  values. Note that buf[] will be unquoted in-place.
 *  Returns 0 on success, -1 otherwise.
 */
int csv_unquote(csv_t *csv, char *buf, int buflen, char **val, int *vlen);

typedef struct csv_filescan_t csv_filescan_t;

csv_filescan_t *csv_filescan_open(const char *path, void *context, int qte,
                                  int esc, int delim, csv_notify_t *notifyfn,
                                  csv_status_t *status);

void csv_filescan_close(csv_filescan_t *fs);

int csv_filescan_run(csv_filescan_t *fs, csv_status_t *status);
