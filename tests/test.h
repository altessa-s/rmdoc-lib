/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */
#ifndef RMD_TEST_H
#define RMD_TEST_H
#include "rmd_internal.h"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                                \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
#define OK(x) CHECK((x) == RMD_OK)
#define STR(x) ((rmd_str){x, sizeof(x) - 1})
#define DATA(x) ((rmd_bytes){(const uint8_t *)(x), sizeof(x) - 1})
static inline rmd_buffer sample(void) {
    const rmd_file files[] = {{STR("doc.content"), DATA("{\"pages\":[]}")},
                              {STR("doc/page.rm"), DATA("ink")},
                              {STR("other.metadata"), DATA("")}};
    rmd_buffer b = {0};
    OK(rmd_pack(files, 3, &b, nullptr));
    return b;
}
static inline rmd_reader *open_buffer(rmd_buffer b, const rmd_limits *limits) {
    rmd_reader *r = nullptr;
    OK(rmd_reader_open((rmd_bytes){b.bytes, b.size}, limits, &r, nullptr));
    return r;
}
static inline rmd_status discard(void *context, const rmd_entry *entry, rmd_bytes b) {
    (void)entry;
    (void)b;
    if (context)
        (*(size_t *)context)++;
    return RMD_OK;
}
static inline rmd_status skip(void *ctx, const rmd_entry *entry, bool *include) {
    (void)entry;
    if (ctx)
        (*(size_t *)ctx)++;
    *include = false;
    return RMD_OK;
}
static inline rmd_status inspect(rmd_buffer b, bool unpack) {
    rmd_reader *r = nullptr;
    rmd_status s = rmd_reader_open((rmd_bytes){b.bytes, b.size}, nullptr, &r, nullptr);
    if (s == RMD_OK && unpack)
        s = rmd_reader_foreach(r, nullptr, nullptr, discard, nullptr, nullptr);
    rmd_reader_close(r);
    return s;
}
#endif
