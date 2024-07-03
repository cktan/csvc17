#pragma once

extern "C" {
#include "../src/csv.h"
}

#include <algorithm>
#include <cstring>
#include <random>
#include <vector>

using namespace std;

TEST_CASE("csv1") {
  csv_status_t status;

  SUBCASE("open and shut") {
    csv_t *csv =
        csv_open((void *)1, '"', '"', '|',
                 [](void *, int, const csv_value_t *, csv_t *) { return 0; });
    CHECK(csv);
    csv_close(csv);
  }
  SUBCASE("one row") {
    struct context_t {
      const char *raw = "abc\r\n";
      const int len = strlen(raw);
      int nrow = 0;
    } context;
    csv_t *csv =
        csv_open((void *)&context, '"', '"', '|',
                 [](void *ctx_, int nval, const csv_value_t *val, csv_t *) {
                   context_t *ctx = (context_t *)ctx_;
                   ctx->nrow++;
                   CHECK(nval == 1);
                   CHECK(val[0].len == 3);
                   CHECK(0 == memcmp(val[0].ptr, "abc", 3));
                   return 0;
                 });
    CHECK(csv);
    int n = csv_feed(csv, context.raw, context.len, &status);
    CHECK(n == context.len);
    csv_close(csv);

    CHECK(context.nrow == 1);
  }
  SUBCASE("two rows") {
    struct context_t {
      const char *raw = "abc|def|ghi\r\njkl|mno|pqr\n";
      const int len = strlen(raw);
      int nrow = 0;
    } context;
    csv_t *csv =
        csv_open((void *)&context, '"', '"', '|',
                 [](void *ctx_, int nval, const csv_value_t *val, csv_t *) {
                   context_t *ctx = (context_t *)ctx_;
                   ctx->nrow++;
                   CHECK(nval == 3);
                   CHECK(val[0].len == 3);
                   CHECK(val[1].len == 3);
                   CHECK(val[2].len == 3);
                   switch (ctx->nrow) {
                   case 1:
                     CHECK(0 == memcmp(val[0].ptr, "abc", 3));
                     CHECK(0 == memcmp(val[1].ptr, "def", 3));
                     CHECK(0 == memcmp(val[2].ptr, "ghi", 3));
                     break;
                   case 2:
                     CHECK(0 == memcmp(val[0].ptr, "jkl", 3));
                     CHECK(0 == memcmp(val[1].ptr, "mno", 3));
                     CHECK(0 == memcmp(val[2].ptr, "pqr", 3));
                     break;
                   default:
                     CHECK(0);
                   }
                   return 0;
                 });
    CHECK(csv);
    int n = csv_feed(csv, context.raw, context.len, &status);
    CHECK(n == context.len);
    CHECK(status.rowno == 3);
    CHECK(status.rowpos == n);
    csv_close(csv);

    CHECK(context.nrow == 2);
  }
  SUBCASE("2 rows with remainder") {
    const char *raw = "abc|def|ghi\njkl|mno|pqr\nxxx";
    const int len = strlen(raw);
    csv_t *csv =
        csv_open((void *)1, '"', '"', '|',
                 [](void *, int, const csv_value_t *, csv_t *) { return 0; });
    CHECK(csv);

    int n = csv_feed(csv, raw, len, &status);
    CHECK(n == len - 3);
    CHECK(status.rowno == 3);
    CHECK(status.fldno == 1);
    CHECK(status.fldpos == status.rowpos);

    raw = raw + len - 3;
    n = csv_feed(csv, raw, len, &status);
    CHECK(n == -1);
    csv_close(csv);
  }
}
