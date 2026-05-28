include(GNUInstallDirs)

add_executable(hello)

# Add example source file.
target_sources(hello PRIVATE hello.cpp)

# Link example with the library.
target_link_libraries(hello PRIVATE smdscheme.smdscheme)

install(
    TARGETS hello
    COMPONENT smdscheme.smdscheme.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_executable(godbolt_arithmetic)
target_sources(godbolt_arithmetic PRIVATE godbolt_arithmetic.cpp)
target_link_libraries(godbolt_arithmetic PRIVATE smdscheme.smdscheme)

install(
    TARGETS godbolt_arithmetic
    COMPONENT smdscheme.smdscheme.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_executable(godbolt_lambda)
target_sources(godbolt_lambda PRIVATE godbolt_lambda.cpp)
target_link_libraries(godbolt_lambda PRIVATE smdscheme.smdscheme)

install(
    TARGETS godbolt_lambda
    COMPONENT smdscheme.smdscheme.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_executable(godbolt_ffi)
target_sources(godbolt_ffi PRIVATE godbolt_ffi.cpp)
target_link_libraries(godbolt_ffi PRIVATE smdscheme.smdscheme)

install(
    TARGETS godbolt_ffi
    COMPONENT smdscheme.smdscheme.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_test(NAME examples_hello COMMAND hello)
set_tests_properties(
    examples_hello
    PROPERTIES PASS_REGULAR_EXPRESSION "smdscheme v0.1.0"
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

add_executable(advanced_reflection_ffi)
target_sources(advanced_reflection_ffi PRIVATE advanced_reflection_ffi.cpp)
target_link_libraries(advanced_reflection_ffi PRIVATE smdscheme.smdscheme)
target_compile_options(advanced_reflection_ffi PRIVATE -freflection)

install(
    TARGETS advanced_reflection_ffi
    COMPONENT smdscheme.smdscheme.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_test(NAME examples_advanced_reflection_ffi COMMAND advanced_reflection_ffi)
set_tests_properties(
    examples_advanced_reflection_ffi
    PROPERTIES
        PASS_REGULAR_EXPRESSION
            "Scheme executing evaluation step for n = 6.*Final Scheme Result Output: 720"
)

add_executable(advanced_sender_ffi)
target_sources(advanced_sender_ffi PRIVATE advanced_sender_ffi.cpp)
target_link_libraries(advanced_sender_ffi PRIVATE smdscheme.smdscheme)
target_compile_options(advanced_sender_ffi PRIVATE -freflection)

install(
    TARGETS advanced_sender_ffi
    COMPONENT smdscheme.smdscheme.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_test(NAME examples_advanced_sender_ffi COMMAND advanced_sender_ffi)
set_tests_properties(
    examples_advanced_sender_ffi
    PROPERTIES PASS_REGULAR_EXPRESSION "Final Scheme Result Output: 5"
)

add_executable(sender_graph_demo)
target_sources(sender_graph_demo PRIVATE sender_graph_demo.cpp)
target_link_libraries(sender_graph_demo PRIVATE smdscheme.sender)
target_compile_options(sender_graph_demo PRIVATE -freflection)

install(
    TARGETS sender_graph_demo
    COMPONENT smdscheme.smdscheme.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_test(NAME examples_sender_graph_demo COMMAND sender_graph_demo)
set_tests_properties(
    examples_sender_graph_demo
    PROPERTIES PASS_REGULAR_EXPRESSION "digraph SchemeExecutionPlan"
)

add_executable(fibonacci_graph)
target_sources(fibonacci_graph PRIVATE fibonacci_graph.cpp)
target_link_libraries(fibonacci_graph PRIVATE smdscheme.sender)
target_compile_options(fibonacci_graph PRIVATE -freflection)

install(
    TARGETS fibonacci_graph
    COMPONENT smdscheme.smdscheme.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_test(NAME examples_fibonacci_graph COMMAND fibonacci_graph)
set_tests_properties(
    examples_fibonacci_graph
    PROPERTIES PASS_REGULAR_EXPRESSION "digraph SchemeExecutionPlan"
)
