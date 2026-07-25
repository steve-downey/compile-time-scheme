# src/smd/smdlisp/macroexpand/CMakeLists.txt                       -*-CMake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_library(smdlisp.macroexpand STATIC "")
target_sources(
    smdlisp.macroexpand
    PUBLIC
        FILE_SET smdlisp_macroexpand_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES expander.hpp
)
target_link_libraries(
    smdlisp.macroexpand
    PUBLIC
        smdscheme.foundation
        smdlisp.reader
        smdlisp.elaborator
        smdlisp.closure
        smd.fixpoint
)
