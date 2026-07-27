// src/examples/lisp_sender_graph_demo.cpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Runs a Common Lisp `catch`/`throw`/`unwind-protect` program through the
// `smdlisp` sender backend, then renders the sender graph its
// `unwind-protect` form builds as Graphviz DOT.
//
// The program is `cleanup_order_src` from the backend's own test suite: a
// `throw` crossing two nested `unwind-protect` frames on its way to a
// `catch`. It witnesses that cleanups run innermost-first (log = 12, not 21)
// while the thrown value still reaches the catch.

#include <smd/smdlisp/sender/dump_lisp_plan.hpp>
#include <smd/smdlisp/sender/sender_program.hpp>

#include <iostream>
#include <string_view>
#include <variant>

namespace lisp = smd::smdlisp;
namespace scm = smd::smdscheme;
namespace snd = smd::smdlisp::sender;

namespace {

constexpr int MaxNodes = 128;
constexpr int MaxList = 16;
constexpr int MaxBindings = 16;
constexpr int MaxEnvs = 32;

using Core = lisp::elaborator::core_type<MaxNodes, MaxList>;
using Val = lisp::closure::value<Core>;

constexpr std::string_view program =
    "(let ((log 0))"
    "  (catch 'c"
    "    (unwind-protect"
    "      (unwind-protect (throw 'c 99) (setq log (+ (* log 10) 1)))"
    "      (setq log (+ (* log 10) 2))))"
    "  log)";

/// A stand-in for one evaluated sub-form, so the graph's type can be named
/// outside an evaluation context.
auto const thunk = [] { return snd::make_value<Core>(Val{1}); };

/// The sender graph the evaluator builds for the program's nested
/// `unwind-protect` pair.
///
/// These are the same two factories `sender_eval.hpp`'s `core_unwind_protect`
/// arm calls, in the same nesting, with the recursive `child(...)` calls
/// replaced by a constant thunk.
using NestedUnwindPlan = decltype(snd::unwind_protect<Core>(
    snd::unwind_protect<Core>(snd::defer_outcome<Core>(thunk), thunk), thunk));

} // namespace

int main() {
    scm::foundation::tree_arena<lisp::reader::datum_type<MaxNodes, MaxList>,
                                MaxNodes>
        datum_arena;
    lisp::closure::pair_heap<Core, lisp::closure::default_max_pairs> heap;
    lisp::closure::store<Core, lisp::closure::default_max_store> store;
    lisp::closure::env_arena<Core, MaxBindings, MaxEnvs> envs;

    auto pr = snd::compile_to_sender<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
        program, datum_arena);
    if (!pr.has_value()) {
        std::cerr << "compile failed: " << pr.error().message << '\n';
        return 1;
    }
    auto const &prog = pr.value();
    auto environment =
        lisp::closure::default_env<Core, MaxBindings>(heap, store);
    auto const result = prog.run(environment, envs);

    std::cout << "// smdlisp sender backend\n";
    std::cout << "// program: " << program << '\n';
    if (result.is_value() && std::holds_alternative<int>(result.val)) {
        std::cout << "// cleanup log (innermost first): "
                  << std::get<int>(result.val) << '\n';
    } else {
        std::cout << "// program did not produce an integer value\n";
    }
    std::cout << "// Pipe to: dot -Tpng -o plan.png\n\n";

    snd::dump_lisp_plan<NestedUnwindPlan>(std::cout);
    return 0;
}
