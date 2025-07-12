/* Copyright (c) 2024-2025, CK Tan.
 * https://github.com/cktan/csvc17/blob/main/LICENSE
 */
#ifndef CSVC17_H
#define CSVC17_H

/*
 *  USAGE:
 *
 *  1. Call csv_open() to obtain a handle.
 *  2. Call csv_parse() and supply a feed function and a per-row
 *     function, or call csv_parse_file().
 *  3. Call csv_close() to free up resources.
 *
 *  The feed function will be invoked automatically when the parser
 *  needs more data.
 *
 *  The per-row function will be invoked whenever a row is
 *  determined. Utility functions to parse date, time, and timestamp
 *  are also provided.
 *
 *  C++ Note: If the callback functions may potentially throw
 *  exceptions, make sure to catch the exception and call csv_close()
 *  to avoid resource leak.
 *
 */

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
  bool unquote_values; /* default true; */
  bool skip_header;    /* default false; */
  char nullstr[16];    /* what is NULL? default '' */
  char qte;            /* default double-quote */
  char esc;            /* default double-quote */
  char delim;          /* default comma */
  int initbufsz;       /* default 4KB */
  int maxbufsz;        /* default 1GB */
};

typedef struct csv_t csv_t;
struct csv_t {
  bool ok;          /* check this for error */
  void *__internal; /* do not touch */
  char errmsg[200]; /* if not ok, this stores an error message */
};

/**
 *  A cell in the CSV file.
 */
typedef struct csv_value_t csv_value_t;
struct csv_value_t {
  char *ptr; // for NULL, ptr will be nullptr.
  int len;
  bool quoted;
};

/**
 *  This callback is invoked when the parser needs data.
 *  Return #bytes copied into buf on success, 0 on EOF, -1 on error. If
 *  you return -1, be sure to write an error message into errbuf[].
 */
typedef int csv_feed_t(void *context, char *buf, int bufsz, char *errbuf,
                       int errsz);

/**
 *  This callback is invoked per row.  Return 0 on success,
 *  -1 otherwise. If you return -1, be sure to write an error message
 *  into errbuf[] using lineno and rowno.
 */
typedef int csv_perrow_t(void *context, int n, csv_value_t value[],
                         int64_t lineno, int64_t rowno, char *errbuf,
                         int errsz);

/**
 *  Open a scan. The csv_t handle returned must be freed using
 *  csv_close() after use. The param 'conf' may be NULL to use the
 *  default configuration.
 *
 *  Note: this is a 3-part open(), parse(), close() interface to cater to C++
 *  exceptions (or longjmp) being thrown inside the callback routines during
 *  parse() operation.
 */
CSV_EXTERN csv_t csv_open(const csv_config_t *conf);

/**
 *  Run the scan and invoke callbacks on demand. Return 0 on success, -1
 * otherwise. On failure, check for error message in csv->errmsg.
 */
CSV_EXTERN int csv_parse(csv_t *csv, void *context, csv_feed_t *feed,
                         csv_perrow_t *perrrow);

/**
 *  Parse a file. This function will call csv_parse(). Return 0 on success, -1
 * otherwise. On failure, check for error message in csv->errmsg.
 *
 *  Note: csv_close() will eventually call on fclose(fp) whether this function
 * succeed or not.
 */
CSV_EXTERN int csv_parse_file(csv_t *csv, FILE *fp, void *context,
                              csv_perrow_t *perrow);

/**
 *  Parse a file. This function will call csv_parse(). Return 0 on success, -1
 * otherwise. On failure, check for error message in csv->errmsg.
 */
CSV_EXTERN int csv_parse_file_ex(csv_t *csv, const char *path, void *context,
                                 csv_perrow_t *perrow);

/**
 *  Close the scan and release resources.
 */
CSV_EXTERN void csv_close(csv_t *csv);

/**
 *  Parse a YYYY-MM-DD. Return 0 on success, -1 otherwise.
 *
 *  The function only extract integer values matching the pattern. It
 *  does not validate the date values, i.e., day may be 32.
 */
CSV_EXTERN int csv_parse_ymd(const char *s, int *year, int *month, int *day);

/**
 *  Parse a M/D/YYYY. Return 0 on success, -1 otherwise.
 *
 *  The function only extract integer values matching the pattern. It
 *  does not validate the date values, i.e., day may be 32.
 */
CSV_EXTERN int csv_parse_mdy(const char *s, int *year, int *month, int *day);

/**
 *  Parse a HH:MM:SS{.subsec}. Return 0 on success, -1 otherwise.
 *
 *  The function only extract integer values matching the pattern. It
 *  does not validate the time values, i.e., hour may be 25.
 */
CSV_EXTERN int csv_parse_time(const char *s, int *hour, int *minute,
                              int *second, int *usec);

/**
 *  Parse a timestamp 'YYYY-MM-DD HH:MM:SS{.subsec}'. The character
 *  separating date and time may be a 'T' or a space. Return 0 on
 *  success, -1 otherwise.
 *
 *  The function only extract integer values matching the pattern. It
 *  function does not validate the date and time values.
 */
CSV_EXTERN int csv_parse_timestamp(const char *s, int *year, int *month,
                                   int *day, int *hour, int *minute,
                                   int *second, int *usec);

/**
 *  Parse a timestamptz 'YYYY-MM-DD HH:MM:SS{.subsec}{timezone}'. The
 *  character separating date and time may be a 'T' or a
 *  space. Timezone is formatted like this: '[+-]HH:MM'. Return 0 on
 *  success, -1 otherwise.
 *
 *  The function only extract integer values matching the pattern. It
 *  does not validate the timestamp values.
 */
CSV_EXTERN int csv_parse_timestamptz(const char *s, int *year, int *month,
                                     int *day, int *hour, int *minute,
                                     int *second, int *usec, char *tzsign,
                                     int *tzhour, int *tzminute);

/**
 *  Get the default config. Set values if default is not correct, and
 *  pass to csv_open().
 */
CSV_EXTERN csv_config_t csv_default_config(void);

#endif
