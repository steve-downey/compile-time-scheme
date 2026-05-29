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
                dump_scheme_plan.hpp
)
target_link_libraries(
    smdscheme.sender
    PUBLIC
        smdscheme.closure
        $<BUILD_INTERFACE:beman::execution_headers>
        $<BUILD_INTERFACE:beman.task_headers>
)
target_include_directories(
    smdscheme.sender
    PUBLIC $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
