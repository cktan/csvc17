#pragma once

extern "C" {
#include "../src/csv.h"
}

#include <cstring>

using namespace std;

TEST_CASE("unquote1") {

  // quote: "
  // escape: "
  // delim: |

  csv_t *csv =
      csv_open((void *)1, '"', '"', '|',
               [](void *, int, const csv_value_t *, csv_t *) { return 0; });

  SUBCASE("nop") {
    string raw = "an unquoted string";
    char *val;
    int vlen;
    CHECK(0 == csv_unquote(csv, raw.data(), raw.length(), &val, &vlen));
    CHECK(val == raw.data());
    CHECK(vlen == raw.length());

    raw = "";
    CHECK(0 == csv_unquote(csv, raw.data(), raw.length(), &val, &vlen));
    CHECK(val == raw.data());
    CHECK(vlen == raw.length());
  }

  SUBCASE("simple") {
    // this just return the string without the quotes
    string raw = "\"hello there\"";
    char *val;
    int vlen;
    CHECK(0 == csv_unquote(csv, raw.data(), raw.length(), &val, &vlen));
    CHECK(val == raw.data() + 1);
    CHECK(vlen == raw.length() - 2);

    raw = "\"a\"";
    CHECK(0 == csv_unquote(csv, raw.data(), raw.length(), &val, &vlen));
    CHECK(val == raw.data() + 1);
    CHECK(vlen == raw.length() - 2);

    raw = "\"\"";
    CHECK(0 == csv_unquote(csv, raw.data(), raw.length(), &val, &vlen));
    CHECK(val == raw.data() + 1);
    CHECK(vlen == raw.length() - 2);
  }

  SUBCASE("complex") {
    string expected = "abcd\"efg";
    string raw = "\"abcd\"\"efg\"";
    // input: "abcd""efg".  output: abcd"efg
    char *val;
    int vlen;
    CHECK(0 == csv_unquote(csv, raw.data(), raw.length(), &val, &vlen));
    CHECK(vlen == expected.length());
    CHECK(0 == memcmp(expected.data(), val, vlen));
  }

  csv_close(csv);
}
