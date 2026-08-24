# src/smd/kit/parser/CMakeLists.txt                                 -*-CMake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_library(kit.parser INTERFACE)
target_sources(
    kit.parser
    INTERFACE
        FILE_SET kit_parser_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES cursor.hpp parse_context.hpp parser.hpp parser_instances.hpp
)
target_link_libraries(kit.parser INTERFACE kit.foundation)

if(SCHEMEPOC_ENABLE_TESTING)
    find_package(Catch2 REQUIRED)

    add_executable(kit_parser_test)
    target_sources(
        kit_parser_test
        PRIVATE
            cursor.test.cpp
            parse_context.test.cpp
            parser.test.cpp
            parser_instances.test.cpp
    )
    target_link_libraries(
        kit_parser_test
        PRIVATE kit.parser Catch2::Catch2WithMain
    )

    include(Catch)
    catch_discover_tests(kit_parser_test)
endif()
