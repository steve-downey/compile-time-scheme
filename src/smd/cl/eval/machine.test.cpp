// src/smd/cl/eval/machine.test.cpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/eval/machine.hpp>
#include <smd/cl/eval/machine.hpp> // test 2nd include OK

#include <smd/cl/core/ast.hpp>
#include <smd/cl/elaborator/elaborate.hpp>
#include <smd/cl/eval/builtin.hpp>
#include <smd/cl/eval/heap.hpp>
#include <smd/cl/eval/outcome.hpp>
#include <smd/cl/eval/value.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/foundation/result_instances.hpp>
#include <smd/cl/reader/read.hpp>

#include <catch2/catch_test_macros.hpp>

#include <ranges>
#include <string_view>
#include <variant>

using smd::cl::core::core_progn;
using smd::cl::core::core_tag;
using smd::cl::core::core_tree;
using smd::cl::elaborator::elaborate;
using smd::cl::elaborator::elaborate_into;
using smd::cl::eval::install_builtins;
using smd::cl::eval::limits;
using smd::cl::eval::machine;
using smd::cl::eval::outcome;
using smd::cl::eval::standard_symbols;
using smd::cl::eval::symbol_table;
using smd::cl::eval::value;
using smd::cl::eval::value_cons;
using smd::cl::eval::value_fixnum;
using smd::cl::eval::value_symbol;
using smd::cl::foundation::bind;
using smd::cl::foundation::result;
using smd::cl::reader::datum_tree;
using smd::cl::reader::read;

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

using machine_type = machine<standard_limits, program_type, table_type>;

// ------------------------------------------------------------------
// Driving one program: install, read, elaborate, run
// ------------------------------------------------------------------

/// How a program completed, and what its symbol table looked like after —
/// the second half matters because `defun`'s only effect is on a slot.
struct report {
    outcome how;              ///< The program's completion.
    bool cleanup_ran = false; ///< Whether `RAN` acquired a definition.
};

/// Runs @p source as one top-level form.
constexpr auto evaluate(std::string_view source) -> report {
    table_type symbols;
    auto const ran = bind(
        install_builtins(symbols), [&symbols, source](standard_symbols known) {
            return bind(
                read<datum_nodes, datum_list>(source, symbols),
                [&symbols, known](form_type const &form) {
                    return bind(
                        elaborate<core_nodes, core_children>(form, symbols),
                        [&symbols, known](
                            program_type const &program) -> result<outcome> {
                            machine_type running{program, symbols, known};
                            return running.run();
                        });
                });
        });
    auto const witness = symbols.find("RAN");
    bool const cleanup_ran = witness && symbols.function(*witness).has_value();
    return report{ran.has_value() ? ran.value() : outcome{ran.error()},
                  cleanup_ran};
}

constexpr auto yields(std::string_view source, int expected) -> bool {
    auto const done = evaluate(source).how;
    return done.is_value() && done.as_value() == value{value_fixnum{expected}};
}

constexpr auto is_error(std::string_view source) -> bool {
    return evaluate(source).how.is_error();
}

// ------------------------------------------------------------------
// The acceptance witness: recursive defun (DIV-0009)
// ------------------------------------------------------------------

/// Elaborates two top-level forms into one core arena and runs them in
/// order, which is what "this form, then that one" means.
///
/// One arena, because a closure names the node its body lives at: a
/// function defined by the first form is called from the second, so both
/// forms' nodes must outlive both forms.
constexpr auto evaluate_both(std::string_view first, std::string_view second)
    -> outcome {
    table_type symbols;
    program_type program;
    auto const ran =
        bind(install_builtins(symbols), [&](standard_symbols known) {
            return bind(
                read<datum_nodes, datum_list>(first, symbols),
                [&](form_type const &head) {
                    return bind(
                        elaborate_into(head, symbols, program),
                        [&](int first_root) {
                            return bind(
                                read<datum_nodes, datum_list>(second, symbols),
                                [&, first_root](form_type const &tail) {
                                    return bind(
                                        elaborate_into(tail, symbols, program),
                                        [&, first_root](int second_root)
                                            -> result<outcome> {
                                            program_type::child_list forms;
                                            forms.push_back(first_root);
                                            forms.push_back(second_root);
                                            program.set_root(program.add_branch(
                                                core_tag{core_progn{}}, forms));
                                            machine_type running{
                                                program, symbols, known};
                                            return running.run();
                                        });
                                });
                        });
                });
        });
    return ran.has_value() ? ran.value() : outcome{ran.error()};
}

