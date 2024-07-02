#pragma once

typedef struct csv_value_t csv_value_t;
struct csv_value_t {
  const char *ptr; // points into input
  int len;
  int quoted; // flag if quoted
};

typedef int csv_notify_t(void *context, int nvalue, const csv_value_t value[]);

typedef struct csv_status_t csv_status_t;
struct csv_status_t {
  int rowno;  // current row number
  int rowpos; // position of current row's first char
  int fldno;  // field number starting at 1
  int fldpos; // position of current field's first char
  char errmsg[200];
};

typedef struct csv_t csv_t;

csv_t *csv_open(void *context, int qte, int esc, int delim,
                csv_notify_t *notifyfn);
void csv_close(csv_t *csv);

/**
 *  Scan buf[] and call notify() for each row.
 *  Returns #bytes processed in buf[], or -1 on error.
 *
 *  Note that each row, INCLUDING THE LAST ROW, must end with \n.
 */
int csv_feed(csv_t *csv, const char *buf, int buflen, csv_status_t *status);
