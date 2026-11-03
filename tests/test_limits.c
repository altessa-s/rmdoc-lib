/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "test.h"

typedef struct {
  size_t selected, delivered;
} counts;

static rmd_status selection(void* context, const rmd_entry* entry,
                            bool* include) {
  (void)entry;
  counts* c = context;
  CHECK(c->delivered == 0);
  c->selected++;
  *include = true;
  return RMD_OK;
}

static rmd_status body(void* context, const rmd_entry* entry, rmd_bytes data) {
  (void)entry;
  (void)data;
  counts* c = context;
  CHECK(c->selected == 3);
  c->delivered++;
  return RMD_OK;
}

static rmd_status refuse(void* context, const rmd_entry* entry, bool* include) {
  (void)context;
  (void)entry;
  (void)include;
  return RMD_ERR_IO;
}

int main(void) {
  rmd_buffer b = sample();
  rmd_limits d = rmd_limits_default();
  CHECK(d.maximum_entry_count == 65535 && d.maximum_compression_ratio == 200 &&
        d.ratio_threshold == (1u << 20));
  CHECK(rmd_limits_format().maximum_compression_ratio == 1032);
  rmd_limits cases[5];
  for (size_t i = 0; i < 5; i++) cases[i] = d;
  cases[0].maximum_entry_size = 1;
  cases[1].maximum_total_size = 12;
  cases[2].maximum_compressed_entry_size = 1;
  cases[3].maximum_compressed_total_size = 12;
  cases[4].ratio_threshold = 0;
  cases[4].maximum_compression_ratio = 0;
  rmd_status expected[] = {RMD_ERR_ENTRY_TOO_LARGE, RMD_ERR_TOTAL_SIZE_EXCEEDED,
                           RMD_ERR_COMPRESSED_ENTRY_TOO_LARGE,
                           RMD_ERR_COMPRESSED_TOTAL_SIZE_EXCEEDED, RMD_OK};
  for (size_t i = 0; i < 5; i++) {
    rmd_reader* r = open_buffer(b, &cases[i]);
    size_t callbacks = 0;
    CHECK(rmd_reader_foreach(r, nullptr, nullptr, discard, &callbacks,
                             nullptr) == expected[i]);
    CHECK(callbacks == (i == 4 ? 3u : 0u));
    OK(rmd_reader_foreach(r, skip, nullptr, discard, nullptr, nullptr));
    rmd_reader_close(r);
  }
  for (size_t i = 0; i < 3; i++) {
    rmd_limits l = d;
    rmd_reader* r = nullptr;
    rmd_status expected_status;
    if (!i) {
      l.maximum_entry_count = 2;
      expected_status = RMD_ERR_TOO_MANY_ENTRIES;
    } else if (i == 1) {
      l.maximum_directory_size = 1;
      expected_status = RMD_ERR_DIRECTORY_TOO_LARGE;
    } else {
      l.maximum_name_length = 1;
      expected_status = RMD_ERR_NAME_TOO_LONG;
    }
    CHECK(rmd_reader_open((rmd_bytes){b.bytes, b.size}, &l, &r, nullptr) ==
          expected_status);
    CHECK(!r);
  }
  rmd_reader* r = open_buffer(b, nullptr);
  counts c = {0};
  OK(rmd_reader_foreach(r, selection, &c, body, &c, nullptr));
  CHECK(c.delivered == 3);
  CHECK(rmd_reader_foreach(r, refuse, nullptr, discard, nullptr, nullptr) ==
        RMD_ERR_CALLBACK);
  rmd_archive a = {0};
  OK(rmd_reader_unpack(r, skip, nullptr, &a, nullptr));
  CHECK(a.count == 0);
  rmd_archive_free(&a);
  rmd_reader_close(r);
  rmd_buffer_free(&b);
  puts("limits: passed");
  return 0;
}
