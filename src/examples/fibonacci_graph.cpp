// src/examples/fibonacci_graph.cpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Y combinator fibonacci compiled at compile time, evaluated via sender_cps.
// The sender_cps evaluator drives execution through just/then/when_all
// senders; dump_scheme_plan reflects on the static sender type to produce
// the Graphviz DOT execution plan.

#include <smd/smdscheme/sender/dump_scheme_plan.hpp>
#include <smd/smdscheme/sender/sender_cps_program.hpp>
#include <smd/smdscheme/sender/sender_v.hpp>

#include <iostream>
#include <span>

namespace scm = smd::smdscheme;
namespace sv = smd::smdscheme::sender_v;
namespace s = smd::smdscheme::sender;
namespace cps = smd::smdscheme::sender_cps;

using Core = scm::elaborator::core_type<512, 16>;
using Val = scm::closure::value<Core>;
using Res = scm::foundation::result<Val>;

constexpr auto fibonacci_source = R"(
  ((lambda (fib)
     (fib fib 5))
   (lambda (self n)
     (if (eq? n 0)
         0
         (if (eq? n 1)
             1
             (+ (self self (+ n -1))
                (self self (+ n -2))))))))
)";

constexpr auto ffi_eq(std::span<Val const> args) -> Res {
    if (args.size() != 2)
        return scm::foundation::parse_error{{}, "arity eq?"};
    if (!std::holds_alternative<int>(args[0]) ||
        !std::holds_alternative<int>(args[1]))
        return scm::foundation::parse_error{{}, "type err eq?"};
    return Val{std::get<int>(args[0]) == std::get<int>(args[1])};
}

// Compiled at compile time — no evaluation yet.
constexpr auto program =
    cps::compile_sender_cps<512, 16>(fibonacci_source).value();

int main() {
    // Evaluate at runtime via the sender_cps stack (just/then/when_all
    // internally, no beman::task type erasure needed).
    auto env = scm::closure::default_env<Core, 16>();
    env.define("eq?", Val{scm::closure::foreign_function<Core>{ffi_eq}});

    auto result = program(env);
    if (result.has_value() && std::holds_alternative<int>(result.value()))
        std::cout << "fib(5) = " << std::get<int>(result.value()) << "\n\n";

    // The sender_cps evaluator drives computation through this sender
    // composition pattern.  dump_scheme_plan reflects on the static type
    // to show the execution plan for fib(3) = fib(2) + fib(1):
    //
    //   then( when_all( then( when_all(just(1), just(0)), add ),  // fib(2)
    //                   just(1) ),                                 // fib(1)
    //         add )
    using Fib3 = decltype(sv::then(
        sv::when_all(sv::then(sv::when_all(sv::just(1), sv::just(0)),
                              [](int a, int b) { return a + b; }),
                     sv::just(1)),
        [](int a, int b) { return a + b; }));

    std::cout << "// Sender execution plan: fib(3) = 2\n\n";
    s::dump_scheme_plan<Fib3>(std::cout);
    return 0;
}
