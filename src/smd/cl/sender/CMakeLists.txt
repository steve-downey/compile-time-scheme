# src/smd/cl/sender/CMakeLists.txt                                 -*-CMake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_library(cl.sender INTERFACE)
target_sources(
    cl.sender
    INTERFACE
        FILE_SET cl_sender_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES machine_sender.hpp run_sender.hpp sender_v.hpp
)
target_link_libraries(
    cl.sender
    INTERFACE cl.eval $<BUILD_INTERFACE:beman::execution_headers>
)

if(SCHEMEPOC_ENABLE_TESTING)
    find_package(Catch2 REQUIRED)

    add_executable(cl_sender_test)
    target_sources(
        cl_sender_test
        PRIVATE machine_sender.test.cpp run_sender.test.cpp
    )
    target_link_libraries(
        cl_sender_test
        PRIVATE cl.elaborator cl.eval cl.reader cl.sender Catch2::Catch2WithMain
    )

    include(Catch)
    catch_discover_tests(cl_sender_test)
endif()
