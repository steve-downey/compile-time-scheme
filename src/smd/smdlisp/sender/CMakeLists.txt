# src/smd/smdlisp/sender/CMakeLists.txt                            -*-CMake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_library(smdlisp.sender STATIC "")
target_compile_options(smdlisp.sender INTERFACE -freflection)
target_sources(
    smdlisp.sender
    PUBLIC
        FILE_SET smdlisp_sender_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES
                sender_v.hpp
                outcome.hpp
                defer_outcome.hpp
                unwind_protect.hpp
                sender_eval.hpp
                sender_program.hpp
                lisp_plan_tree.hpp
                build_lisp_tree.hpp
                dump_lisp_plan.hpp
)
target_link_libraries(
    smdlisp.sender
    PUBLIC
        smdscheme.foundation
        smdscheme.sender
        smdlisp.closure
        smdlisp.elaborator
        smd.fixpoint
        $<BUILD_INTERFACE:beman::execution_headers>
)
target_include_directories(
    smdlisp.sender
    PUBLIC $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
