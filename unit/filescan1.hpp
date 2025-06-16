#pragma once

extern "C" {
#include "../src/csvc17.h"
}

#include <cstring>

using namespace std;

TEST_CASE("filescan1") {
  csv_status_t status;

  const char *path = "/tmp/csv_filescan_test.cv";

  // Create an output file stream object
  {
    std::ofstream out(path);
    CHECK(out);
    out << "abc|def|hij\nabc|def|hij";
    out.close();
  }

  SUBCASE("open and shut") {
    csv_filescan_t *fs = csv_filescan_open(
        path, (void *)1, '"', '"', '|',
        [](void *, int, const csv_value_t *, csv_t *) { return 0; }, &status);
    CHECK(fs);
    csv_filescan_close(fs);
  }
  SUBCASE("2 rows with missing final newline") {
    struct context_t {
      int count;
    } context;
    context.count = 0;
    csv_filescan_t *fs = csv_filescan_open(
        path, (void *)&context, '"', '"', '|',
        [](void *ctx_, int, const csv_value_t *, csv_t *) {
          context_t *context = (context_t *)ctx_;
          context->count++;
          return 0;
        },
        &status);
    CHECK(fs);
    CHECK(0 == csv_filescan_run(fs, &status));
    CHECK(context.count == 2);
    csv_filescan_close(fs);
  }
}
