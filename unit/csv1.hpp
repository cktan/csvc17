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

  SUBCASE("open and shut") {
    csv_t *csv = csv_open((void *)1, '"', '"', '|',
                          [](void *, int, const char **, int *) { return 0; });
    CHECK(csv);
    csv_close(csv);
  }
  SUBCASE("one row") {
    const char *raw = "abc\n";
    csv_t *csv = csv_open((void *)1, '"', '"', '|',
                          [](void *, int, const char **, int *) { return 0; });
    CHECK(csv);
    csv_status_t status;
    int n = csv_feed(csv, raw, strlen(raw), &status);
    CHECK(n == strlen(raw));
    csv_close(csv);
  }
  SUBCASE("two rows") {
    const char *raw = "abc|def|ghi\njkl|mno|pqr\n";
    csv_t *csv = csv_open((void *)1, '"', '"', '|',
                          [](void *, int, const char **, int *) { return 0; });
    CHECK(csv);
    csv_status_t status;
    int n = csv_feed(csv, raw, strlen(raw), &status);
    CHECK(n == strlen(raw));
    csv_close(csv);
  }
  SUBCASE("2 rows with remainder") {
    const char *raw = "abc|def|ghi\njkl|mno|pqr\nxxx";
    csv_t *csv = csv_open((void *)1, '"', '"', '|',
                          [](void *, int, const char **, int *) { return 0; });
    CHECK(csv);
    csv_status_t status;
    int n = csv_feed(csv, raw, strlen(raw), &status);
    CHECK(n == strlen(raw) - 3);
    raw = raw + strlen(raw) - 3;
    n = csv_feed(csv, raw, strlen(raw), &status);
    CHECK(n == -1);
    csv_close(csv);
  }
}
