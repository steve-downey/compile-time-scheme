# src/smd/cl/foundation/CMakeLists.txt                             -*-CMake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_library(cl.foundation INTERFACE)
target_sources(
    cl.foundation
    INTERFACE
        FILE_SET cl_foundation_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES
                alternative.hpp
                applicative.hpp
                arena_box.hpp
                fold_left_short.hpp
                foldable.hpp
                functor.hpp
                identity.hpp
                monoid.hpp
                node_f.hpp
                parse_error.hpp
                result.hpp
                result_instances.hpp
                source_pos.hpp
                source_span.hpp
                static_vector.hpp
                static_vector_instances.hpp
                tagged_tree.hpp
                tagged_tree_instances.hpp
                tagged_tree_schemes.hpp
                topo_fold.hpp
                trampoline.hpp
                traversable.hpp
)
target_link_libraries(cl.foundation INTERFACE kit.foundation)

if(SCHEMEPOC_ENABLE_TESTING)
    find_package(Catch2 REQUIRED)

    add_executable(cl_foundation_test)
    target_sources(
        cl_foundation_test
        PRIVATE
            alternative.test.cpp
            applicative.test.cpp
            arena_box.test.cpp
            fold_left_short.test.cpp
            foldable.test.cpp
            functor.test.cpp
            identity.test.cpp
            monoid.test.cpp
            node_f.test.cpp
            parse_error.test.cpp
            result.test.cpp
            result_instances.test.cpp
            source_pos.test.cpp
            source_span.test.cpp
            static_vector.test.cpp
            static_vector_instances.test.cpp
            tagged_tree.test.cpp
            tagged_tree_instances.test.cpp
            tagged_tree_schemes.test.cpp
            topo_fold.test.cpp
            trampoline.test.cpp
            traversable.test.cpp
    )
    target_link_libraries(
        cl_foundation_test
        PRIVATE cl.foundation Catch2::Catch2WithMain
    )

    include(Catch)
    catch_discover_tests(cl_foundation_test)
endif()
