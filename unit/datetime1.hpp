#pragma once

#include "../src/csvc17.h"

using namespace std;

TEST_CASE("datetime1") {

  int year, month, day, hour, minute, sec, usec;
  char tzsign;
  int tzhour, tzminute;

  SUBCASE("date ymd") {
    CHECK(0 == csv_parse_ymd("2015-01-23", &year, &month, &day));
    CHECK(2015 == year);
    CHECK(1 == month);
    CHECK(23 == day);
  }

  SUBCASE("date mdy") {
    CHECK(0 == csv_parse_mdy("1/23/2015", &year, &month, &day));
    CHECK(2015 == year);
    CHECK(1 == month);
    CHECK(23 == day);
  }

  SUBCASE("time") {
    CHECK(0 == csv_parse_time("12:30:45.5", &hour, &minute, &sec, &usec));
    CHECK(hour == 12);
    CHECK(minute == 30);
    CHECK(sec == 45);
    CHECK(500000 == usec);
  }

  SUBCASE("timestamp") {
    CHECK(0 == csv_parse_timestamp("2015-01-23 12:30:45.5", &year, &month, &day,
                                   &hour, &minute, &sec, &usec));
    CHECK(2015 == year);
    CHECK(1 == month);
    CHECK(23 == day);
    CHECK(hour == 12);
    CHECK(minute == 30);
    CHECK(sec == 45);
    CHECK(500000 == usec);
  }

  SUBCASE("timestamptz") {
    CHECK(0 == csv_parse_timestamptz("2015-01-23 12:30:45.5+03:15", &year,
                                     &month, &day, &hour, &minute, &sec, &usec,
                                     &tzsign, &tzhour, &tzminute));
    CHECK(2015 == year);
    CHECK(1 == month);
    CHECK(23 == day);
    CHECK(hour == 12);
    CHECK(minute == 30);
    CHECK(sec == 45);
    CHECK(500000 == usec);
    CHECK('+' == tzsign);
    CHECK(3 == tzhour);
    CHECK(15 == tzminute);
  }
}
