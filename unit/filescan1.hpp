#pragma once

using namespace std;
namespace filescan1 {

const char *PATH = "/tmp/csv_filescan_test.cv";

struct context_t {
  csv_t csv;
  std::vector<std::vector<std::string>> result;
  context_t() { csv = csv_open(0); }
  ~context_t() { csv_close(&csv); }
  context_t(context_t &) = delete;
  context_t &operator=(context_t &) = delete;
  context_t(context_t &&) = delete;
  context_t &operator=(context_t &&) = delete;
};

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

}; // namespace filescan1

TEST_CASE("filescan1") {

  using namespace filescan1;

  // Create an output file stream object
  {
    std::ofstream out(PATH);
    CHECK(out);
    out << "abc,def,hij\r\nxxx,yyy,zzz";
    out.close();
  }

  SUBCASE("sub1") {
    context_t ctx;

    FILE *fp = fopen(PATH, "r");
    CHECK(fp);
    csv_parse_file(&ctx.csv, fp, &ctx, perrow);

    CHECK(ctx.result.size() == 2);
    CHECK(ctx.result[0] == vector<string>{"abc", "def", "hij"});
    CHECK(ctx.result[1] == vector<string>{"xxx", "yyy", "zzz"});
  }

  SUBCASE("sub2") {
    context_t ctx;

    csv_parse_file_ex(&ctx.csv, PATH, &ctx, perrow);

    CHECK(ctx.result.size() == 2);
    CHECK(ctx.result[0] == vector<string>{"abc", "def", "hij"});
    CHECK(ctx.result[1] == vector<string>{"xxx", "yyy", "zzz"});
  }
}