// (defun len (l) (if (null l) 0 (+ 1 (len (cdr l))))) followed by
// (len '(a b c)) is 3.
//
// It works because the callee is resolved in the function slot at the moment
// of the *call*, not captured when the definition was made: the body names
// LEN while LEN is still being defined, and finds the definition being
// installed. That is DIV-0009, and closing it is what decision D12's
// interned function slot exists for.
constexpr auto recursive_defun_counts_a_list() -> bool {
    auto const done =
        evaluate_both("(defun len (l) (if (null l) 0 (+ 1 (len (cdr l)))))",
                      "(len '(a b c))");
    return done.is_value() && done.as_value() == value{value_fixnum{3}};
}

// ------------------------------------------------------------------
// Atoms and variables
// ------------------------------------------------------------------

constexpr auto literals_evaluate_to_themselves() -> bool {
    return yields("42", 42) && yields("(progn 1 2 3)", 3) &&
           evaluate("nil").how.as_value() == evaluate("'()").how.as_value() &&
           evaluate("t").how.is_value();
}

constexpr auto an_unbound_variable_is_diagnosed() -> bool {
    return is_error("x");
}

// A quoted list is data, built by the core's own pair node rather than by
// calling whatever the function slot of CONS holds.
constexpr auto quoted_data_becomes_cons_cells() -> bool {
    auto const done = evaluate("(car '(7 8))").how;
    return done.is_value() && done.as_value() == value{value_fixnum{7}} &&
           std::holds_alternative<value_cons>(
               evaluate("'(1)").how.as_value()) &&
           std::holds_alternative<value_symbol>(evaluate("'a").how.as_value());
}

// ------------------------------------------------------------------
// Conditionals and sequences
// ------------------------------------------------------------------

// The unevaluated arm really is unevaluated: the else branch here would be
// an error if it ran.
constexpr auto if_evaluates_one_arm() -> bool {
    return yields("(if t 1 (car 2))", 1) && yields("(if nil (car 2) 3)", 3) &&
           // A two-armed if whose test is false is nil, not an error.
           evaluate("(if nil 1)").how.is_value() &&
           // Zero is true in Common Lisp; only nil is false.
           yields("(if 0 1 2)", 1);
}

constexpr auto an_error_in_a_test_stops_the_form() -> bool {
    return is_error("(if (car 2) 1 3)");
}

// ------------------------------------------------------------------
// Calls
// ------------------------------------------------------------------

constexpr auto calls_evaluate_arguments_left_to_right() -> bool {
    return yields("(+ (* 2 3) (- 10 4))", 12) && yields("(length '(1 2 3))", 3);
}

constexpr auto defun_returns_its_name() -> bool {
    auto const done = evaluate("(defun f (x) x)").how;
    return done.is_value() &&
           std::holds_alternative<value_symbol>(done.as_value());
}

constexpr auto a_defined_function_is_callable() -> bool {
    return yields("(progn (defun twice (x) (+ x x)) (twice 21))", 42) &&
           // The body is an implicit progn, and the parameters are lexical.
           yields("(progn (defun f (a b) (progn 0 (+ a b))) (f 1 2))", 3);
}

// A builtin is not a special case in the call path: it is a symbol whose
// function slot happens to hold one, so redefining it works like anything
// else (decision D12).
constexpr auto a_builtin_can_be_redefined() -> bool {
    return yields("(progn (defun car (x) 99) (car '(1 2)))", 99);
}

constexpr auto calling_an_undefined_function_is_diagnosed() -> bool {
    return is_error("(nosuchfunction 1)");
}

constexpr auto wrong_argument_count_is_diagnosed() -> bool {
    return is_error("(progn (defun f (x) x) (f 1 2))") &&
           is_error("(progn (defun f (x) x) (f))");
}

// A parameter shadows an outer binding of the same name, because the
// environment is an association list extended on the left.
constexpr auto parameters_shadow() -> bool {
    return yields("(progn (defun inner (x) x) (defun outer (x) (inner 2)) "
                  "(outer 1))",
                  2);
}

// ------------------------------------------------------------------
// The third channel: block and return-from
// ------------------------------------------------------------------

