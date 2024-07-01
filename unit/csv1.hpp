#pragma once

extern "C" {
#include "../src/csv.h"
}

#include <algorithm>
#include <cstring>
#include <random>
#include <vector>

using namespace std;

TEST_CASE("02: csv") {
  csv_status_t status;

  SUBCASE("open and shut") {
    csv_t *csv = csv_open((void *)1, '"', '"', '|',
                          [](void *, int, const csv_value_t *) { return 0; });
    CHECK(csv);
    csv_close(csv);
  }
  SUBCASE("one row") {
    const char *raw = "abc\n";
    const int len = strlen(raw);
    csv_t *csv = csv_open((void *)1, '"', '"', '|',
                          [](void *, int, const csv_value_t *) { return 0; });
    CHECK(csv);
    int n = csv_feed(csv, raw, len, &status);
    CHECK(n == len);
    csv_close(csv);
  }
  SUBCASE("two rows") {
    const char *raw = "abc|def|ghi\njkl|mno|pqr\n";
    const int len = strlen(raw);
    csv_t *csv = csv_open((void *)1, '"', '"', '|',
                          [](void *, int, const csv_value_t *) { return 0; });
    CHECK(csv);
    int n = csv_feed(csv, raw, len, &status);
    CHECK(n == len);
    CHECK(status.rowno == 3);
    CHECK(status.rowpos == n);
    csv_close(csv);
  }
  SUBCASE("2 rows with remainder") {
    const char *raw = "abc|def|ghi\njkl|mno|pqr\nxxx";
    const int len = strlen(raw);
    csv_t *csv = csv_open((void *)1, '"', '"', '|',
                          [](void *, int, const csv_value_t *) { return 0; });
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
