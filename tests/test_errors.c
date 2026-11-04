/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "fixtures.h"
#include "test.h"

int main(void) {
  rmd_error e = {0};
  rmd_reader* sentinel = (rmd_reader*)(uintptr_t)1;
  CHECK(rmd_reader_open((rmd_bytes){nullptr, 1}, nullptr, &sentinel, &e) ==
        RMD_ERR_INVALID_ARGUMENT);
  CHECK(sentinel == (rmd_reader*)(uintptr_t)1 &&
        e.code == RMD_ERR_INVALID_ARGUMENT);
  CHECK(rmd_validate_name((rmd_str){nullptr, 1000}, RMD_DEVICE, &e) ==
        RMD_ERR_INVALID_NAME);
  rmd_buffer seed = sample();
  bool success = false;
  for (size_t budget = 0; budget < 300; budget++) {
    rmd_test_fail_after(budget);
    rmd_reader* r = nullptr;
    rmd_status s =
        rmd_reader_open((rmd_bytes){seed.bytes, seed.size}, nullptr, &r, &e);
    rmd_test_fail_after(SIZE_MAX);
    if (s == RMD_OK) {
      rmd_reader_close(r);
      success = true;
      break;
    }
    CHECK(s == RMD_ERR_OUT_OF_MEMORY && r == nullptr);
  }
  CHECK(success);
  rmd_reader* r = open_buffer(seed, nullptr);
  success = false;
  for (size_t budget = 0; budget < 100; budget++) {
    rmd_archive a = {0};
    rmd_test_fail_after(budget);
    rmd_status s = rmd_reader_unpack(r, nullptr, nullptr, &a, &e);
    rmd_test_fail_after(SIZE_MAX);
    if (s == RMD_OK) {
      rmd_archive_free(&a);
      success = true;
      break;
    }
    CHECK(s == RMD_ERR_OUT_OF_MEMORY && a.count == 0 && !a._storage);
  }
  CHECK(success);
  success = false;
  for (size_t budget = 0; budget < 100; budget++) {
    rmd_strings ids = {0};
    rmd_test_fail_after(budget);
    rmd_status s = rmd_reader_document_ids(r, &ids, &e);
    rmd_test_fail_after(SIZE_MAX);
    if (s == RMD_OK) {
      rmd_strings_free(&ids);
      success = true;
      break;
    }
    CHECK(s == RMD_ERR_OUT_OF_MEMORY && !ids._storage);
  }
  CHECK(success);
  rmd_reader_close(r);
  rmd_buffer_free(&seed);
  seed = (rmd_buffer){(uint8_t*)(uintptr_t)fixture_deflated,
                      sizeof fixture_deflated};
  r = open_buffer(seed, nullptr);
  success = false;
  for (size_t budget = 0; budget < 100; budget++) {
    rmd_test_fail_after(budget);
    rmd_status s =
        rmd_reader_foreach(r, nullptr, nullptr, discard, nullptr, &e);
    rmd_test_fail_after(SIZE_MAX);
    if (s == RMD_OK) {
      success = true;
      break;
    }
    CHECK(s == RMD_ERR_OUT_OF_MEMORY);
  }
  CHECK(success);
  rmd_reader_close(r);
  const rmd_file file = {STR("doc.content"), DATA("test payload")};
  success = false;
  for (size_t budget = 0; budget < 100; budget++) {
    rmd_buffer b = {0};
    rmd_test_fail_after(budget);
    rmd_status s = rmd_pack(&file, 1, &b, &e);
    rmd_test_fail_after(SIZE_MAX);
    if (s == RMD_OK) {
      rmd_buffer_free(&b);
      success = true;
      break;
    }
    CHECK(s == RMD_ERR_OUT_OF_MEMORY && !b.bytes);
  }
  CHECK(success);
  success = false;
  for (size_t budget = 0; budget < 100; budget++) {
    rmd_writer* w = nullptr;
    OK(rmd_writer_open("oom-writer.rmdoc", RMD_DEFLATE, RMD_DEVICE, &w,
                       nullptr));
    rmd_test_fail_after(budget);
    rmd_status s = rmd_writer_add(w, file, RMD_DEFAULT_COMPRESSION, &e);
    rmd_test_fail_after(SIZE_MAX);
    if (s == RMD_OK) {
      OK(rmd_writer_finish(w, nullptr));
      success = true;
    } else
      CHECK(s == RMD_ERR_OUT_OF_MEMORY);
    rmd_writer_close(w);
    if (success) break;
  }
  CHECK(success);
  CHECK(!remove("oom-writer.rmdoc"));
  puts("errors: passed");
  return 0;
}
