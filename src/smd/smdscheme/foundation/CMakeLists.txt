# src/smd/smdscheme/foundation/CMakeLists.txt                        -*-CMake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_library(smdscheme.foundation STATIC "")
target_compile_options(smdscheme.foundation INTERFACE -freflection)

target_sources(
    smdscheme.foundation
    PUBLIC
        FILE_SET smdscheme_foundation_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES
                version.hpp
                source.hpp
                result.hpp
                static_vector.hpp
                arena_box.hpp
                functor.hpp
                fix.hpp
    PRIVATE version.cpp
)

include(infra/cmake/beman-install-library.cmake OPTIONAL)
if(COMMAND beman_install_library)
    beman_install_library(smdscheme.foundation TARGETS smdscheme.foundation NAMESPACE smd::)
endif()
