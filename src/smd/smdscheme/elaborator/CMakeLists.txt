# -*-CMake-*-
add_library(smdscheme.elaborator STATIC "")
target_compile_options(smdscheme.elaborator INTERFACE -freflection)
target_sources(
    smdscheme.elaborator
    PUBLIC
        FILE_SET smdscheme_elaborator_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES elaborator_core.hpp elaborator.hpp eval_direct.hpp
)
target_link_libraries(smdscheme.elaborator PUBLIC smdscheme.reader)

include(infra/cmake/beman-install-library.cmake OPTIONAL)
if(COMMAND beman_install_library)
    beman_install_library(smdscheme.elaborator TARGETS smdscheme.elaborator NAMESPACE smd::)
endif()
