// src/examples/fibonacci_graph.cpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// The Y combinator fibonacci in Scheme — compiled at compile time to a sender:
//
//   ((lambda (fib)
//      (fib fib 5))
//    (lambda (self n)
//      (if (eq? n 0)
//          0
//          (if (eq? n 1)
//              1
//              (+ (self self (+ n -1))
//                 (self self (+ n -2)))))))
//
// The sender execution plan for fib(3) = 2 is a statically-typed composition:
//
//   fib(3) = fib(2) + fib(1)
//          = (fib(1) + fib(0)) + fib(1)
//          = (1      + 0     ) + 1
//
//   then(when_all(then(when_all(just(1), just(0)), add),
//                 just(1)),
//        add)

#include <smd/smdscheme/sender/dump_scheme_plan.hpp>
#include <smd/smdscheme/sender/sender_v.hpp>

#include <iostream>

namespace sv = smd::smdscheme::sender_v;
namespace s  = smd::smdscheme::sender;

int main() {
    // Static sender type for fib(3):
    //   then( when_all( then( when_all(just(1), just(0)), add ),   // fib(2)
    //                   just(1) ),                                  // fib(1)
    //         add )
    using Fib3 = decltype(sv::then(
        sv::when_all(
            sv::then(sv::when_all(sv::just(1), sv::just(0)),
                     [](int a, int b) { return a + b; }),
            sv::just(1)),
        [](int a, int b) { return a + b; }));

    std::cout << "// Sender execution plan: fib(3) = 2\n\n";
    s::dump_scheme_plan<Fib3>(std::cout);
    return 0;
}
