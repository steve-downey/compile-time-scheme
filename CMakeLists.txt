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
include(GNUInstallDirs)

# Install vendored beman execution and task headers so sender targets are usable
# from the installed package without requiring a separate beman installation.
install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/vendor/execution/include/beman"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT schemepoc.schemepoc_Development
)
install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/vendor/task/include/beman"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT schemepoc.schemepoc_Development
)

beman_install_library(
    schemepoc.schemepoc
    TARGETS
        smdscheme.smdscheme
        smdscheme.sender
        smdscheme.reflection
        smdscheme.closure
        smdscheme.elaborator
        smdscheme.reader
        smdscheme.parser
        smdscheme.foundation
        smd.fixpoint
    NAMESPACE smd::
)

# Coverage
configure_file("cmake/gcovr.cfg.in" gcovr.cfg @ONLY)

add_custom_target(
    process_coverage
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running gcovr to process coverage results"
    COMMAND mkdir -p coverage
    COMMAND gcovr --config gcovr.cfg .
)
