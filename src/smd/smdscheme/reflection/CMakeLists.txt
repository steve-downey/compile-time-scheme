# -*-CMake-*-
add_library(smdscheme.reflection STATIC "")
target_compile_options(smdscheme.reflection INTERFACE -freflection)
target_sources(
    smdscheme.reflection
    PUBLIC
        FILE_SET smdscheme_reflection_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES reflection_reify.hpp
)
target_link_libraries(smdscheme.reflection PUBLIC smdscheme.foundation)

include(infra/cmake/beman-install-library.cmake OPTIONAL)
if(COMMAND beman_install_library)
    beman_install_library(smdscheme.reflection TARGETS smdscheme.reflection NAMESPACE smd::)
endif()
