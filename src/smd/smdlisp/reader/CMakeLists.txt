# src/smd/smdlisp/reader/CMakeLists.txt                             -*-CMake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_library(smdlisp.reader STATIC "")
target_sources(
    smdlisp.reader
    PUBLIC
        FILE_SET smdlisp_reader_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES atom.hpp cl_chars.hpp
)
target_link_libraries(smdlisp.reader PUBLIC smdscheme.parser)
