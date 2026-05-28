# -*-CMake-*-
add_library(smdscheme.sender STATIC "")
target_compile_options(smdscheme.sender INTERFACE -freflection)
target_sources(
    smdscheme.sender
    PUBLIC
        FILE_SET smdscheme_sender_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES
                sender_program.hpp
                sender_v.hpp
                sender_cps_program.hpp
                scheme_node_data.hpp
                scheme_tree.hpp
                build_scheme_tree.hpp
                string_writer.hpp
                format_scheme_node.hpp
)
target_link_libraries(
    smdscheme.sender
    PUBLIC smdscheme.closure beman::execution_headers beman.task_headers
)

include(infra/cmake/beman-install-library.cmake OPTIONAL)
if(COMMAND beman_install_library)
    beman_install_library(smdscheme.sender TARGETS smdscheme.sender NAMESPACE smd::)
endif()
