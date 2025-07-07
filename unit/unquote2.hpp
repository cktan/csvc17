#pragma once

#include <algorithm>
#include <random>
#include <vector>

using namespace std;

TEST_CASE("unquote2") {

  // quote: double-quote
  // escape: backslash
  // delim: pipe

  csv_t *csv =
      csv_open((void *)1, '"', '\\', '|',
               [](void *, int, const csv_value_t *, csv_t *) { return 0; });

  string expected;
  string raw;
  char *val;
  int vlen;

  SUBCASE("not in quote") {
    // when raw string is not in quote

    raw = "abcd";
    CHECK(0 == csv_unquote(csv, raw.data(), raw.length(), &val, &vlen));
    CHECK(val == raw.data());
    CHECK(vlen == raw.length());

    raw = "ab\"c\"d"; // ab, quoted c, d
    expected = "abcd";
    CHECK(0 == csv_unquote(csv, raw.data(), raw.length(), &val, &vlen));
    CHECK(vlen == expected.size());
    CHECK(0 == memcmp(expected.data(), val, vlen));

    raw = "ab\"\"c\"\"d"; // ab, quoted empty, c, quoted empty, d
    expected = "abcd";
    CHECK(0 == csv_unquote(csv, raw.data(), raw.length(), &val, &vlen));
    CHECK(vlen == expected.size());
    CHECK(0 == memcmp(expected.data(), val, vlen));

    /* backslash is verbatim when not wrapped in quote */
    raw = "ab\\cd"; // ab, unescaped \, cd
    expected = "ab\\cd";
    CHECK(0 == csv_unquote(csv, raw.data(), raw.length(), &val, &vlen));
    CHECK(vlen == expected.size());
    CHECK(0 == memcmp(expected.data(), val, vlen));

    /* backslash is verbatim when not wrapped in quote */
    raw = "ab\\\\cd"; // ab, unescaped \, unescaped \, cd
    expected = "ab\\\\cd";
    CHECK(0 == csv_unquote(csv, raw.data(), raw.length(), &val, &vlen));
    CHECK(vlen == expected.size());
    CHECK(0 == memcmp(expected.data(), val, vlen));
  }

  SUBCASE("in quote") {
    // when raw string is in quote

    raw = "\"abcd\""; // quote, abcd, quote
    expected = "abcd";
    CHECK(0 == csv_unquote(csv, raw.data(), raw.length(), &val, &vlen));
    CHECK(vlen == expected.size());
    CHECK(0 == memcmp(expected.data(), val, vlen));

    // note: blackslash can only be used to escape backslash and quote!!

    // verbatim backslash because it cannot be used to escape c
    raw = "\"ab\\cd\"";  // quote, ab, blackslash, cd, quote
    expected = "ab\\cd"; // ab, backslash, cd
    CHECK(0 == csv_unquote(csv, raw.data(), raw.length(), &val, &vlen));
    CHECK(vlen == expected.size());
    CHECK(0 == memcmp(expected.data(), val, vlen));

    // escape a quote
    raw = "\"ab\\\"cd\""; // quote, ab, backslash, quote, cd, quote
    expected = "ab\"cd";  // ab, quote, cd
    CHECK(0 == csv_unquote(csv, raw.data(), raw.length(), &val, &vlen));
    CHECK(vlen == expected.size());
    CHECK(0 == memcmp(expected.data(), val, vlen));

    // escape a backslash
    raw = "\"ab\\\\cd\""; // quote, ab, backslash, blackslash, cd, quote
    expected = "ab\\cd";  // ab, backslash, cd
    CHECK(0 == csv_unquote(csv, raw.data(), raw.length(), &val, &vlen));
    CHECK(vlen == expected.size());
    CHECK(0 == memcmp(expected.data(), val, vlen));

    // quote quote -> out of quote, and back into quote
    raw = "\"ab\"\"cd\""; // quote, ab, quote, quote, cd, quote
    expected = "abcd";    // ab, backslash, cd
    CHECK(0 == csv_unquote(csv, raw.data(), raw.length(), &val, &vlen));
    CHECK(vlen == expected.size());
    CHECK(0 == memcmp(expected.data(), val, vlen));

    // quote quote -> out of quote, and back into quote
    raw = "\"ab\" \"cd\""; // quote, ab, quote, space, quote, cd, quote
    expected = "ab cd";    // ab, space, cd
    CHECK(0 == csv_unquote(csv, raw.data(), raw.length(), &val, &vlen));
    CHECK(vlen == expected.size());
    CHECK(0 == memcmp(expected.data(), val, vlen));
  }

  csv_close(csv);
}
