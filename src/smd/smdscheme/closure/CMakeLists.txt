# -*-CMake-*-
add_library(smdscheme.closure STATIC "")
target_compile_options(smdscheme.closure INTERFACE -freflection)
target_sources(
    smdscheme.closure
    PUBLIC
        FILE_SET smdscheme_closure_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES value.hpp cps_code.hpp closure_program.hpp eval_direct.hpp
)
target_link_libraries(smdscheme.closure PUBLIC smdscheme.elaborator smd.fixpoint)
