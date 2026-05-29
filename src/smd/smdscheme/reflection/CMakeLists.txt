# -*-CMake-*-
add_library(smdscheme.reflection STATIC "")
target_compile_options(smdscheme.reflection INTERFACE -freflection)
target_sources(
    smdscheme.reflection
    PUBLIC
        FILE_SET smdscheme_reflection_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES reified_environment.hpp
)
target_link_libraries(smdscheme.reflection PUBLIC smdscheme.foundation)
