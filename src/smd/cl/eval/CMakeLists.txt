# src/smd/cl/eval/CMakeLists.txt                                   -*-CMake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_library(cl.eval INTERFACE)
target_sources(
    cl.eval
    INTERFACE
        FILE_SET cl_eval_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES
                builtin.hpp
                environment.hpp
                heap.hpp
                machine.hpp
                outcome.hpp
                value.hpp
)
target_link_libraries(cl.eval INTERFACE cl.core cl.foundation cl.symbol)

if(SCHEMEPOC_ENABLE_TESTING)
    find_package(Catch2 REQUIRED)

    add_executable(cl_eval_test)
    target_sources(
        cl_eval_test
        PRIVATE
            builtin.test.cpp
            environment.test.cpp
            heap.test.cpp
            machine.test.cpp
            outcome.test.cpp
            value.test.cpp
    )
    target_link_libraries(
        cl_eval_test
        PRIVATE cl.elaborator cl.eval cl.reader Catch2::Catch2WithMain
    )

    include(Catch)
    catch_discover_tests(cl_eval_test)
endif()
