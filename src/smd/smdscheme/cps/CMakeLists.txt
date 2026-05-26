# -*-CMake-*-
add_library(smdscheme.cps STATIC "")
target_compile_options(smdscheme.cps INTERFACE -freflection)
target_sources(
    smdscheme.cps
    PUBLIC
        FILE_SET smdscheme_cps_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES fix.hpp cps.hpp
)
target_link_libraries(smdscheme.cps PUBLIC smdscheme.elaborator)

include(infra/cmake/beman-install-library.cmake OPTIONAL)
if(COMMAND beman_install_library)
    beman_install_library(smdscheme.cps TARGETS smdscheme.cps NAMESPACE smd::)
endif()
