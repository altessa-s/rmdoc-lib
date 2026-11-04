/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "test.h"

int main(void) {
  const char *path = "writer-test.rmdoc", *source = "writer-source.bin";
  rmd_writer* w = nullptr;
  OK(rmd_writer_open(path, RMD_STORE, RMD_DEVICE, &w, nullptr));
  CHECK(rmd_writer_finish(w, nullptr) == RMD_ERR_EMPTY_ARCHIVE);
  CHECK(rmd_writer_add_file(w, STR("../x"), "missing-file", nullptr) ==
        RMD_ERR_INVALID_NAME);
  rmd_file files[] = {{STR("doc.content"), DATA("{\"pages\":[]}")},
                      {STR("doc/page.rm"), DATA("ink")},
                      {STR("other.metadata"), DATA("")}};
  for (size_t i = 0; i < 3; i++)
    OK(rmd_writer_add(w, files[i], RMD_DEFAULT_COMPRESSION, nullptr));
  CHECK(rmd_writer_add(w, files[0], RMD_STORE, nullptr) ==
        RMD_ERR_DUPLICATE_NAME);
  CHECK(rmd_writer_add_file(w, STR("doc.content"), "missing-file", nullptr) ==
        RMD_ERR_DUPLICATE_NAME);
  OK(rmd_writer_finish(w, nullptr));
  CHECK(rmd_writer_finish(w, nullptr) == RMD_ERR_ARCHIVE_FINISHED);
  CHECK(rmd_writer_add(w, files[0], RMD_STORE, nullptr) ==
        RMD_ERR_ARCHIVE_FINISHED);
  rmd_writer_close(w);
  rmd_buffer expected = sample();
  FILE* f = fopen(path, "rb");
  CHECK(f);
  uint8_t* bytes = malloc(expected.size + 1);
  CHECK(bytes);
  CHECK(fread(bytes, 1, expected.size + 1, f) == expected.size);
  CHECK(memcmp(bytes, expected.bytes, expected.size) == 0);
  fclose(f);
  free(bytes);
  rmd_buffer_free(&expected);
  size_t n = (1u << 20) + 137;
  uint8_t* payload = malloc(n);
  CHECK(payload);
  memset(payload, 'a', n);
  f = rmd_fopen(source, true);
  CHECK(f);
  CHECK(fwrite(payload, 1, n, f) == n);
  CHECK(!fclose(f));
  OK(rmd_writer_open(path, RMD_DEFLATE, RMD_DEVICE, &w, nullptr));
  OK(rmd_writer_add_file(w, STR("doc.pdf"), source, nullptr));
  OK(rmd_writer_add(w, (rmd_file){STR("doc.content"), {payload, n}},
                    RMD_DEFAULT_COMPRESSION, nullptr));
  OK(rmd_writer_add(w, (rmd_file){STR("doc.pagedata"), DATA("x")}, RMD_DEFLATE,
                    nullptr));
  OK(rmd_writer_finish(w, nullptr));
  rmd_writer_close(w);
  rmd_reader* r = nullptr;
  rmd_limits limits = rmd_limits_format();
  OK(rmd_reader_open_file(path, &limits, &r, nullptr));
  CHECK(rmd_reader_entry(r, 0)->method == 0);
  CHECK(rmd_reader_entry(r, 1)->method == 8);
  CHECK(rmd_reader_entry(r, 2)->method == 0);
  rmd_archive a = {0};
  OK(rmd_reader_unpack(r, nullptr, nullptr, &a, nullptr));
  CHECK(a.count == 3);
  for (size_t i = 0; i < 2; i++)
    CHECK(a.files[i].data.size == n &&
          memcmp(a.files[i].data.bytes, payload, n) == 0);
  rmd_reader_close(r);
  rmd_archive_free(&a);
  free(payload);
  OK(rmd_writer_open(path, RMD_STORE, RMD_PACKAGE, &w, nullptr));
  OK(rmd_writer_add(w, (rmd_file){STR("OEBPS/.rels/book.xml"), DATA("x")},
                    RMD_STORE, nullptr));
  OK(rmd_writer_finish(w, nullptr));
  rmd_writer_close(w);
  CHECK(rmd_reader_open_file(path, nullptr, &r, nullptr) ==
        RMD_ERR_INVALID_NAME);
  CHECK(!remove(path));
  CHECK(!remove(source));
  puts("writer: passed");
  return 0;
}
