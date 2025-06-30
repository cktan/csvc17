/* Copyright (c) 2024-2025, CK Tan.
 * https://github.com/cktan/csvc17/blob/main/LICENSE
 */
#ifndef CSVC17_H
#define CSVC17_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
#define CSV_EXTERN extern "C"
#else
#define CSV_EXTERN extern
#endif

typedef struct csv_config_t csv_config_t;
struct csv_config_t {
  int qte;       /* default double-quote */
  int esc;       /* default double-quote */
  int delim;     /* default comma */
  int initbufsz; /* default 4KB */
  int maxbufsz;  /* default 1GB */
};

typedef struct csv_t csv_t;
struct csv_t {
  bool ok;
  void *__internal;
  char errmsg[200];
};

typedef struct csv_value_t csv_value_t;
struct csv_value_t {
  char *ptr;
  int len;
  bool quoted;
};

/**
 *  This is a callback that is invoked when the parser needs data.
 *  Return #bytes copied into buf on success, 0 on EOF, -1 on error. If
 *  you return -1, be sure to write an error message into errbuf[].
 */
typedef int csv_feed_t(void *context, char *buf, int bufsz, char *errbuf,
                       int errsz);

/**
 *  This is a callback that is invoked per row.  Return 0 on success,
 *  -1 otherwise. If you return -1, be sure to write an error message
 *  into errbuf[] using lineno and rowno.
 *
 *  For each csv_value, call csv_unquote() to obtain a NUL-terminated string.
 */
typedef int csv_perrow_t(void *context, int n, csv_value_t value[],
                         int64_t lineno, int64_t rowno, char *errbuf,
                         int errsz);

/**
 *  Open a scan. The csv_t handle returned must be freed using
 *  csv_close() after use.
 *
 *  Note: this is a 3-part open(), parse(), close() interface to cater to C++
 *  exceptions (or longjmp) being thrown inside the callback routines during
 *  parse() operation.
 */
CSV_EXTERN csv_t csv_open(csv_config_t config);

/**
 *  Run the scan and invoke callbacks on demand. Always returns the csv param.
 *  Check csv->ok for success or failure.
 */
CSV_EXTERN csv_t *csv_parse(csv_t *csv, void *context, csv_feed_t *feed,
                            csv_perrow_t *perrrow);

/**
 *  Parse a file. This function will call csv_parse(). Always returns the csv
 * param. Check csv->ok for success or failure.
 */
CSV_EXTERN csv_t *csv_parse_file(csv_t *csv, FILE *fp, void *context,
                                 csv_perrow_t *perrow);

/**
 *  Close the scan and release resources.
 */
CSV_EXTERN void csv_close(csv_t *csv);

/**
 *  Unquote a value and return a NUL-terminated string.
 */
CSV_EXTERN char *csv_unquote(csv_value_t value, int qte, int esc);

/**
 *  Get the default config. Set values if default is not correct, and
 *  pass to csv_open().
 */
CSV_EXTERN csv_config_t csv_default_config(void);

#endif
