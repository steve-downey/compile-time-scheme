# src/smd/cl/conformance/CMakeLists.txt                            -*-CMake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_library(cl.conformance INTERFACE)
target_sources(
    cl.conformance
    INTERFACE
        FILE_SET cl_conformance_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES corpus.hpp driver.hpp sbcl_oracle.hpp
)
target_link_libraries(
    cl.conformance
    INTERFACE
        cl.core
        cl.elaborator
        cl.eval
        cl.foundation
        cl.printer
        cl.reader
        cl.symbol
)

if(SCHEMEPOC_ENABLE_TESTING)
    find_package(Catch2 REQUIRED)

    add_executable(cl_conformance_test)
    target_sources(
        cl_conformance_test
        PRIVATE
            corpus.test.cpp
            driver.test.cpp
            reader_differential.test.cpp
            sbcl_differential.test.cpp
    )
    target_link_libraries(
        cl_conformance_test
        PRIVATE cl.conformance Catch2::Catch2WithMain
    )

    include(Catch)
    catch_discover_tests(cl_conformance_test)
endif()
