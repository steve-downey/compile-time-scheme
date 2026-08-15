// src/smd/cl/sender/machine_sender.test.cpp                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/sender/machine_sender.hpp>
#include <smd/cl/sender/machine_sender.hpp> // test 2nd include OK

#include <smd/cl/core/ast.hpp>
#include <smd/cl/elaborator/elaborate.hpp>
#include <smd/cl/eval/builtin.hpp>
#include <smd/cl/eval/outcome.hpp>
#include <smd/cl/eval/value.hpp>
#include <smd/cl/reader/read.hpp>
#include <smd/cl/sender/run_sender.hpp>
#include <smd/cl/sender/sender_v.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <utility>
#include <variant>

using smd::cl::core::core_tree;
using smd::cl::elaborator::elaborate;
using smd::cl::eval::install_builtins;
using smd::cl::eval::limits;
using smd::cl::eval::standard_symbols;
using smd::cl::eval::symbol_table;
using smd::cl::eval::value;
using smd::cl::eval::value_fixnum;
using smd::cl::reader::datum_tree;
using smd::cl::reader::read;
using smd::cl::sender::evaluate;
using smd::cl::sender::run_sender;

namespace sender_v = smd::cl::sender::sender_v;

namespace {

constexpr int datum_nodes = 96;
constexpr int datum_list = 12;
constexpr int core_nodes = 128;
constexpr int core_children = 8;

using table_type = symbol_table<64, 512>;
using form_type = datum_tree<datum_nodes, datum_list>;
using program_type = core_tree<core_nodes, core_children>;

constexpr limits standard_limits{.cells = 256,
                                 .string_chars = 128,
                                 .closures = 8,
                                 .frames = 64,
                                 .call_args = 8,
                                 .steps = 20000};

/// Everything one program owns for the length of a test: its symbol table,
/// its elaborated tree, and the resolved `NIL`/`T` ids the machine needs.
///
/// A `machine_sender` only borrows its program and symbol table (the same
/// caller-owns-the-arena contract `eval::machine` itself has), so connecting
/// one needs somewhere for those to live -- this is that storage, one per
/// compiled program, exactly as two independent top-level forms need two
/// separate machines and heaps.
struct compiled {
    table_type symbols{};
    program_type program{};
    standard_symbols known{};
};

/// Elaborates @p source as one top-level form into a fresh @ref compiled.
///
/// A test helper, not a production path: failing loudly with `REQUIRE`
/// rather than propagating a `result` is the right behaviour for a fixture.
auto compile_source(std::string_view source) -> compiled {
    compiled out;
    auto const installed = install_builtins(out.symbols);
    REQUIRE(installed.has_value());
    out.known = installed.value();
    auto const form = read<datum_nodes, datum_list>(source, out.symbols);
    REQUIRE(form.has_value());
    auto const elaborated =
        elaborate<core_nodes, core_children>(form.value(), out.symbols);
    REQUIRE(elaborated.has_value());
    out.program = elaborated.value();
    return out;
}

} // namespace

// ------------------------------------------------------------------
// D13's three channels, from the sender side
// ------------------------------------------------------------------

TEST_CASE("MachineSenderTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("MachineSenderTest - OrdinaryValueUsesTheValueChannel") {
    compiled prog = compile_source("(+ 2 3)");
    auto const completed = run_sender(
        evaluate<standard_limits>(prog.program, prog.symbols, prog.known));
    REQUIRE(completed.is_value());
    CHECK(completed.as_value() == value{value_fixnum{5}});
}

TEST_CASE("MachineSenderTest - DiagnosedErrorUsesTheErrorChannel") {
    compiled prog = compile_source("(car 2)");
    auto const completed = run_sender(
        evaluate<standard_limits>(prog.program, prog.symbols, prog.known));
    CHECK(completed.is_error());
}

TEST_CASE("MachineSenderTest - EscapedUnwindUsesTheStoppedChannel") {
    // `machine.test.cpp`'s "return-from with no matching block" case
    // (`escaped`): no `block` claims this exit, so `eval::machine::run`
    // itself returns the unmatched unwind. The sender maps that onto
    // `set_stopped`, and `run_sender` -- which cannot recover a stopped
    // completion's payload, because Execution26's `set_stopped` does not
    // carry one -- reports it back as a diagnosed error, same as the
    // pivot's `to_result` reports an unwind reaching the top of a program.
    compiled prog = compile_source("(return-from nowhere 4)");
    auto const completed = run_sender(
        evaluate<standard_limits>(prog.program, prog.symbols, prog.known));
    CHECK(completed.is_error());
}

// ------------------------------------------------------------------
// Structural independence between whole programs, honestly scoped
// ------------------------------------------------------------------

// 84e9ce88-8829-4620-83f2-0b259fca7f3f
TEST_CASE("MachineSenderTest - WhenAllJoinsTwoIndependentPrograms") {
    // What this test is not: a claim about evaluation order inside one
    // program. R7's step brief is explicit that left-to-right evaluation is
    // ANSI's conforming order (3.1.2.1.2.3) the moment an argument can have
    // an effect, so nothing in this backend reorders a call's arguments.
    //
    // What it is: two *separate* top-level programs -- separate symbol
    // tables, separate elaborated trees, separate machines and heaps once
    // connected, no shared mutable state at all -- joined with `when_all`.
    // That is a true structural-independence claim, the one `when_all`
    // honestly supports here, and it is the shape a sender backend adds
    // that `eval::machine::run` alone does not offer: composing whole
    // programs as ordinary sender values.
    compiled first = compile_source("(+ 2 3)");
    compiled second = compile_source("(* 4 5)");

    auto joined = sender_v::then(
        sender_v::when_all(evaluate<standard_limits>(
                               first.program, first.symbols, first.known),
                           evaluate<standard_limits>(
                               second.program, second.symbols, second.known)),
        [](value a, value b) noexcept -> value {
            int const lhs = std::get<value_fixnum>(a).value;
            int const rhs = std::get<value_fixnum>(b).value;
            return value{value_fixnum{lhs + rhs}};
        });

    auto const completed = run_sender(std::move(joined));
    REQUIRE(completed.is_value());
    CHECK(completed.as_value() == value{value_fixnum{25}});
}
// 84e9ce88-8829-4620-83f2-0b259fca7f3f end