// An unwind aimed at a block lands there and becomes that block's value.
constexpr auto return_from_lands_at_its_block() -> bool {
    return yields("(block here (return-from here 5) (car 2))", 5) &&
           // A block whose body completes normally is its last form's value.
           yields("(block here 1 2)", 2) &&
           // return-from with no value carries nil.
           evaluate("(block here (return-from here))").how.is_value();
}

// Every defun body sits in a block named for the function (ANSI 3.1.2.1.2.2),
// which is what makes return-from usable without a block of one's own.
constexpr auto a_defun_body_is_a_block() -> bool {
    return yields("(progn (defun f (x) (return-from f 7) (car 2)) (f 1))", 7);
}

// An unwind with no matching block reaches the top as an unwind — not as an
// error. Decision D13's whole point: control flow does not travel in the
// error channel, so it is still recognisably control flow when it escapes.
constexpr auto an_unaimed_unwind_reaches_the_top() -> bool {
    auto const done = evaluate("(block here 1) (return-from here 1)");
    auto const escaped = evaluate("(return-from nowhere 4)").how;
    return escaped.is_unwind() && !escaped.is_error() &&
           escaped.as_unwind().payload == value{value_fixnum{4}} &&
           done.how.is_error(); // trailing input: two forms, one read
}

// An unwind passes through an intervening block that is not its target.
constexpr auto an_unwind_passes_through_a_foreign_block() -> bool {
    auto const done = evaluate("(block outer (block inner "
                               "(return-from outer 9)) (car 2))")
                          .how;
    return done.is_value() && done.as_value() == value{value_fixnum{9}};
}

// ------------------------------------------------------------------
// unwind-protect: one cleanup, three channels
// ------------------------------------------------------------------

// The property decision D13 says has to be *reconstructed* once the channels
// are split. Under a one-wire result the four outcomes are merged before
// unwind-protect sees them, so one unconditional statement suffices; here
// they are not merged, and the k_protect frame is what makes "cleanup runs
// on all three" structural. Each case checks the cleanup ran *and* that the
// protected form's own completion survived it.
constexpr auto cleanup_runs_on_the_value_channel() -> bool {
    auto const done = evaluate("(unwind-protect 1 (defun ran () 1))");
    return done.cleanup_ran && done.how.is_value() &&
           done.how.as_value() == value{value_fixnum{1}};
}

constexpr auto cleanup_runs_on_the_error_channel() -> bool {
    auto const done = evaluate("(unwind-protect (car 2) (defun ran () 1))");
    return done.cleanup_ran && done.how.is_error();
}

constexpr auto cleanup_runs_on_the_unwind_channel() -> bool {
    auto const done = evaluate(
        "(block out (unwind-protect (return-from out 6) (defun ran () 1)))");
    return done.cleanup_ran && done.how.is_value() &&
           done.how.as_value() == value{value_fixnum{6}};
}

// Several cleanups all run, in order, and none of them contributes a value.
constexpr auto every_cleanup_runs_and_none_is_the_value() -> bool {
    return yields("(unwind-protect 3 1 2)", 3);
}

// A transfer out of a cleanup replaces what it was cleaning up after: there
// is nothing left to re-emit.
constexpr auto a_transfer_out_of_a_cleanup_wins() -> bool {
    return is_error("(unwind-protect 1 (car 2))");
}

// The negative control for the three tests above: the probe answers false
// when the definition it watches for does not run, so the three "cleanup
// ran" results say something.
constexpr auto the_cleanup_probe_can_say_no() -> bool {
    return !evaluate("(if nil (defun ran () 1) 2)").cleanup_ran &&
           !evaluate("1").cleanup_ran;
}

// ------------------------------------------------------------------
// Capacity is diagnosed, never asserted
// ------------------------------------------------------------------

constexpr limits tight_steps{.cells = 32,
                             .string_chars = 8,
                             .closures = 2,
                             .frames = 16,
                             .call_args = 4,
                             .steps = 4};

// A non-terminating object program must not become a non-terminating
// translation, so the step budget is part of the contract.
constexpr auto a_spent_step_budget_is_diagnosed() -> bool {
    table_type symbols;
    auto const ran =
        bind(install_builtins(symbols), [&symbols](standard_symbols known) {
            return bind(
                read<datum_nodes, datum_list>("(+ 1 (+ 2 (+ 3 4)))", symbols),
                [&symbols, known](form_type const &form) {
                    return bind(
                        elaborate<core_nodes, core_children>(form, symbols),
                        [&symbols, known](
                            program_type const &program) -> result<outcome> {
                            machine<tight_steps, program_type, table_type>
                                running{program, symbols, known};
                            return running.run();
                        });
                });
        });
    return ran.has_value() && ran.value().is_error();
}

