# src/smd/fixpoint/CMakeLists.txt                               -*-CMake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_library(smd.fixpoint INTERFACE)
target_sources(
    smd.fixpoint
    PUBLIC
        FILE_SET smd_fixpoint_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES overloaded.hpp fix.hpp box.hpp recursion_schemes.hpp
)

find_package(Catch2 REQUIRED)
if(SCHEMEPOC_ENABLE_TESTING)
    add_executable(fixpoint_test)
    target_link_libraries(
        fixpoint_test
        PRIVATE smd.fixpoint Catch2::Catch2WithMain
    )

    file(GLOB TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.t.cpp")
    target_sources(fixpoint_test PRIVATE ${TEST_FILES})

    include(Catch)
    catch_discover_tests(fixpoint_test)
endif()
