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

# Helper to register an installtest executable.
# All tests link to smd::smdscheme so they transitively pick up the
# -freflection flag and the full sub-library include tree.
function(add_installtest name src)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE smd::smdscheme)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_installtest(test_smdscheme test.cpp)
add_installtest(test_foundation test_foundation.cpp)
add_installtest(test_reader test_reader.cpp)
add_installtest(test_elaborator test_elaborator.cpp)
add_installtest(test_closure test_closure.cpp)
add_installtest(test_sender test_sender.cpp)

set(CMAKE_SKIP_TEST_ALL_DEPENDENCY FALSE)
