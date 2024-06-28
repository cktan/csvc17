#pragma once

typedef int csv_notify_t(void *context, int nfield, char **val, int *len);

typedef struct csv_status_t csv_status_t;
struct csv_status_t {
  int line;     // current line number
  int line_pos; // position of current line's first char
  int row;      // current row number
  int row_pos;  // position of current row's first char
  int pos;      // current char relative to byte 0
  char errmsg[200];
};

typedef struct csv_t csv_t;

csv_t *csv_open(void *context, int qte, int esc, int delim,
                csv_notify_t *notifyfn);
void csv_close(csv_t *csv);

/**
 *  Scan buf[] and call notify() for each row.
 *  Note that buf[] will be modified in-place.
 *  Returns 0 on success, -1 otherwise.
 *
 *  Note that each row, INCLUDING THE LAST ROW, must end with \n.
 */
int csv_feed(csv_t *csv, char *buf, int buflen, csv_status_t *status);
