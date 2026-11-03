/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */
#include "fixtures.h"
#include "test.h"
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);
static uint32_t state = 0x524d444f;
static uint32_t random32(void) {
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}
int main(void) {
  uint8_t buffer[8192];
  for (size_t iteration = 0; iteration < 20000; iteration++) {
    size_t which = random32() % (sizeof fixtures / sizeof *fixtures),
           n = fixtures[which].size;
    CHECK(n < sizeof buffer);
    memcpy(buffer, fixtures[which].data, n);
    if (iteration % 4 == 0)
      n = random32() % (n + 1);
    else
      for (size_t j = 0; j < 1 + iteration % 8; j++)
        buffer[random32() % n] ^= (uint8_t)random32();
    CHECK(LLVMFuzzerTestOneInput(buffer, n) == 0);
  }
  puts("fuzz smoke: 20000 inputs passed");
  return 0;
}
