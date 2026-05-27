add_subdirectory(foundation)
add_subdirectory(parser)
add_subdirectory(reader)
add_subdirectory(elaborator)
add_subdirectory(closure)
add_subdirectory(sender)
add_subdirectory(reflection)

add_library(smdscheme.smdscheme STATIC "")
target_compile_options(smdscheme.smdscheme INTERFACE -freflection)
target_sources(
    smdscheme.smdscheme
    PUBLIC
        FILE_SET smdscheme_smdscheme_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES smdscheme.hpp
    PRIVATE smdscheme.cpp
)
target_link_libraries(
    smdscheme.smdscheme
    PUBLIC smdscheme.sender smdscheme.reflection
)

find_package(Catch2 REQUIRED)
if(SCHEMEPOC_ENABLE_TESTING)
    add_executable(smdscheme_test smdscheme.test.cpp)
    target_link_libraries(
        smdscheme_test
        PRIVATE smdscheme.smdscheme Catch2::Catch2WithMain
    )
    target_compile_options(smdscheme_test PRIVATE -freflection)

    # Grab all .test.cpp files dynamically for simplicity, or enumerate:
    file(
        GLOB_RECURSE TEST_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/*test.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*test.cpp"
    )
    target_sources(smdscheme_test PRIVATE ${TEST_FILES})

    include(Catch)
    catch_discover_tests(smdscheme_test)
endif()
