# src/smd/smdscheme/foundation/CMakeLists.txt                        -*-CMake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_library(smdscheme.foundation STATIC "")
target_compile_options(smdscheme.foundation INTERFACE -freflection)

target_sources(
    smdscheme.foundation
    PUBLIC
        FILE_SET smdscheme_foundation_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES
                version.hpp
                source_pos.hpp
                source_span.hpp
                parse_error.hpp
                result.hpp
                static_vector.hpp
                arena_box.hpp
                functor.hpp
                applicative.hpp
                alternative.hpp
                fix.hpp
    PRIVATE version.cpp
)
