// src/examples/hello.cpp                                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/version.hpp>

#include <print>

// 710c39c6-c7e1-403f-a9f0-9f8ecf890dc9
int main() {
    std::println("schemepoc v{}.{}.{}", smd::schemepoc::version_major,
                 smd::schemepoc::version_minor, smd::schemepoc::version_patch);
}
// 710c39c6-c7e1-403f-a9f0-9f8ecf890dc9 end
