/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include "test.h"

int main(void) {
  CHECK(strcmp(rmd_version(), "1.0.0") == 0);
  rmd_buffer b = sample();
  rmd_reader* r = open_buffer(b, nullptr);
  CHECK(rmd_reader_count(r) == 3);
  CHECK(!rmd_reader_entry(r, 3));
  const rmd_entry* entry = rmd_reader_entry(r, 0);
  CHECK(entry->uncompressed_size == 12);
  rmd_strings ids = {0};
  OK(rmd_reader_document_ids(r, &ids, nullptr));
  CHECK(ids.count == 2);
  CHECK(ids.values[0].length == 3);
  CHECK(memcmp(ids.values[1].bytes, "other", 5) == 0);
  rmd_archive a = {0};
  OK(rmd_reader_unpack(r, nullptr, nullptr, &a, nullptr));
  CHECK(a.count == 3);
  rmd_buffer packed = {0};
  OK(rmd_pack(a.files, a.count, &packed, nullptr));
  CHECK(b.size == packed.size && memcmp(b.bytes, packed.bytes, b.size) == 0);
  rmd_reader_close(r);
  rmd_buffer_free(&b);
  CHECK(memcmp(a.files[1].data.bytes, "ink", 3) == 0);
  rmd_archive_free(&a);
  rmd_buffer_free(&packed);
  rmd_strings_free(&ids);
  rmd_archive_free(&a);
  rmd_strings_free(&ids);
  const char* bad[] = {"",
                       "../x",
                       ".\xcc\x81hidden",
                       "a/\xcc\x81\x62/c",
                       "a\\\xcc\x81\x62",
                       "a/b/c",
                       "a//b",
                       "/a",
                       "a/",
                       "a\\b",
                       ".a",
                       "a\x7f",
                       "a\xc2\x85",
                       "a\xe2\x80\xae",
                       "a\xf3\xa0\x80\x81",
                       "\xc0\xaf",
                       "\xed\xa0\x80"};
  for (size_t i = 0; i < sizeof bad / sizeof *bad; i++)
    CHECK(rmd_validate_name((rmd_str){bad[i], strlen(bad[i])}, RMD_DEVICE,
                            nullptr) == RMD_ERR_INVALID_NAME);
  OK(rmd_validate_name(STR("OEBPS/.rels/doc.xml"), RMD_PACKAGE, nullptr));
  CHECK(rmd_validate_name(STR("a/../x"), RMD_PACKAGE, nullptr) ==
        RMD_ERR_INVALID_NAME);
  OK(rmd_validate_name(STR("a/./x"), RMD_PACKAGE, nullptr));
  rmd_str id = rmd_document_id(STR("a.b.c"));
  CHECK(id.length == 1);
  id = rmd_document_id(STR("a.b/c.rm"));
  CHECK(id.length == 3);
  rmd_file duplicates[] = {{STR("é.rm"), DATA("x")},
                           {STR("e\xcc\x81.rm"), DATA("y")}};
  CHECK(rmd_pack(duplicates, 2, &b, nullptr) == RMD_ERR_DUPLICATE_NAME);
  duplicates[0].name = STR("가.rm");
  duplicates[1].name = STR("가.rm");
  CHECK(rmd_pack(duplicates, 2, &b, nullptr) == RMD_ERR_DUPLICATE_NAME);
  /* The full 16-bit count is legal, not automatically a ZIP64 sentinel. */
  constexpr size_t count = UINT16_MAX;
  rmd_file* many = calloc(count, sizeof *many);
  char (*names)[24] = calloc(count, sizeof *names);
  CHECK(many && names);
  for (size_t i = 0; i < count; i++) {
    int n = snprintf(names[i], sizeof names[i], "doc%05zu.content", i);
    CHECK(n > 0);
    many[i].name = (rmd_str){names[i], (size_t)n};
  }
  OK(rmd_pack(many, count, &b, nullptr));
  r = open_buffer(b, nullptr);
  CHECK(rmd_reader_count(r) == count);
  rmd_reader_close(r);
  rmd_buffer_free(&b);
  free(many);
  free(names);
  puts("archive: passed");
  return 0;
}
