# src/smd/cl/printer/CMakeLists.txt                                -*-CMake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_library(cl.printer INTERFACE)
target_sources(
    cl.printer
    INTERFACE
        FILE_SET cl_printer_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES
                prin1.hpp
)
target_link_libraries(cl.printer INTERFACE cl.foundation cl.reader cl.symbol)

if(SCHEMEPOC_ENABLE_TESTING)
    find_package(Catch2 REQUIRED)

    add_executable(cl_printer_test)
    target_sources(
        cl_printer_test
        PRIVATE
            prin1.test.cpp
    )
    target_link_libraries(
        cl_printer_test
        PRIVATE cl.printer Catch2::Catch2WithMain
    )

    include(Catch)
    catch_discover_tests(cl_printer_test)
endif()
