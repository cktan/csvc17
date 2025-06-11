#ifndef CSVC17_H
#define CSVC17_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
#define CSV_EXTERN extern "C"
#else
#define CSV_EXTERN extern
#endif

typedef struct csv_t csv_t;
struct csv_t {
  bool ok;
  void *__internal;
  char errmsg[200];
};

typedef struct csv_value_t csv_value_t;
struct csv_value_t {
  const char *ptr;
  int len;
  bool quoted;
};

/**
 *  This is a callback that is invoked when the parser needs data.
 *  Return #bytes copied into buf on success, 0 on EOF, -1 on error.
 */
typedef int csv_feed_t(void *context, char *buf, int bufsz, char *errbuf,
                       int errsz);

/**
 *  This is a callback that is invoked per row.
 *  Return 0 on success, -1 otherwise.
 */
typedef int csv_perrow_t(void *context, int n, const csv_value_t value[],
                         char *errbuf, int errsz);

/**
 *  Open a scan. The csv_t handle returned must be freed using csv_close() after
 * use.
 *
 *  Note: this is a 3-part open(), parse(), close() interface to cater to C++
 * exceptions (or longjmp) being thrown from inside the callback routines during
 * parse() operation.
 */
CSV_EXTERN csv_t csv_open(void *context, int qte, int esc, int delim,
                          csv_feed_t *feed, csv_perrow_t *perrrow);

/**
 *  Run the scan and invoke callbacks on demand.
 *  Check csv->ok for success or failure.
 */
CSV_EXTERN void csv_parse(csv_t *csv);

/**
 *  Close the scan and release resources.
 */
CSV_EXTERN void csv_close(csv_t *csv);

#endif
