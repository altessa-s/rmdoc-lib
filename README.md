# RMDoc (C23)

The `.rmdoc` container: the file a reMarkable tablet and the reMarkable apps
export and import a document as. A C23 reader and writer for its ZIP structure
and opaque file payloads; it does not parse `.rm`, metadata JSON, cloud hashes
or document schemas.

An independent package with no runtime dependency beyond the C standard
library; the deflate codec is vendored. The same container is available for
Swift as [rmdoc-swift](https://github.com/altessa-s/rmdoc-swift); the build and
ownership conventions follow [rmlines-lib](https://github.com/altessa-s/rmlines-lib)
and [rmmarks-lib](https://github.com/altessa-s/rmmarks-lib): CMake, static or
shared library, an explicitly exported C API, counted UTF-8 strings, status
codes and owned results with one release function.

Part of **[Slate Drive](https://slatedrive.app)**
([repository](https://github.com/altessa-s/slatedrive)), the macOS app that
shows a reMarkable library in Finder.

## Build

Requires CMake 3.21+ and a C23 compiler with typed enums, `constexpr`, `nullptr`
and `[[nodiscard]]` (modern Clang or GCC; Windows uses Clang with the MSVC SDK).
The vendored miniz codec is compiled into the library; no system zlib is needed.

```sh
cmake -S . -B build -DRMDOC_WERROR=ON
cmake --build build
cmake --build build --target check
cmake --install build --prefix "$PWD/stage"
```

Use `-DBUILD_SHARED_LIBS=ON` for a shared library. Tests default to ON only when
this is the top-level project. `RMDOC_BUILD_TESTS=OFF` builds just the library.
`RMDOC_BUILD_FUZZERS=ON` requires Clang and its libFuzzer runtime, plus tests.

Installed CMake consumers use:

```cmake
find_package(RMDoc 1 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE RMDoc::rmdoc)
```

Embedding uses `add_subdirectory(path/to/rmdoc-lib)` and the same target.
For other build systems, `pkg-config --cflags --libs --static rmdoc` supplies
headers and linkage. Installation includes licenses and relocatable metadata.

## Read

```c
#include <rmdoc/rmdoc.h>
#include <stdio.h>

static rmd_status show(void *context, const rmd_entry *entry, rmd_bytes data) {
    (void)context;
    printf("%.*s: %zu bytes\n", (int)entry->name.length, entry->name.bytes, data.size);
    return RMD_OK;
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    rmd_reader *reader = nullptr;
    rmd_error error = {0};
    rmd_limits limits = rmd_limits_default();
    limits.maximum_directory_size = 16u << 20;
    limits.maximum_name_length = 1024;
    rmd_status status = rmd_reader_open_file(argv[1], &limits, &reader, &error);
    if (status == RMD_OK)
        status = rmd_reader_foreach(reader, nullptr, nullptr, show, nullptr, &error);
    rmd_reader_close(reader);
    if (status != RMD_OK) fprintf(stderr, "%s: %s\n", rmd_status_string(status), error.message);
    return status == RMD_OK ? 0 : 1;
}
```

`rmd_reader_open` borrows immutable memory. `rmd_reader_open_file` owns a read-only
mapping on POSIX and Windows. Keep the input memory or mapped file unchanged
until `rmd_reader_close`; mappings are not snapshots. Paths are UTF-8, including
on Windows. Returned entry names belong to the reader and are NUL-terminated;
use their explicit byte lengths.

Opening validates the entire directory and every local header without inflating
anything. `rmd_reader_count` / `rmd_reader_entry` list this validated metadata.
`rmd_reader_document_ids` returns unique IDs in first-appearance order, owned by
a `rmd_strings` result. Names are compared using Unicode canonical equivalence.

`rmd_reader_foreach` calls an optional selector for **all** entries first, then
checks aggregate budgets for the selected entries, then verifies CRC and delivers
one payload at a time. Skipped payloads are neither inflated nor checksummed.
Callback payloads are borrowed for the duration of the call only. A nonzero
callback result stops the operation with `RMD_ERR_CALLBACK`. A later payload
failure can occur after earlier callbacks; budget failures always precede them.
Callbacks must not mutate or close their reader or its input.

`rmd_reader_unpack` gathers selected files into an independent owned archive;
release it with `rmd_archive_free`. It may outlive the reader. An empty selection
succeeds. `rmd_pack` writes supplied files into an owned buffer, released with
`rmd_buffer_free`. Output arguments stay unchanged on failure; free an earlier
result before reusing its output variable. Free functions accept NULL or a
zero-initialized result. Do not copy owning structs and free both copies.

## Write

`rmd_pack` produces stored device-layout ZIPs, deterministically: the same
files give the same bytes. Entry order is preserved; DOS time is fixed at
1980-01-01. The archive and every name must be nonempty, names unique, with at
most 65,535 entries and no ZIP64.

For output larger than is convenient to hold in memory:

1. `rmd_writer_open(path, RMD_DEFLATE, RMD_DEVICE, &writer, &error)` opens and
   truncates a staging file.
2. `rmd_writer_add` adds memory; `RMD_DEFAULT_COMPRESSION` uses the writer's mode,
   and `RMD_STORE` / `RMD_DEFLATE` override it for an entry. Deflate is used only
   when smaller than the input.
3. `rmd_writer_add_file` reads a source in 1 MiB chunks. File sources are
   always stored. CRC and sizes are patched into their local headers.
4. `rmd_writer_finish` writes the directory and closes the file. Only after it
   succeeds should the caller promote the staging file.
5. Always `rmd_writer_close` to release the handle and bookkeeping. Closing does
   not implicitly finish or remove partial output.

A failed write poisons the writer; subsequent operations return
`RMD_ERR_ARCHIVE_FINISHED`. Validation/allocation failures before writes are
recoverable. Empty `finish` is recoverable. Each writer is confined to one
caller; independent readers/writers share no mutable global state.

`RMD_PACKAGE` permits deeper and dotted package paths for EPUB/Office ZIP
output. The reader intentionally accepts only device layout. No extraction to
filesystem paths is performed by this library.

## Limits and compatibility

Default size/count ceilings: 65,535 entries, 32-bit entry sizes,
200:1 expansion above 1 MiB, 65,535 UTF-8 bytes per name, and `INT64_MAX` total
and directory budgets. `rmd_limits_format()` uses 1032:1 above zero bytes.
Limits use `uint64_t`; a zero ratio is clamped to one. Listing applies only count,
directory-size and name-length limits. All other budgets apply to selected
payloads. Callers can lower the defaults, as in the example.

The reader rejects encryption, spanning, ZIP64, unsupported compression, local
header disagreement, overlapping regions, invalid descriptors, trailing deflate
bytes and checksum mismatches. Deflate output grows with actual production,
starting at up to 256 KiB, never allocating the declared output size up front.
One spare byte detects streams longer than their declared size.

Path syntax is judged on Unicode scalars: a combining mark after `/`, `.` or
`\` cannot hide it, so a name such as `.` + U+0301 + `hidden` is refused.
Deflate uses miniz on every platform; deflated archives interoperate with other
ZIP readers, but compressed bytes are not promised to match another encoder's.
Running out of memory while compressing is an error, never a silent fallback
to stored data. Diagnostic names in `rmd_error` are truncated to 255 bytes with
the full length recorded; use the status for control flow.

## Verify

`check` runs archive, structure, limits, writer, allocation-failure and 20,000
mutation smoke tests. All ZIP fixtures are generated from original strings and
bytes; no device exports or proprietary assets are included.

```sh
python3 tests/generate-fixtures.py --check
python3 scripts/generate-unicode.py --check
```

The Unicode generator requires Python's Unicode 16.0.0 database; generated tables
are checked in and Python is not needed to build or use the library.

On macOS, an optional cross-check builds the rmdoc-swift sources and compares
the two implementations on error categories, selection, payloads and stored
output bytes:

```sh
cmake -S . -B build-shared -DBUILD_SHARED_LIBS=ON
cmake --build build-shared
python3 tests/compare_swift.py --swift ../rmdoc-swift --library build-shared/librmdoc.dylib
```

This check needs the separate Swift checkout; ordinary builds and tests do not.
CI defines Linux/macOS/Windows builds, shared/static consumers, sanitizers,
libFuzzer, CodeQL and tag-based releases. A local Apple toolchain may lack the
libFuzzer runtime; the deterministic mutation test still runs with sanitizers.

## See also

- **[rmdoc-swift](https://github.com/altessa-s/rmdoc-swift)** — the same
  container as a Swift package.
- **[rmlines-lib](https://github.com/altessa-s/rmlines-lib)** — the `.rm` v6
  page format the container's pages are written in, as a C23 library.
- **[rmlines-swift](https://github.com/altessa-s/rmlines-swift)** and
  **[rmlines-go](https://github.com/altessa-s/rmlines-go)** — the same page
  format for Swift and Go.
- **[rmmarks-lib](https://github.com/altessa-s/rmmarks-lib)** — geometry for
  highlighted text on those pages.
- **[Slate Drive](https://slatedrive.app)**
  ([repository](https://github.com/altessa-s/slatedrive)) — the app these
  packages are part of.

## Acknowledgements

The `.rmdoc` container is not documented by reMarkable. This library is built
on openly available information — the public ZIP specification among it — and
on the research of third-party developers who documented reMarkable's file
formats.

## License

MIT, as in the sibling packages; see [LICENSE](LICENSE). Vendored miniz and
the generated Unicode tables retain their own notices; see
[third-party notices](THIRD-PARTY.md) and the
[Unicode data license](UNICODE-LICENSE.txt).

reMarkable is a trademark of reMarkable AS. This project is not affiliated
with, endorsed by or sponsored by reMarkable AS.
