# src/smd/smdlisp/CMakeLists.txt                                    -*-CMake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_subdirectory(closure)

add_library(smdlisp.smdlisp STATIC "")
target_sources(
    smdlisp.smdlisp
    PUBLIC
        FILE_SET smdlisp_smdlisp_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES version.hpp
    PRIVATE version.cpp
)
target_link_libraries(
    smdlisp.smdlisp
    PUBLIC smdscheme.foundation smdscheme.parser smdlisp.closure
)

find_package(Catch2 REQUIRED)
if(SCHEMEPOC_ENABLE_TESTING)
    add_executable(smdlisp_test)
    target_link_libraries(
        smdlisp_test
        PRIVATE smdlisp.smdlisp Catch2::Catch2WithMain
    )

    file(GLOB_RECURSE TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*test.cpp")
    target_sources(smdlisp_test PRIVATE ${TEST_FILES})

    include(Catch)
    catch_discover_tests(smdlisp_test)
endif()
