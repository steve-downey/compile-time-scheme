include(GNUInstallDirs)

target_sources(
    schemepoc.schemepoc
    PUBLIC FILE_SET schemepoc_schemepoc_headers TYPE HEADERS FILES schemepoc.hpp
    PRIVATE schemepoc.cpp
)

## Tests
if(SCHEMEPOC_ENABLE_TESTING AND NOT GTest_FOUND)
    find_package(GTest QUIET)
    if(GTest_FOUND)
        include(GoogleTest)
    else()
        message(
            WARNING
            "
No provider for GTest. Unable to build tests.
Consider using CMAKE_PROJECT_TOP_LEVEL_INCLUDES=./cmake/use-fetch-content.cmake as documented in the README.md
"
        )
    endif()
endif()

if(SCHEMEPOC_ENABLE_TESTING)
    add_executable(schemepoc_test)

    target_sources(schemepoc_test PRIVATE schemepoc.test.cpp)

    target_link_libraries(schemepoc_test schemepoc.schemepoc)
    target_link_libraries(schemepoc_test GTest::gtest)
    target_link_libraries(schemepoc_test GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(schemepoc_test)
endif()
