# src/smd/smdscheme/foundation/CMakeLists.txt                        -*-CMake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_library(smdscheme.foundation STATIC "")
target_compile_options(smdscheme.foundation INTERFACE -freflection)

target_sources(
    smdscheme.foundation
    PUBLIC
        FILE_SET smdscheme_foundation_headers
            TYPE HEADERS
            FILES
                version.hpp
                source.hpp
                result.hpp
                static_vector.hpp
                arena_box.hpp
    PRIVATE version.cpp
)
