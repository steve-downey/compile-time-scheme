# -*-CMake-*-
add_library(smdscheme.reader STATIC "")
target_compile_options(smdscheme.reader INTERFACE -freflection)
target_sources(
    smdscheme.reader
    PUBLIC
        FILE_SET smdscheme_reader_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES atom.hpp datum_type.hpp read_datum.hpp
)
target_link_libraries(smdscheme.reader PUBLIC smdscheme.parser)

include(infra/cmake/beman-install-library.cmake OPTIONAL)
if(COMMAND beman_install_library)
    beman_install_library(smdscheme.reader TARGETS smdscheme.reader NAMESPACE smd::)
endif()
