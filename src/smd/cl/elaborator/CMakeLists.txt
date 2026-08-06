# src/smd/cl/elaborator/CMakeLists.txt                             -*-CMake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_library(cl.elaborator INTERFACE)
target_sources(
    cl.elaborator
    INTERFACE
        FILE_SET cl_elaborator_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES elaborate.hpp
)
target_link_libraries(
    cl.elaborator
    INTERFACE cl.core cl.foundation cl.reader cl.symbol
)

if(SCHEMEPOC_ENABLE_TESTING)
    find_package(Catch2 REQUIRED)

    add_executable(cl_elaborator_test)
    target_sources(cl_elaborator_test PRIVATE elaborate.test.cpp)
    target_link_libraries(
        cl_elaborator_test
        PRIVATE cl.elaborator Catch2::Catch2WithMain
    )

    include(Catch)
    catch_discover_tests(cl_elaborator_test)
endif()
