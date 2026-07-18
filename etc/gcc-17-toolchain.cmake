# etc/gcc-17-toolchain.cmake                                        -*-cmake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/gcc-flags.cmake")

set(CMAKE_C_COMPILER gcc-17)
set(CMAKE_CXX_COMPILER g++-17)
set(GCOV_EXECUTABLE "gcov-17" CACHE STRING "GCOV executable" FORCE)

get_filename_component(RPATH "~/.local/lib64" ABSOLUTE)

set(CMAKE_EXE_LINKER_FLAGS
    "-Wl,-rpath,${RPATH}"
    CACHE STRING
    "CMAKE_EXE_LINKER_FLAGS"
    FORCE
)

set(CMAKE_CXX_FLAGS_ASAN
    "${CMAKE_CXX_FLAGS_ASAN} -Wno-maybe-uninitialized"
    CACHE STRING
    "C++ ASAN Flags"
    FORCE
)
