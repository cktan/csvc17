
const char *usagestr = "\n\
  USAGE: %s [-h] [-d delim] [-q quote] [-e esc] [-n nullstr] [FILE]\n\
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

#include "../src/csvc17.h"
#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// params
int QTE = '"';
int ESC = '"';
int DELIM = ',';
const char *NULLSTR = "(null)";
const char *PATH = 0;

// argv[0]
const char *pname = 0;

static void usage(int exitcode, const char *msg) {
  fprintf(stderr, usagestr, pname);
  fprintf(stderr, "\n");
  fprintf(stderr, "%s\n", msg);
  exit(exitcode);
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
    PATH = argv[optind];
  } else {
    usage(1, "ERROR: specify a file path");
  }

  /* qte */
  if (q) {
    if (strlen(q) != 1) {
      usage(1, "ERROR: -q qoute expects a single char.");
    }
    QTE = q[0];
  }

  /* esc */
  if (e) {
    if (strlen(e) != 1) {
      usage(1, "ERROR: -e esc expects a single char.");
    }
    ESC = e[0];
  }

  /* delim */
  if (d) {
    if (strlen(d) != 1) {
      usage(1, "ERROR: -d delim expects a single char.");
    }
    DELIM = d[0];
  }

  /* nullstr */
  if (n) {
    NULLSTR = n;
  }
}

static int special(const char *ptr) {
  for (; *ptr; ptr++) {
    if (!isprint(*ptr) || *ptr == '\'') {
      return 1;
    }
  }
  return 0;
}

static int perrow(void *context, int n, csv_value_t value[], int64_t lineno,
                  int64_t rowno, char *errbuf, int errsz) {
  (void)context;
  (void)lineno;
  (void)rowno;
  (void)errbuf;
  (void)errsz;
  printf("    [");
  for (int i = 0; i < n; i++) {
    const char *ptr = csv_unquote(value[i], QTE, ESC);
    assert(ptr);

    if (i) {
      printf(", ");
    }

    if (!value[i].quoted) {
      if (0 == strcmp(NULLSTR, ptr)) {
        printf("None");
      } else {
        printf("r'%s'", ptr);
      }
      continue;
    }

    if (special(ptr)) {
      printf("r'''%s'''", ptr);
    } else {
      printf("r'%s'", ptr);
    }
  }
  printf("],\n");
  return 0;
}

int main(int argc, char *argv[]) {

  parse_cmdline(argc, argv);
  FILE *fp = stdin;
  if (PATH) {
    fp = fopen(PATH, "r");
    if (!fp) {
      perror("fopen");
      exit(1);
    }
  }

  csv_t csv = csv_open(QTE, ESC, DELIM);
  if (!csv.ok) {
    fprintf(stderr, "ERROR: %s\n", csv.errmsg);
    exit(1);
  }
  printf("[\n");
  csv_parse_file(&csv, fp, 0, perrow);
  if (!csv.ok) {
    fprintf(stderr, "ERROR: %s\n", csv.errmsg);
    exit(1);
  }
  printf("]\n");
  csv_close(&csv);
  return 0;
}
