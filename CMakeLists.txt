# Copyright 2026 ALTESSA SOLUTIONS INC.
# Use of this source code is governed by an MIT-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.21)

# The version lives in the public header and nowhere else: read out of it
# here rather than written a second time, so that a release is one edit and the
# library cannot report a number its package does not carry.
foreach(part MAJOR MINOR PATCH)
    file(STRINGS include/rmdoc/rmdoc.h line
         REGEX "^#define RMDOC_VERSION_${part} [0-9]+$")
    if(NOT line MATCHES "^#define RMDOC_VERSION_${part} ([0-9]+)$")
        message(FATAL_ERROR "RMDOC_VERSION_${part} not found in include/rmdoc/rmdoc.h")
    endif()
    set(RMDOC_VERSION_${part} "${CMAKE_MATCH_1}")
endforeach()

project(rmdoc
    VERSION ${RMDOC_VERSION_MAJOR}.${RMDOC_VERSION_MINOR}.${RMDOC_VERSION_PATCH}
    DESCRIPTION "Reader and writer for reMarkable document containers"
    HOMEPAGE_URL "https://github.com/altessa-s/rmdoc-lib"
    LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(RMDOC_TOP_LEVEL ON)
else()
    set(RMDOC_TOP_LEVEL OFF)
endif()

option(RMDOC_BUILD_TESTS "Build the test suite" ${RMDOC_TOP_LEVEL})
option(RMDOC_WERROR "Turn compiler warnings into errors" OFF)
option(RMDOC_BUILD_FUZZERS "Build the libFuzzer targets (clang only)" OFF)
if(RMDOC_BUILD_FUZZERS AND NOT RMDOC_BUILD_TESTS)
    message(FATAL_ERROR "RMDOC_BUILD_FUZZERS requires RMDOC_BUILD_TESTS")
endif()

# ---------------------------------------------------------------------------
# The library
# ---------------------------------------------------------------------------

set(RMDOC_SOURCES src/rmd_support.c src/rmd_unicode.c src/rmd_unicode_data.c src/rmd_reader.c
    src/rmd_writer.c src/rmd_codec.c src/rmd_io.c src/rmd_miniz.c)
set(RMDOC_PUBLIC_HEADERS include/rmdoc/rmdoc.h include/rmdoc/export.h)
# Keep upstream miniz unmodified; strict warnings cover our code.
set_source_files_properties(src/rmd_miniz.c PROPERTIES COMPILE_OPTIONS "-w")

# Honours BUILD_SHARED_LIBS: static by default, shared with
# -DBUILD_SHARED_LIBS=ON. One target either way, so the exported CMake package
# and the pkg-config file describe whatever was built.
add_library(rmdoc ${RMDOC_SOURCES})
add_library(RMDoc::rmdoc ALIAS rmdoc)

target_include_directories(rmdoc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

set_target_properties(rmdoc PROPERTIES
    C_STANDARD 23
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    # ZIP parsing, the codec and Unicode helpers are not part of
    # the contract and must not reach a consumer's symbol table.
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${RMDOC_PUBLIC_HEADERS}")

target_compile_features(rmdoc PUBLIC c_std_23)
if(BUILD_SHARED_LIBS)
    target_compile_definitions(rmdoc PRIVATE RMDOC_BUILDING_SHARED
                                      INTERFACE RMDOC_USING_SHARED)
endif()

# The warning set, and it is one set: the library is built with clang or
# gcc on every platform, Windows included. MSVC's C compiler stops at the
# first `constexpr`, `nullptr`, `[[nodiscard]]` and typed enum — all of them
# C23, all of them throughout — and a build that carries a second warning
# dialect for a compiler that cannot read the code is a build with a branch
# nobody can exercise.
target_compile_options(rmdoc PRIVATE
    -Wall -Wextra -Wshadow -Wconversion -Wsign-conversion
    -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wcast-qual)
if(RMDOC_WERROR)
    target_compile_options(rmdoc PRIVATE -Werror)
endif()
# ---------------------------------------------------------------------------
# The tests
# ---------------------------------------------------------------------------

if(RMDOC_BUILD_TESTS)
    enable_testing()

    # Internal tests exercise allocation failures and UTF-8 validation.
    # Compile a test-only copy to keep fault-injection hooks out of the ABI.
    add_library(rmdoc_testing STATIC EXCLUDE_FROM_ALL ${RMDOC_SOURCES})
    target_compile_definitions(rmdoc_testing PUBLIC RMDOC_TESTING)
    target_include_directories(rmdoc_testing PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set_target_properties(rmdoc_testing PROPERTIES
        C_STANDARD 23 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)

    foreach(suite archive structure limits writer errors fuzz_smoke)
        add_executable(test_${suite} EXCLUDE_FROM_ALL tests/test_${suite}.c)
        target_link_libraries(test_${suite} PRIVATE rmdoc_testing)
        target_include_directories(test_${suite} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
        set_target_properties(test_${suite} PROPERTIES
            C_STANDARD 23 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
        add_test(NAME ${suite} COMMAND test_${suite})
    endforeach()

    target_sources(test_fuzz_smoke PRIVATE tests/fuzz_doc.c)
    set_tests_properties(archive structure limits writer errors fuzz_smoke PROPERTIES TIMEOUT 30)

    if(RMDOC_BUILD_FUZZERS)
        if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
            message(FATAL_ERROR "libFuzzer requires clang")
        endif()
        add_executable(fuzz_doc tests/fuzz_doc.c)
        target_link_libraries(fuzz_doc PRIVATE rmdoc_testing)
        target_compile_options(fuzz_doc PRIVATE -fsanitize=fuzzer)
        target_link_options(fuzz_doc PRIVATE -fsanitize=fuzzer)
        set_target_properties(fuzz_doc PROPERTIES C_STANDARD 23 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    endif()

    # `ctest` builds nothing on its own, and a suite that was never compiled is
    # a suite that silently did not run.
    add_custom_target(build_tests DEPENDS test_archive test_structure test_limits
        test_writer test_errors test_fuzz_smoke)
    # Under a multi-configuration generator — Xcode, Visual Studio — a
    # `ctest` that is not told which configuration to run finds no tests at
    # all and says so five times, so the configuration is passed along. Passed
    # conditionally: under a single-configuration generator with no build type
    # set, `$<CONFIG>` is empty, and `-C ""` is an error of its own. The
    # expression yields the two arguments or nothing, and COMMAND_EXPAND_LISTS
    # is what lets "nothing" vanish rather than arrive as an empty argument.
    add_custom_target(check
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
                "$<$<BOOL:$<CONFIG>>:-C;$<CONFIG>>"
        DEPENDS build_tests
        COMMAND_EXPAND_LISTS)
endif()

# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------

install(TARGETS rmdoc
    EXPORT RMDocTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rmdoc)


install(EXPORT RMDocTargets
    FILE RMDocTargets.cmake
    NAMESPACE RMDoc::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/RMDoc)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RMDocConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/RMDocConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/RMDoc)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/RMDocConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/RMDocConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/RMDocConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/RMDoc)

# pkg-config, for the builds that never learned CMake.
#
# The way back from the `.pc` to the prefix is worked out here rather than
# written as a fixed `../..`: CMAKE_INSTALL_LIBDIR is `lib` on one
# distribution, `lib64` on another and `lib/x86_64-linux-gnu` on a third, and
# only one of those is two levels down.
file(RELATIVE_PATH RMDOC_PC_PREFIX_RELATIVE
     "${CMAKE_INSTALL_FULL_LIBDIR}/pkgconfig" "${CMAKE_INSTALL_PREFIX}")
if(RMDOC_PC_PREFIX_RELATIVE STREQUAL "")
    set(RMDOC_PC_PREFIX_RELATIVE ".")
endif()
string(REGEX REPLACE "/$" "" RMDOC_PC_PREFIX_RELATIVE "${RMDOC_PC_PREFIX_RELATIVE}")

set(RMDOC_PC_LIBS_PRIVATE "")
set(RMDOC_PC_CFLAGS "")
if(WIN32 AND BUILD_SHARED_LIBS)
    set(RMDOC_PC_CFLAGS "-DRMDOC_USING_SHARED")
endif()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/rmdoc.pc.in
               ${CMAKE_CURRENT_BINARY_DIR}/rmdoc.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/rmdoc.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

install(FILES LICENSE COPYRIGHT UNICODE-LICENSE.txt
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/rmdoc)
install(FILES vendor/miniz/LICENSE.txt
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/rmdoc/miniz)
