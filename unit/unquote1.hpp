#pragma once

#include "../src/csvc17.h"

#include <cstring>

using namespace std;

static char *do_unquote(std::string s, bool quoted = true) {
  static csv_config_t conf = csv_default_config();
  char accept[3];
  {
    int i = 0;
    accept[i++] = conf.qte;
    accept[i++] = (conf.esc != conf.qte ? conf.esc : 0);
    accept[i] = 0;
  }
  scan_t scan = scan_init(accept);
  csv_value_t value;
  value.ptr = s.data();
  value.len = s.length();
  value.quoted = quoted;
  unquote(&scan, &value, &conf);
  return value.ptr;
}

TEST_CASE("unquote1") {

  // quote: "
  // escape: "
  // delim: |

  SUBCASE("nop") {
    {
      string raw = "an unquoted string";
      string v = do_unquote(raw, false);
      CHECK(v == raw);
    }

    {
      string raw = "";
      // this is a NULL, not an empty string.
      CHECK(nullptr == do_unquote(raw, false));
    }
  }

  SUBCASE("simple") {
    // this just return the string without the quotes
    {
      string raw = "\"hello there\"";
      string v = do_unquote(raw);
      CHECK(v == "hello there");
    }

    {
      string raw = "\"a\"";
      string v = do_unquote(raw);
      CHECK(v == "a");
    }

    {
      string raw = "\"\"";
      // this is an empty string, not a NULL.
      string v = do_unquote(raw);
      CHECK(v == "");
    }
  }

  SUBCASE("complex") {
    {
      string raw = "\"abcd\"\"efg\"";
      // input: "abcd""efg".  output: abcd"efg
      string v = do_unquote(raw);
      CHECK(v == "abcd\"efg");
    }

    {
      string raw = "\"w\"\"x\"\"\""; // "w""x"""
      string v = do_unquote(raw);
      CHECK(v == "w\"x\"");
    }
  }

  SUBCASE("multiline") {
    {
      string raw = "\"abcd\n\n\n\n\nefg\"";
      string v = do_unquote(raw);
      CHECK(v == "abcd\n\n\n\n\nefg");
    }
  }
}
