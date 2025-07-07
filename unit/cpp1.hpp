#pragma once

#include "../src/csv.hpp"

using namespace std;

namespace cpp1 {

class parser_t : public csv_parser_t {
public:
  // this holds the result of a parsing a csv document
  // using the perrow function below.
  vector<vector<string>> result;
  // This holds the chars that are fed to the csv parser using the feed
  // function below.
  string input;
  int offset = 0; // first byte yet to be consumed in input[]

  void set_input(string_view s) {
    input = s;
    offset = 0;
  }

  static int perrow(void *ctx, int n, csv_value_t value[], int64_t lineno,
                    int64_t rowno, char *errbuf, int errsz) {
    parser_t *p = (parser_t *)ctx;
    (void)lineno;
    (void)rowno;
    (void)errbuf;
    (void)errsz;
    p->result.clear();
    vector<string> row;
    for (int i = 0; i < n; i++) {
      row.push_back(string{value[i].ptr});
    }
    p->result.push_back(std::move(row));
    return 0;
  }

  static int feed(void *ctx, char *buf, int bufsz, char *errbuf, int errsz) {
    parser_t *p = (parser_t *)ctx;
    (void)errbuf;
    (void)errsz;
    int avail = 0;
    if (p->offset < (int)p->input.size()) {
      avail = p->input.size() - p->offset;
    }
    if (avail > bufsz) {
      avail = bufsz;
    }
    memcpy(buf, p->input.data() + p->offset, avail);
    p->offset += avail;
    return avail;
  }
};

}; // namespace cpp1

TEST_CASE("cpp1") {

  using namespace cpp1;

  SUBCASE("open and shut") {
    parser_t p;
    p.set_input("a|b|c");
    p.set_delim('|');
    p.parse(parser_t::feed, parser_t::perrow);
    CHECK(p.ok());
    CHECK(p.result.size() == 1);
    CHECK(p.result[0] == vector<string>{"a", "b", "c"});
  }
};
