/* Copyright 2026 ALTESSA SOLUTIONS INC.
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file. */

#include <rmdoc/rmdoc.h>
#include <stdio.h>

int main(void) {
  puts(rmd_version());
  return rmd_limits_default().maximum_entry_count == 65535 ? 0 : 1;
}
