include(GNUInstallDirs)

target_sources(
    schemepoc.schemepoc
    PUBLIC
        FILE_SET schemepoc_schemepoc_headers
            TYPE HEADERS
            FILES
                schemepoc.hpp
                version.hpp
                source.hpp
                result.hpp
                static_vector.hpp
    PRIVATE schemepoc.cpp version.cpp
)

## Tests
if(SCHEMEPOC_ENABLE_TESTING AND NOT Catch2_FOUND)
    find_package(Catch2 REQUIRED)
    if(Catch2_FOUND)
        include(Catch)
    else()
        message(
            WARNING
            "
No provider for Catch2. Unable to build tests.
Consider using CMAKE_PROJECT_TOP_LEVEL_INCLUDES=./cmake/use-fetch-content.cmake as documented in the README.md
"
        )
    endif()
endif()

if(SCHEMEPOC_ENABLE_TESTING)
    add_executable(schemepoc_test)

    target_sources(
        schemepoc_test
        PRIVATE
            schemepoc.test.cpp
            version.test.cpp
            source.test.cpp
            result.test.cpp
            static_vector.test.cpp
    )

    target_link_libraries(schemepoc_test PRIVATE schemepoc.schemepoc)
    target_link_libraries(schemepoc_test PRIVATE Catch2::Catch2WithMain)

    include(Catch)
    catch_discover_tests(schemepoc_test)
endif()
