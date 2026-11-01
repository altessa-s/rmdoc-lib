/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */
#include <rmdoc/rmdoc.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
static rmd_status consume(void *ctx, const rmd_entry *e, rmd_bytes b) {
    (void)ctx;
    (void)e;
    (void)b;
    return RMD_OK;
}
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    rmd_limits limits = rmd_limits_default();
    limits.maximum_entry_count = 128;
    limits.maximum_directory_size = 1u << 20;
    limits.maximum_name_length = 4096;
    limits.maximum_entry_size = 1u << 20;
    limits.maximum_total_size = 2u << 20;
    limits.maximum_compressed_total_size = 2u << 20;
    rmd_reader *r = nullptr;
    if (rmd_reader_open((rmd_bytes){data, size}, &limits, &r, nullptr) == RMD_OK) {
        rmd_status ignored = rmd_reader_foreach(r, nullptr, nullptr, consume, nullptr, nullptr);
        (void)ignored;
        rmd_reader_close(r);
    }
    return 0;
}
