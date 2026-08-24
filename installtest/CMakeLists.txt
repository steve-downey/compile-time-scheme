# installtest/CMakeLists.txt                                        -*-CMake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 4.0)
project(TestInstalledSchemepoc)

# Toolchain sets C++26; don't override it here.

enable_testing()

find_package(
    schemepoc.schemepoc
    REQUIRED
    PATHS "${CMAKE_PREFIX_PATH}/lib/cmake/schemepoc.schemepoc/"
    NO_DEFAULT_PATH
)

# The add_installtest helper and its six calls used to live here, all
# linking against the now-dead-ended reader tree (A3 removes it). Install-test
# coverage against kit::foundation/cl.* is real, positive work, tracked as
# docs/backlog/BL-0005-export-live-trees-and-turn-testinstall-green.md.

set(CMAKE_SKIP_TEST_ALL_DEPENDENCY FALSE)
