# example/CMakeLists.txt                                            -*-cmake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 4.0..4.2)

project(schemepoc VERSION 0.0.0 LANGUAGES CXX)

# Instrumentation query
# find_package(Python COMPONENTS Interpreter)
cmake_instrumentation(
  API_VERSION 1
  DATA_VERSION 1
  HOOKS postGenerate preBuild postBuild preCMakeBuild postCMakeBuild postCMakeInstall postCTest
  OPTIONS trace
  CALLBACK ${CMAKE_SOURCE_DIR}/scripts/instrument.sh
)

# Includes
include(CTest)
include(FetchContent)

option(
    SCHEMEPOC_ENABLE_TESTING
    "Enable building tests and test infrastructure"
    ${PROJECT_IS_TOP_LEVEL}
)

set(CMAKE_VERIFY_INTERFACE_HEADER_SETS ON)

# Create the library target and named header set for beman.optional
add_library(schemepoc.schemepoc STATIC "")

target_sources(
    schemepoc.schemepoc
    PUBLIC FILE_SET schemepoc_schemepoc_headers TYPE HEADERS BASE_DIRS src
)

if(SCHEMEPOC_ENABLE_TESTING)
    find_package(Catch2 3 REQUIRED)
    if(Catch2_FOUND)
        include(Catch)
    else()
        message(
            WARNING
            "
No provider for Catch2. Unable to build tests.
Consider using CMAKE_PROJECT_TOP_LEVEL_INCLUDES=./cmake/use-fetch-content.cmake as documented in the README.md
"
        )
    endif()
endif()

set(BEMAN_USE_MODULES OFF CACHE BOOL "Disable modules" FORCE)
set(BEMAN_TASK_BUILD_EXAMPLES OFF CACHE BOOL "Disable task examples" FORCE)
set(BEMAN_TASK_BUILD_TESTS OFF CACHE BOOL "Disable task tests" FORCE)

# Prevent vendor/task from refetching execution and let it load execution
set(FETCHCONTENT_SOURCE_DIR_EXECUTION
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/execution
    CACHE PATH
    ""
    FORCE
)
add_subdirectory(vendor/task EXCLUDE_FROM_ALL)

add_subdirectory(src)

include(infra/cmake/beman-install-library.cmake)
beman_install_library(schemepoc.schemepoc TARGETS schemepoc.schemepoc NAMESPACE smd::)

# Coverage
configure_file("cmake/gcovr.cfg.in" gcovr.cfg @ONLY)

add_custom_target(
    process_coverage
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running gcovr to process coverage results"
    COMMAND mkdir -p coverage
    COMMAND gcovr --config gcovr.cfg .
)
