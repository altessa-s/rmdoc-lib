/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */
#include "fixtures.h"
#include "test.h"
int main(void) {
    for (size_t i = 0; i < sizeof fixtures / sizeof *fixtures; i++) {
        rmd_buffer b = {(uint8_t *)(uintptr_t)fixtures[i].data, fixtures[i].size};
        rmd_status actual = inspect(b, true);
        if (actual != fixtures[i].expected)
            fprintf(stderr, "fixture %s got %s\n", fixtures[i].name, rmd_status_string(actual));
        CHECK(actual == fixtures[i].expected);
    }
    rmd_buffer b = sample();
    size_t n = b.size;
    for (size_t i = 0; i < n; i++) {
        b.size = i;
        CHECK(inspect(b, true) != RMD_OK);
    }
    b.size = n;
    size_t central = rmd_u32(b.bytes + n - 6);
    struct mutation {
        size_t at;
        uint32_t value;
        int width;
        rmd_status expected;
    } changes[] = {{central + 8, 1, 2, RMD_ERR_ENCRYPTED_ENTRY},
                   {central + 10, 99, 2, RMD_ERR_UNSUPPORTED_COMPRESSION},
                   {central + 34, 1, 2, RMD_ERR_SPANNED_ARCHIVE},
                   {central + 42, UINT32_MAX, 4, RMD_ERR_MALFORMED_ARCHIVE},
                   {6, 8, 2, RMD_ERR_MALFORMED_ARCHIVE},
                   {8, 8, 2, RMD_ERR_MALFORMED_ARCHIVE},
                   {14, 123, 4, RMD_ERR_MALFORMED_ARCHIVE},
                   {18, 123, 4, RMD_ERR_MALFORMED_ARCHIVE},
                   {n - 18, 1, 2, RMD_ERR_SPANNED_ARCHIVE},
                   {n - 12, 2, 2, RMD_ERR_SPANNED_ARCHIVE},
                   {n - 6, UINT32_MAX, 4, RMD_ERR_MALFORMED_ARCHIVE}};
    for (size_t i = 0; i < sizeof changes / sizeof *changes; i++) {
        struct mutation m = changes[i];
        uint8_t old[4];
        memcpy(old, b.bytes + m.at, (size_t)m.width);
        if (m.width == 2)
            rmd_put16(b.bytes + m.at, (uint16_t)m.value);
        else
            rmd_put32(b.bytes + m.at, m.value);
        CHECK(inspect(b, false) == m.expected);
        memcpy(b.bytes + m.at, old, (size_t)m.width);
    }
    b.bytes[30 + strlen("doc.content")] ^= 1;
    CHECK(inspect(b, false) == RMD_OK);
    CHECK(inspect(b, true) == RMD_ERR_CHECKSUM_MISMATCH);
    rmd_reader *r = open_buffer(b, nullptr);
    size_t selected = 0;
    OK(rmd_reader_foreach(r, skip, &selected, discard, nullptr, nullptr));
    CHECK(selected == 3);
    rmd_reader_close(r);
    rmd_buffer_free(&b);
    puts("structure: passed");
    return 0;
}
