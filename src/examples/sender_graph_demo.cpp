// src/examples/sender_graph_demo.cpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/sender/dump_scheme_plan.hpp>
#include <smd/smdscheme/sender/sender_v.hpp>

#include <iostream>

int main() {
    // Demonstrate DOT output for a composed sender expression.
    // This shows the execution plan for: then(when_all(just(1), just(2)), add)
    using Sender = decltype(smd::smdscheme::sender_v::then(
        smd::smdscheme::sender_v::when_all(smd::smdscheme::sender_v::just(1),
                                           smd::smdscheme::sender_v::just(2)),
        [](int a, int b) { return a + b; }));

    std::cout << "// Sender execution plan for: (+ 1 2)\n";
    std::cout << "// Pipe to: dot -Tpng -o plan.png\n\n";
    smd::smdscheme::sender::dump_scheme_plan<Sender>(std::cout);
    return 0;
}
