# -*-CMake-*-
add_library(smdscheme.parser STATIC "")
target_compile_options(smdscheme.parser INTERFACE -freflection)
target_sources(
    smdscheme.parser
    PUBLIC
        FILE_SET smdscheme_parser_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES
                parser.hpp
                parser_alternative.hpp
                parser_ops.hpp
                cursor.hpp
)
target_link_libraries(smdscheme.parser PUBLIC smdscheme.foundation)

include(infra/cmake/beman-install-library.cmake OPTIONAL)
if(COMMAND beman_install_library)
    beman_install_library(smdscheme.parser TARGETS smdscheme.parser NAMESPACE smd::)
endif()
