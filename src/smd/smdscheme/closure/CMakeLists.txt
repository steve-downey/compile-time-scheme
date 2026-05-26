# -*-CMake-*-
add_library(smdscheme.closure STATIC "")
target_compile_options(smdscheme.closure INTERFACE -freflection)
target_sources(
    smdscheme.closure
    PUBLIC
        FILE_SET smdscheme_closure_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES value.hpp closure_backend.hpp
)
target_link_libraries(smdscheme.closure PUBLIC smdscheme.cps)

include(infra/cmake/beman-install-library.cmake OPTIONAL)
if(COMMAND beman_install_library)
    beman_install_library(smdscheme.closure TARGETS smdscheme.closure NAMESPACE smd::)
endif()
