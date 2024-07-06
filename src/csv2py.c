
const char *usagestr = "\n\
  USAGE: %s [-h] [-d delim] [-q quote] [-e esc] [-n nullstr] FILE\n\
                        \n\
                        \n\
  Print a csv file in a format that can be read into a \n\
  python program and eval() into a python object\n\
                                           \n\
  For example, a csv file with these lines:\n\
    1|2|(null)|3                           \n\
    \"abcd\"|efg|hij|klm                   \n\
                                           \n\
  will be printed as:                      \n\
                                           \n\
   [                                       \n\
     ['1','2',None,'3'],                   \n\
     ['abcd','efg','hij','klm']            \n\
   ]                                       \n\
                        \n\
  OPTIONS:              \n\
                        \n\
      -h         : print this message          \n\
      -q quote   : specify quote char; default to double-quote           \n\
      -e esc     : specify escape char; default to the quote char        \n\
      -n nullstr : specify string representing null; default to \"\"     \n\
      \n\
";

#include "csv.h"
#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int qte = '"';
int esc = '"';
int delim = ',';
const char *nullstr = "(null)";
int nulllen = 6;
const char *pname = 0; // argv[0]
const char *path = 0;

static void usage(int exitcode, const char *msg) {
  fprintf(stderr, usagestr, pname);
  fprintf(stderr, "\n");
  fprintf(stderr, "%s\n", msg);
  exit(exitcode);
}

static int is_regular_file(const char *path) {
  struct stat path_stat;
  if (stat(path, &path_stat) != 0) {
    perror("stat");
    exit(EXIT_FAILURE);
  }
  return S_ISREG(path_stat.st_mode);
}

static void parse_cmdline(int argc, char **argv) {
  pname = argv[0];
  int opt;
  const char *d = 0;
  const char *q = 0;
  const char *e = 0;
  const char *n = 0;
  while ((opt = getopt(argc, argv, "d:q:e:n:h")) != -1) {
    switch (opt) {
    case 'd':
      d = optarg;
      break;
    case 'q':
      q = optarg;
      break;
    case 'e':
      e = optarg;
      break;
    case 'n':
      n = optarg;
      break;
    case 'h':
      usage(0, 0);
      break;
    default:
      usage(1, 0);
      break;
    }
  }

  if (optind + 1 == argc) {
    path = argv[optind];
    if (!is_regular_file(path)) {
      usage(1, "ERROR: the file must be a regular file");
    }
  } else {
    usage(1, "ERROR: specify a file path");
  }

  /* qte */
  if (q) {
    if (strlen(q) != 1) {
      usage(1, "ERROR: -q qoute expects a single char.");
    }
    qte = q[0];
  }

  /* esc */
  if (e) {
    if (strlen(e) != 1) {
      usage(1, "ERROR: -e esc expects a single char.");
    }
    esc = e[0];
  }

  /* delim */
  if (d) {
    if (strlen(d) != 1) {
      usage(1, "ERROR: -d delim expects a single char.");
    }
    delim = d[0];
  }

  /* nullstr */
  if (n) {
    nullstr = n;
    nulllen = strlen(n);
  }
}

static int special(const char *ptr, int len) {
  for (const char *q = ptr + len; ptr < q; ptr++) {
    if (!isprint(*ptr) || *ptr == '\'') {
      return 1;
    }
  }
  return 0;
}

typedef struct context_t context_t;
struct context_t {
  char *buf;
  int max;
};

static int notify(void *ctx_, int n, const csv_value_t value[], csv_t *csv) {
  context_t *ctx = (context_t *)ctx_;
  printf("[");
  for (int i = 0; i < n; i++) {
    int len = value[i].len;
    const char *ptr = value[i].ptr;
    if (i) {
      printf(", ");
    }
    if (!value[i].quoted) {
      if (nulllen == len &&
	  0 == memcmp(nullstr, ptr, len)) {
	printf("None");
      } else {
        printf("'%.*s'", len, ptr);
      }
      continue;
    }
    if (ctx->max <= len) {
      int newmax = (len + 10) * 1.5;
      free(ctx->buf);
      ctx->buf = 0;
      ctx->max = 0;

      ctx->buf = malloc(newmax);
      if (!ctx->buf) {
        fprintf(stderr, "ERROR: out of memory\n");
        exit(1);
      }
      ctx->max = newmax;
    }

    memcpy(ctx->buf, ptr, len);
    char *val;
    int vlen;
    if (csv_unquote(csv, ctx->buf, len, &val, &vlen)) {
      fprintf(stderr, "ERROR: csv_unquote failed\n");
      exit(1);
    }
    if (special(val, vlen)) {
      printf("'''%.*s'''", vlen, val);
    } else {
      printf("'%.*s'", vlen, val);
    }
  }
  printf("],\n");
  return 0;
}

int main(int argc, char *argv[]) {

  csv_filescan_t *scan;
  csv_status_t status;
  context_t context = {0};

  parse_cmdline(argc, argv);
  scan = csv_filescan_open(path, &context, qte, esc, delim, notify, &status);
  if (!scan) {
    fprintf(stderr, "csv_scanfile_open: %s", status.errmsg);
    exit(1);
  }
  printf("[\n");
  if (csv_filescan_run(scan, &status)) {
    fprintf(stderr, "csv_scanfile_run: %s", status.errmsg);
    exit(1);
  }
  printf("]\n");

  csv_filescan_close(scan);

  free(context.buf);
  return 0;
}
