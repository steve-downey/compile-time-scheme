// testinstall/test.cpp                                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <iostream>
#include <smd/schemepoc/schemepoc.hpp>

int main() {
    std::cout << "schemepoc: |" << schemepoc::schemepoc() << '|' << '\n';
    return 0;
}
