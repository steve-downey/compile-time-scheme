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
                arena_box.hpp
                reader_cursor.hpp
                parser.hpp
                parser_alternative.hpp
                reader_atom.hpp
                datum_tree.hpp
                reader.hpp
                elaborator.hpp
                value.hpp
                eval_direct.hpp
                parser_ops.hpp
                fix.hpp
                functor.hpp
                cps.hpp
                closure_backend.hpp
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
            arena_box.test.cpp
            reader_cursor.test.cpp
            parser.test.cpp
            parser_alternative.test.cpp
            reader_atom.test.cpp
            datum_tree.test.cpp
            reader.test.cpp
            elaborator.test.cpp
            value.test.cpp
            eval_direct.test.cpp
            parser_ops.test.cpp
            fix.test.cpp
            cps_arithmetic.test.cpp
            cps.test.cpp
            closure_backend.test.cpp
    )

    target_link_libraries(schemepoc_test PRIVATE schemepoc.schemepoc)
    target_link_libraries(schemepoc_test PRIVATE Catch2::Catch2WithMain)

    include(Catch)
    catch_discover_tests(schemepoc_test)
endif()

if(SCHEMEPOC_ENABLE_TESTING)
    # Negative Compile Test: source_literal rejects non-string-literal usage
    add_library(
        test_neg_source_literal
        OBJECT
        EXCLUDE_FROM_ALL
        test_neg_source_literal.cpp
    )
    target_link_libraries(test_neg_source_literal PRIVATE schemepoc.schemepoc)
    add_test(
        NAME NegativeTest.SourceLiteralRejectsNonString
        COMMAND
            ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target
            test_neg_source_literal --config $<CONFIG>
    )
    set_tests_properties(
        NegativeTest.SourceLiteralRejectsNonString
        PROPERTIES
            PASS_REGULAR_EXPRESSION "class template argument deduction failed"
    )

    # Negative Compile Test: static_vector overflow in consteval context
    add_library(
        test_neg_static_vector
        OBJECT
        EXCLUDE_FROM_ALL
        test_neg_static_vector.cpp
    )
    target_link_libraries(test_neg_static_vector PRIVATE schemepoc.schemepoc)
    add_test(
        NAME NegativeTest.StaticVectorOverflow
        COMMAND
            ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target
            test_neg_static_vector --config $<CONFIG>
    )
    set_tests_properties(
        NegativeTest.StaticVectorOverflow
        PROPERTIES
            PASS_REGULAR_EXPRESSION
                "size_ < Capacity|in ‘constexpr’ expansion of"
    )

    # Negative Compile Test: parser combinator misuse produces specific concept diagnostic
    add_library(
        test_neg_parser_concept
        OBJECT
        EXCLUDE_FROM_ALL
        test_neg_parser_concept.cpp
    )
    target_link_libraries(test_neg_parser_concept PRIVATE schemepoc.schemepoc)
    add_test(
        NAME NegativeTest.ParserConceptMisuse
        COMMAND
            ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target
            test_neg_parser_concept --config $<CONFIG>
    )
    set_tests_properties(
        NegativeTest.ParserConceptMisuse
        PROPERTIES
            PASS_REGULAR_EXPRESSION "constraints not satisfied|parser_like"
    )
endif()
