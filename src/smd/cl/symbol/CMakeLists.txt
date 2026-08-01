# src/smd/cl/symbol/CMakeLists.txt                                 -*-CMake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_library(cl.symbol INTERFACE)
target_sources(
    cl.symbol
    INTERFACE
        FILE_SET cl_symbol_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES symbol_id.hpp symbol_table.hpp
)
target_link_libraries(cl.symbol INTERFACE cl.foundation)

if(SCHEMEPOC_ENABLE_TESTING)
    find_package(Catch2 REQUIRED)

    add_executable(cl_symbol_test)
    target_sources(
        cl_symbol_test
        PRIVATE symbol_id.test.cpp symbol_table.test.cpp
    )
    target_link_libraries(
        cl_symbol_test
        PRIVATE cl.symbol Catch2::Catch2WithMain
    )

    include(Catch)
    catch_discover_tests(cl_symbol_test)
endif()
