#pragma once

extern "C" {
#include "../src/scan.h"
}

#include <cstring>

using namespace std;

TEST_CASE("scan1") {

  SUBCASE("simple") {

    const char *raw = "here is a quote \", "
                      "followed by a backslash \\, "
                      "followed by a pipe |, "
                      "and finally a newline\n";

    scan_t scan = scan_reset('"', '\\', '|', raw, strlen(raw));

    const char *const quote = strchr(raw, '"');
    const char *const backslash = strchr(raw, '\\');
    const char *const pipe = strchr(raw, '|');
    const char *const newline = strchr(raw, '\n');
    CHECK(quote);
    CHECK(backslash);
    CHECK(pipe);
    CHECK(newline);

    CHECK(quote == scan_next(&scan));
    CHECK(quote + 1 == scan_peek(&scan));

    CHECK(backslash == scan_next(&scan));
    CHECK(backslash + 1 == scan_peek(&scan));

    CHECK(pipe == scan_next(&scan));
    CHECK(pipe + 1 == scan_peek(&scan));

    CHECK(newline == scan_next(&scan));
    CHECK(0 == *scan_peek(&scan));

    CHECK(nullptr == scan_next(&scan));
    CHECK(0 == *scan_peek(&scan));
  }

  SUBCASE("empty string") {
    const char *raw = "";
    scan_t scan = scan_reset('"', '\\', '|', raw, strlen(raw));
    const char *p = scan_next(&scan);
    CHECK(p == nullptr);
  }

  SUBCASE("all char special; first char is special") {
    const char *raw = "|||||";
    scan_t scan = scan_reset('"', '\\', '|', raw, strlen(raw));
    for (int i = 0; i < 5; i++) {
      const char *p = scan_next(&scan);
      CHECK(p == raw + i);
    }
    CHECK(nullptr == scan_next(&scan));
  }

  SUBCASE("random") {
    char buf[1025];
    memset(buf, 'x', sizeof(buf));
    buf[sizeof(buf) - 1] = 0;
    int buflen = sizeof(buf) - 1;
    const char *special = "\"\\|\n";
    const int N = 100;
    for (int i = 0; i < N; i++) {
      int off = random() % buflen;
      int ch = special[random() % 4];
      if (buf[off] == 'x')
        buf[off] = ch;
      else
        i--; // collision; retry.
    }

    scan_t scan = scan_reset('"', '\\', '|', buf, buflen);
    char *xp = buf;
    for (int i = 0; i < N; i++) {
      const char *p = scan_next(&scan);
      xp += strcspn(xp, special);
      CHECK(p == xp);
      CHECK(p + 1 == scan_peek(&scan));
      xp++;
    }
    CHECK(nullptr == scan_next(&scan));
  }
}
