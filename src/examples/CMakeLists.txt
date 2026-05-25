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
