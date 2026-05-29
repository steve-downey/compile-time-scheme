# -*-CMake-*-
add_library(smdscheme.parser STATIC "")
target_compile_options(smdscheme.parser INTERFACE -freflection)
target_sources(
    smdscheme.parser
    PUBLIC
        FILE_SET smdscheme_parser_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES parser.hpp alt.hpp parser_ops.hpp cursor.hpp
)
target_link_libraries(smdscheme.parser PUBLIC smdscheme.foundation)