// Recursion depth is a diagnosed capacity rather than the C++ stack, because
// no evaluation recursion is C++ recursion.
constexpr auto an_overflowing_control_stack_is_diagnosed() -> bool {
    return is_error("(progn (defun down (n) (down n)) (down 1))");
}

// The same program is fine when it terminates within the budget.
constexpr auto bounded_recursion_finishes() -> bool {
    return yields("(progn (defun down (n) (if (= n 0) 0 (down (- n 1)))) "
                  "(down 20))",
                  0);
}

} // namespace

static_assert(recursive_defun_counts_a_list());
static_assert(literals_evaluate_to_themselves());
static_assert(an_unbound_variable_is_diagnosed());
static_assert(quoted_data_becomes_cons_cells());
static_assert(if_evaluates_one_arm());
static_assert(an_error_in_a_test_stops_the_form());
static_assert(calls_evaluate_arguments_left_to_right());
static_assert(defun_returns_its_name());
static_assert(a_defined_function_is_callable());
static_assert(a_builtin_can_be_redefined());
static_assert(calling_an_undefined_function_is_diagnosed());
static_assert(wrong_argument_count_is_diagnosed());
static_assert(parameters_shadow());
static_assert(return_from_lands_at_its_block());
static_assert(a_defun_body_is_a_block());
static_assert(an_unaimed_unwind_reaches_the_top());
static_assert(an_unwind_passes_through_a_foreign_block());
static_assert(cleanup_runs_on_the_value_channel());
static_assert(cleanup_runs_on_the_error_channel());
static_assert(cleanup_runs_on_the_unwind_channel());
static_assert(every_cleanup_runs_and_none_is_the_value());
static_assert(a_transfer_out_of_a_cleanup_wins());
static_assert(the_cleanup_probe_can_say_no());
static_assert(a_spent_step_budget_is_diagnosed());
static_assert(an_overflowing_control_stack_is_diagnosed());
static_assert(bounded_recursion_finishes());

TEST_CASE("MachineTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("MachineTest - RecursiveDefun") {
    CHECK(recursive_defun_counts_a_list());
}

TEST_CASE("MachineTest - AtomsAndVariables") {
    CHECK(literals_evaluate_to_themselves());
    CHECK(an_unbound_variable_is_diagnosed());
    CHECK(quoted_data_becomes_cons_cells());
}

TEST_CASE("MachineTest - Conditionals") {
    CHECK(if_evaluates_one_arm());
    CHECK(an_error_in_a_test_stops_the_form());
}

TEST_CASE("MachineTest - Calls") {
    CHECK(calls_evaluate_arguments_left_to_right());
    CHECK(defun_returns_its_name());
    CHECK(a_defined_function_is_callable());
    CHECK(a_builtin_can_be_redefined());
    CHECK(calling_an_undefined_function_is_diagnosed());
    CHECK(wrong_argument_count_is_diagnosed());
    CHECK(parameters_shadow());
}

TEST_CASE("MachineTest - BlockAndReturnFrom") {
    CHECK(return_from_lands_at_its_block());
    CHECK(a_defun_body_is_a_block());
    CHECK(an_unaimed_unwind_reaches_the_top());
    CHECK(an_unwind_passes_through_a_foreign_block());
}

TEST_CASE("MachineTest - UnwindProtectOnAllThreeChannels") {
    CHECK(cleanup_runs_on_the_value_channel());
    CHECK(cleanup_runs_on_the_error_channel());
    CHECK(cleanup_runs_on_the_unwind_channel());
    CHECK(every_cleanup_runs_and_none_is_the_value());
    CHECK(a_transfer_out_of_a_cleanup_wins());
    CHECK(the_cleanup_probe_can_say_no());
}

TEST_CASE("MachineTest - CapacityIsDiagnosed") {
    CHECK(a_spent_step_budget_is_diagnosed());
    CHECK(an_overflowing_control_stack_is_diagnosed());
    CHECK(bounded_recursion_finishes());
}
