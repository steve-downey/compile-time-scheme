include(GNUInstallDirs)

add_executable(hello)

# Add example source file.
target_sources(hello PRIVATE hello.cpp)

# Link example with the library.
target_link_libraries(hello PRIVATE schemepoc.schemepoc)

install(
    TARGETS hello
    COMPONENT schemepoc.schemepoc.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_executable(godbolt_arithmetic)
target_sources(godbolt_arithmetic PRIVATE godbolt_arithmetic.cpp)
target_link_libraries(godbolt_arithmetic PRIVATE schemepoc.schemepoc)

install(
    TARGETS godbolt_arithmetic
    COMPONENT schemepoc.schemepoc.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_executable(godbolt_lambda)
target_sources(godbolt_lambda PRIVATE godbolt_lambda.cpp)
target_link_libraries(godbolt_lambda PRIVATE schemepoc.schemepoc)

install(
    TARGETS godbolt_lambda
    COMPONENT schemepoc.schemepoc.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_executable(godbolt_ffi)
target_sources(godbolt_ffi PRIVATE godbolt_ffi.cpp)
target_link_libraries(godbolt_ffi PRIVATE schemepoc.schemepoc)

install(
    TARGETS godbolt_ffi
    COMPONENT schemepoc.schemepoc.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_test(NAME examples_hello COMMAND hello)
set_tests_properties(
    examples_hello
    PROPERTIES PASS_REGULAR_EXPRESSION "schemepoc v0.1.0"
)

add_test(NAME examples_godbolt_arithmetic COMMAND godbolt_arithmetic)
set_tests_properties(
    examples_godbolt_arithmetic
    PROPERTIES PASS_REGULAR_EXPRESSION "7"
)

add_test(NAME examples_godbolt_lambda COMMAND godbolt_lambda)
set_tests_properties(
    examples_godbolt_lambda
    PROPERTIES PASS_REGULAR_EXPRESSION "2"
)

add_test(NAME examples_godbolt_ffi COMMAND godbolt_ffi)
set_tests_properties(
    examples_godbolt_ffi
    PROPERTIES
        PASS_REGULAR_EXPRESSION "FFI Called with: 2026 and 10.*Result: 2036"
)
