# -*-CMake-*-
add_library(smdscheme.elaborator STATIC "")
target_compile_options(smdscheme.elaborator INTERFACE -freflection)
target_sources(
    smdscheme.elaborator
    PUBLIC
        FILE_SET smdscheme_elaborator_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES elaborated_core.hpp elaborate.hpp
)
target_link_libraries(smdscheme.elaborator PUBLIC smdscheme.reader)
