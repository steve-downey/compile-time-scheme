# src/smd/cl/reader/CMakeLists.txt                                 -*-CMake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_library(cl.reader INTERFACE)
target_sources(
    cl.reader
    INTERFACE
        FILE_SET cl_reader_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES
                cursor.hpp
                datum.hpp
                number.hpp
                read.hpp
                readtable.hpp
                token.hpp
)
target_link_libraries(cl.reader INTERFACE cl.foundation cl.symbol)

if(SCHEMEPOC_ENABLE_TESTING)
    find_package(Catch2 REQUIRED)

    add_executable(cl_reader_test)
    target_sources(
        cl_reader_test
        PRIVATE
            cursor.test.cpp
            datum.test.cpp
            number.test.cpp
            read.test.cpp
            readtable.test.cpp
            token.test.cpp
    )
    target_link_libraries(
        cl_reader_test
        PRIVATE cl.reader Catch2::Catch2WithMain
    )

    include(Catch)
    catch_discover_tests(cl_reader_test)
endif()
