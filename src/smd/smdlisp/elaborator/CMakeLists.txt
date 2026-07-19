# src/smd/smdlisp/elaborator/CMakeLists.txt                        -*-CMake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_library(smdlisp.elaborator STATIC "")
target_sources(
    smdlisp.elaborator
    PUBLIC
        FILE_SET smdlisp_elaborator_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES elaborated_core.hpp elaborate.hpp
)
target_link_libraries(smdlisp.elaborator PUBLIC smdscheme.foundation smdlisp.reader)
