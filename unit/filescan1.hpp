#pragma once

extern "C" {
#include "../src/csv.h"
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
}
