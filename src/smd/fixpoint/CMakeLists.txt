# src/smd/fixpoint/CMakeLists.txt                               -*-CMake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_library(smd.fixpoint INTERFACE)
target_sources(
    smd.fixpoint
    PUBLIC
        FILE_SET smd_fixpoint_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES overloaded.hpp fix.hpp box.hpp recursion_schemes.hpp
)
