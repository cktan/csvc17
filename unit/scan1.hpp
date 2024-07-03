#pragma once

extern "C" {
#include "../src/scan.h"
}

#include <cstring>

using namespace std;

TEST_CASE("scan1") {

  scan_t scan;
  scan_init(&scan, '"', '\\', '|');

  SUBCASE("simple") {

    const char *raw = "here is a quote \", "
                      "followed by a backslash \\, "
                      "followed by a pipe |, "
                      "and finally a newline\n";

    scan_reset(&scan, raw, strlen(raw));

    const char *quote = strchr(raw, '"');
    const char *backslash = strchr(raw, '\\');
    const char *pipe = strchr(raw, '|');
    const char *newline = strchr(raw, '\n');
    CHECK(quote);
    CHECK(quote == scan_peek(&scan));
    CHECK(quote == scan_pop(&scan));
    CHECK(*quote == '"');

    CHECK(backslash);
    CHECK(backslash == scan_peek(&scan));
    CHECK(backslash == scan_pop(&scan));
    CHECK(*backslash == '\\');

    CHECK(pipe);
    CHECK(pipe == scan_peek(&scan));
    CHECK(pipe == scan_pop(&scan));
    CHECK(*pipe == '|');

    CHECK(newline);
    CHECK(newline == scan_peek(&scan));
    CHECK(newline == scan_pop(&scan));
    CHECK(*newline == '\n');

    CHECK(nullptr == scan_peek(&scan));
    CHECK(nullptr == scan_pop(&scan));
  }

  SUBCASE("empty string") {
    const char *raw = "";
    scan_reset(&scan, raw, strlen(raw));
    const char *p = scan_pop(&scan);
    CHECK(p == nullptr);
  }

  SUBCASE("all char special; first char is special") {
    const char *raw = "|||||";
    scan_reset(&scan, raw, strlen(raw));
    for (int i = 0; i < 5; i++) {
      const char *p = scan_pop(&scan);
      CHECK(p == raw + i);
    }
    CHECK(nullptr == scan_pop(&scan));
  }
}
