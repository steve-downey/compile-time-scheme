# src/smd/smdlisp/closure/CMakeLists.txt                            -*-CMake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_library(smdlisp.closure STATIC "")
target_sources(
    smdlisp.closure
    PUBLIC
        FILE_SET smdlisp_closure_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES value.hpp pairs.hpp env.hpp eval_direct.hpp
)
target_link_libraries(
    smdlisp.closure
    PUBLIC smdscheme.foundation smdlisp.elaborator smd.fixpoint
)
