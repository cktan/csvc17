#ifndef CSVC17_H
#define CSVC17_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
#define CSV_EXTERN extern "C"
#else
#define CSV_EXTERN extern
#endif


typedef struct csv_t csv_t;
struct csv_t {
  bool ok;
  void* __internal;
  char errmsg[200];
};


typedef struct csv_value_t csv_value_t;
struct csv_value_t {
  const char* ptr;
  int len;
  bool quoted;
};

/**
 *  This is a callback that is invoked when the parser needs data.
 *  Return #bytes copied into buf on success, 0 on EOF, -1 on error.
 */
typedef int csv_feed_t(void* context, char* buf, int bufsz);

/**
 *  This is a callback that is invoked per row.
 *  Return 0 on success, -1 otherwise.
 */
typedef int csv_perrow_t(void* context, int n, const csv_value_t value[]);


/**
 *  Open a scan. The csv_t handle returned must be freed using csv_close() after use.
 */
CSV_EXTERN csv_t csv_open(void* context, int qte, int esc, int delim,
			  csv_feed_t* feed, csv_perrow_t* perrrow);

/**
 *  Run the scan and invoke callbacks on demand.
 *  Return 0 on success, -1 otherwise.
 */
CSV_EXTERN int csv_run(csv_t* csv);


/**
 *  Close the scan and release resources.
 */
CSV_EXTERN void csv_close(csv_t* csv);



#endif
