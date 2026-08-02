# src/smd/cl/core/CMakeLists.txt                                   -*-CMake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_library(cl.core INTERFACE)
target_sources(
    cl.core
    INTERFACE
        FILE_SET cl_core_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES ast.hpp
)
target_link_libraries(cl.core INTERFACE cl.foundation cl.symbol)

if(SCHEMEPOC_ENABLE_TESTING)
    find_package(Catch2 REQUIRED)

    add_executable(cl_core_test)
    target_sources(cl_core_test PRIVATE ast.test.cpp)
    target_link_libraries(cl_core_test PRIVATE cl.core Catch2::Catch2WithMain)

    include(Catch)
    catch_discover_tests(cl_core_test)
endif()
