#pragma once

#include "../src/csvc17.h"

#include <cstring>

using namespace std;

static char *unquote(std::string s, bool quoted = true) {
  const int QTE = '"';
  const int ESC = '"';
  csv_value_t value;
  value.ptr = s.data();
  value.len = s.length();
  value.quoted = quoted;
  unquote(&value, QTE, ESC);
  return value.ptr;
}

TEST_CASE("unquote1") {

  // quote: "
  // escape: "
  // delim: |

  SUBCASE("nop") {
    {
      string raw = "an unquoted string";
      string v = unquote(raw, false);
      CHECK(v == raw);
    }

    {
      string raw = "";
      string v = unquote(raw, false);
      CHECK(v == raw);
    }
  }

  SUBCASE("simple") {
    // this just return the string without the quotes
    {
      string raw = "\"hello there\"";
      string v = unquote(raw);
      CHECK(v == "hello there");
    }

    {
      string raw = "\"a\"";
      string v = unquote(raw);
      CHECK(v == "a");
    }

    {
      string raw = "\"\"";
      string v = unquote(raw);
      CHECK(v == "");
    }
  }

  SUBCASE("complex") {
    {
      string raw = "\"abcd\"\"efg\"";
      // input: "abcd""efg".  output: abcd"efg
      string v = unquote(raw);
      CHECK(v == "abcd\"efg");
    }

    {
      string raw = "\"w\"\"x\"\"\""; // "w""x"""
      string v = unquote(raw);
      CHECK(v == "w\"x\"");
    }
  }

  SUBCASE("multiline") {
    {
      string raw = "\"abcd\n\n\n\n\nefg\"";
      string v = unquote(raw);
      CHECK(v == "abcd\n\n\n\n\nefg");
    }
  }
}
