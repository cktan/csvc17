#pragma once

using namespace std;

namespace csv1 {

struct context_t {
  const char QTE = '"';
  const char ESC = '"';
  const char DELIM = '|';
  csv_t csv;
  const char *doc;
  std::vector<std::vector<std::string>> result;
  context_t(const char *doc_) : doc(doc_) {
    auto conf = csv_default_config();
    conf.qte = QTE;
    conf.esc = ESC;
    conf.delim = DELIM;
    csv = csv_open(&conf);
  }
  ~context_t() { csv_close(&csv); }
  context_t(context_t &) = delete;
  context_t &operator=(context_t &) = delete;
  context_t(context_t &&) = delete;
  context_t &operator=(context_t &&) = delete;
};

static int feed(void *ctx_, char *buf, int bufsz, char *errbuf, int errsz) {
  (void)errbuf;
  (void)errsz;
  context_t *ctx = (context_t *)ctx_;
  int len = strlen(ctx->doc);
  if (len > bufsz) {
    len = bufsz;
  }
  memcpy(buf, ctx->doc, len);
  ctx->doc += len;
  return len;
}

static int perrow(void *ctx_, int n, csv_value_t value[], int64_t lineno,
                  int64_t rowno, char *errbuf, int errsz) {
  (void)lineno;
  (void)rowno;
  (void)errbuf;
  (void)errsz;
  context_t *ctx = (context_t *)ctx_;
  std::vector<std::string> row;
  row.resize(n);
  for (int i = 0; i < n; i++) {
    char *p = value[i].ptr;
    if (!p) {
      fprintf(stderr, "Internal error!");
      abort();
    }
    row[i] = p;
  }
  ctx->result.push_back(std::move(row));
  return 0;
}

} // namespace csv1

TEST_CASE("csv1") {

  using namespace csv1;

  SUBCASE("open and shut") {
    context_t ctx{""};
    CHECK(ctx.csv.ok);
  }

  SUBCASE("one row") {
    context_t ctx{"abc\r\n"};
    CHECK(ctx.csv.ok);
    csv_parse(&ctx.csv, (void *)&ctx, feed, perrow);
    CHECK(ctx.csv.ok);
    CHECK(ctx.result.size() == 1);
    CHECK(ctx.result[0].size() == 1);
    CHECK(ctx.result[0][0] == "abc");
  }

  SUBCASE("two rows") {
    context_t ctx{"abc|def|ghi\r\njkl|mno|pqr"};
    CHECK(ctx.csv.ok);
    csv_parse(&ctx.csv, (void *)&ctx, feed, perrow);
    CHECK(ctx.csv.ok);
    CHECK(ctx.result.size() == 2);
    auto row1 = ctx.result[0];
    auto row2 = ctx.result[1];
    CHECK(row1.size() == 3);
    CHECK(row1[0] == "abc");
    CHECK(row1[1] == "def");
    CHECK(row1[2] == "ghi");
    CHECK(row2.size() == 3);
    CHECK(row2[0] == "jkl");
    CHECK(row2[1] == "mno");
    CHECK(row2[2] == "pqr");
  }
}
