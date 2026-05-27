// src/examples/hello.cpp                                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/foundation/version.hpp>

#include <print>

namespace scm = smd::smdscheme;

// 710c39c6-c7e1-403f-a9f0-9f8ecf890dc9
int main() {
    std::println("smdscheme v{}.{}.{}",
                 smd::smdscheme::foundation::version_major,
                 smd::smdscheme::foundation::version_minor,
                 smd::smdscheme::foundation::version_patch);
}
// 710c39c6-c7e1-403f-a9f0-9f8ecf890dc9 end
