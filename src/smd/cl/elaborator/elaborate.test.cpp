// src/smd/cl/elaborator/elaborate.test.cpp                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/elaborator/elaborate.hpp>
#include <smd/cl/elaborator/elaborate.hpp> // test 2nd include OK

#include <smd/cl/core/ast.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/reader/read.hpp>
#include <smd/cl/symbol/symbol_table.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

using smd::cl::core::core_call;
using smd::cl::core::core_character;
using smd::cl::core::core_cons;
using smd::cl::core::core_fixnum;
using smd::cl::core::core_if;
using smd::cl::core::core_keyword;
using smd::cl::core::core_leaf;
using smd::cl::core::core_nil;
using smd::cl::core::core_progn;
using smd::cl::core::core_quoted_symbol;
using smd::cl::core::core_string;
using smd::cl::core::core_t;
using smd::cl::core::core_tag;
using smd::cl::core::core_tree;
using smd::cl::core::core_variable;
using smd::cl::elaborator::elaborate;
using smd::cl::foundation::and_then;
using smd::cl::foundation::result;
using smd::cl::reader::datum_tree;
using smd::cl::reader::read;
using smd::cl::symbol::symbol_id;

namespace {

// Slot types are the evaluator's business (R5); ints stand in.
using sym_table = smd::cl::symbol::symbol_table<int, int, int, 64, 512>;

constexpr int datum_nodes = 64;
constexpr int datum_list = 8;
constexpr int core_nodes = 64;
constexpr int core_children = 8;

using tree = core_tree<core_nodes, core_children>;

constexpr auto elaborate_text(std::string_view text, sym_table &symbols)
    -> result<tree> {
    return and_then(
        read<datum_nodes, datum_list>(text, symbols),
        [&symbols](datum_tree<datum_nodes, datum_list> const &form) {
            return elaborate<core_nodes, core_children>(form, symbols);
        });
}

// Elaborates with a throwaway table, for the cases that do not need to ask
// the table anything afterwards.
constexpr auto elaborated(std::string_view text) -> result<tree> {
    sym_table symbols;
    return elaborate_text(text, symbols);
}

constexpr auto fails(std::string_view text) -> bool {
    return !elaborated(text).has_value();
}

// True when text elaborates to a single leaf equal to expected.
constexpr auto is_leaf(std::string_view text, core_leaf const &expected)
    -> bool {
    auto const r = elaborated(text);
    return r.has_value() && r.value().size() == 1 && r.value().root() == 0 &&
           r.value().leaf(0) == expected;
}

// ------------------------------------------------------------------
// The merge criterion: (if (zerop n) 1 nil) is the shape core/ast.test.cpp
// builds by hand, with the ids the reader actually interned.
// ------------------------------------------------------------------

constexpr auto expected_conditional(symbol_id zerop, symbol_id n) -> tree {
    tree t;
    int const variable = t.add_leaf(core_leaf{core_variable{n}});
    tree::child_list arguments;
    arguments.push_back(variable);
    int const test = t.add_branch(core_tag{core_call{zerop}}, arguments);
    int const then_arm = t.add_leaf(core_leaf{core_fixnum{1}});
    int const else_arm = t.add_leaf(core_leaf{core_nil{}});
    tree::child_list arms;
    arms.push_back(test);
    arms.push_back(then_arm);
    arms.push_back(else_arm);
    t.set_root(t.add_branch(core_tag{core_if{}}, arms));
    return t;
}

constexpr auto elaborates_the_sample_core() -> bool {
    sym_table symbols;
    auto const r = elaborate_text("(if (zerop n) 1 nil)", symbols);
    auto const zerop = symbols.find("ZEROP");
    auto const n = symbols.find("N");
    return r.has_value() && zerop && n &&
           r.value() == expected_conditional(*zerop, *n);
}

// ------------------------------------------------------------------
// Atoms
// ------------------------------------------------------------------

constexpr auto self_evaluating_atoms() -> bool {
    return is_leaf("42", core_leaf{core_fixnum{42}}) &&
           is_leaf("-7", core_leaf{core_fixnum{-7}}) &&
           is_leaf("nil", core_leaf{core_nil{}}) &&
           is_leaf("()", core_leaf{core_nil{}}) &&
           is_leaf("t", core_leaf{core_t{}}) &&
           is_leaf("#\\a", core_leaf{core_character{'a'}}) &&
           is_leaf("#\\Space", core_leaf{core_character{' '}});
}

constexpr auto keywords_and_variables_carry_their_ids() -> bool {
    sym_table symbols;
    auto const keyword = elaborate_text(":foo", symbols);
    auto const variable = elaborate_text("foo", symbols);
    auto const keyword_id = symbols.find(":FOO");
    auto const variable_id = symbols.find("FOO");
    // Decision D7 carries over: a keyword and a like-named symbol are
    // distinct table entries, so these ids differ.
    return keyword.has_value() && variable.has_value() && keyword_id &&
           variable_id && *keyword_id != *variable_id &&
           keyword.value().leaf(0) == core_leaf{core_keyword{*keyword_id}} &&
           variable.value().leaf(0) == core_leaf{core_variable{*variable_id}};
}

constexpr auto string_literals_carry_their_own_characters() -> bool {
    auto const r = elaborated("\"hi\"");
    if (!r.has_value() || r.value().size() != 1) {
        return false;
    }
    auto const *literal = std::get_if<core_string>(&r.value().leaf(0));
    return literal != nullptr && literal->view() == "hi";
}

// Decision D19: readable is not executable. The reader accepts the whole
// tower's syntax; the elaborator says so when the semantics are missing.
constexpr auto tower_literals_are_readable_but_not_executable() -> bool {
    return fails("1/2") && fails("1.5") && fails("#xFFFFFFFFFFFF") &&
           fails("(+ 1 1/2)");
}

constexpr auto deferred_syntax_is_diagnosed() -> bool {
    return fails("#(1 2)") && fails("#'car") && fails("`x") && fails(",x");
}

// ------------------------------------------------------------------
// Compound forms
// ------------------------------------------------------------------

constexpr auto calls_put_the_callee_in_the_tag() -> bool {
    sym_table symbols;
    auto const r = elaborate_text("(car x)", symbols);
    auto const car = symbols.find("CAR");
    if (!r.has_value() || !car) {
        return false;
    }
    auto const &t = r.value();
    auto const &root = t.branch(t.root());
    // Lisp-2: the callee is not a child, so the call has exactly one child.
    return t.size() == 2 && std::holds_alternative<core_call>(root.tag) &&
           std::get<core_call>(root.tag).callee == *car &&
           root.children.size() == 1;
}

constexpr auto calls_may_take_no_arguments() -> bool {
    auto const r = elaborated("(f)");
    return r.has_value() && r.value().size() == 1 &&
           r.value().branch(r.value().root()).children.empty();
}

constexpr auto conditionals_take_two_or_three_arms() -> bool {
    auto const two = elaborated("(if a b)");
    auto const three = elaborated("(if a b c)");
    return two.has_value() &&
           two.value().branch(two.value().root()).children.size() == 2 &&
           three.has_value() &&
           three.value().branch(three.value().root()).children.size() == 3 &&
           fails("(if)") && fails("(if a)") && fails("(if a b c d)");
}

constexpr auto sequences_lower_to_progn() -> bool {
    auto const body = elaborated("(progn 1 2)");
    return body.has_value() &&
           std::holds_alternative<core_progn>(
               body.value().branch(body.value().root()).tag) &&
           body.value().branch(body.value().root()).children.size() == 2 &&
           // (progn) is NIL, which needs no progn node at all.
           is_leaf("(progn)", core_leaf{core_nil{}});
}

constexpr auto a_form_head_must_name_a_function() -> bool {
    return fails("(1 2)") && fails("(nil)") && fails("(t)") &&
           fails("((f) 1)") && fails("(\"s\")");
}

// ------------------------------------------------------------------
// Quotation
// ------------------------------------------------------------------

// 'x and (quote x) are the same form (ANSI 2.4.6), so they must elaborate
// to the same core, and neither needs a node of its own.
constexpr auto both_spellings_of_quote_agree() -> bool {
    // One table for both: identity is id comparison (decision D12), so two
    // cores are only comparable when their symbols came from one table.
    sym_table symbols;
    auto const a = elaborate_text("'x", symbols);
    auto const b = elaborate_text("(quote x)", symbols);
    return a.has_value() && b.has_value() && a.value() == b.value() &&
           a.value().size() == 1 &&
           std::holds_alternative<core_quoted_symbol>(a.value().leaf(0));
}

// A quoted symbol is data; an unquoted one is a variable reference. Same
// atom, same id, different leaf — the position picks the namespace.
constexpr auto quoting_changes_the_namespace_not_the_symbol() -> bool {
    sym_table symbols;
    auto const quoted = elaborate_text("'foo", symbols);
    auto const bare = elaborate_text("foo", symbols);
    auto const id = symbols.find("FOO");
    return quoted.has_value() && bare.has_value() && id &&
           quoted.value().leaf(0) == core_leaf{core_quoted_symbol{*id}} &&
           bare.value().leaf(0) == core_leaf{core_variable{*id}};
}

// Constants are already themselves, so quoting them adds nothing.
constexpr auto quoted_constants_are_the_constants() -> bool {
    return is_leaf("'42", core_leaf{core_fixnum{42}}) &&
           is_leaf("'nil", core_leaf{core_nil{}}) &&
           is_leaf("'()", core_leaf{core_nil{}}) &&
           is_leaf("'t", core_leaf{core_t{}}) &&
           is_leaf("'#\\a", core_leaf{core_character{'a'}});
}

constexpr auto quoted_lists_are_hermetic_pairs() -> bool {
    sym_table symbols;
    auto const r = elaborate_text("'(a b)", symbols);
    auto const a = symbols.find("A");
    auto const b = symbols.find("B");
    if (!r.has_value() || !a || !b) {
        return false;
    }
    auto const &t = r.value();
    auto const &outer = t.branch(t.root());
    if (!std::holds_alternative<core_cons>(outer.tag) ||
        outer.children.size() != 2) {
        return false;
    }
    auto const &inner = t.branch(outer.children[1]);
    return t.leaf(outer.children[0]) == core_leaf{core_quoted_symbol{*a}} &&
           std::holds_alternative<core_cons>(inner.tag) &&
           t.leaf(inner.children[0]) == core_leaf{core_quoted_symbol{*b}} &&
           t.leaf(inner.children[1]) == core_leaf{core_nil{}};
}

// The pairs a quoted list builds are literal data, not a call to whatever
// the function slot of CONS happens to hold.
constexpr auto quoted_lists_do_not_call_cons() -> bool {
    sym_table symbols;
    auto const r = elaborate_text("'(a)", symbols);
    return r.has_value() && !symbols.find("CONS").has_value();
}

// ''x is the two-element list (QUOTE X): ANSI 2.4.6 fixes that
// representation, and the reader never spelled QUOTE, so the elaborator
// interns it.
constexpr auto nested_quotation_materializes_the_operator() -> bool {
    sym_table symbols;
    auto const r = elaborate_text("''x", symbols);
    auto const quote = symbols.find("QUOTE");
    auto const x = symbols.find("X");
    if (!r.has_value() || !quote || !x) {
        return false;
    }
    auto const &t = r.value();
    auto const &outer = t.branch(t.root());
    auto const &inner = t.branch(outer.children[1]);
    return t.leaf(outer.children[0]) == core_leaf{core_quoted_symbol{*quote}} &&
           t.leaf(inner.children[0]) == core_leaf{core_quoted_symbol{*x}} &&
           t.leaf(inner.children[1]) == core_leaf{core_nil{}};
}

constexpr auto quote_takes_exactly_one_argument() -> bool {
    return fails("(quote)") && fails("(quote a b)");
}

// Not yet representable as data, and said so rather than silently dropped.
constexpr auto deferred_quoted_syntax_is_diagnosed() -> bool {
    return fails("'#(1)") && fails("'`x") && fails("'1/2");
}

// ------------------------------------------------------------------
// Error propagation
// ------------------------------------------------------------------

// The atom pass runs over the whole form before any structure is lowered,
// so an unexecutable atom is reported even where the structure around it
// would also have failed.
constexpr auto the_leftmost_atom_error_wins() -> bool {
    auto const r = elaborated("(if 1/2 1.5 x)");
    return !r.has_value() && std::string_view{r.error().message} ==
                                 "numeric tower literal is not yet executable";
}

// Capacity is diagnosed, never asserted: a form too big for the core tree
// it was asked to fill comes back as an error.
constexpr auto exhausted_capacity_is_an_error() -> bool {
    sym_table symbols;
    auto const form = read<datum_nodes, datum_list>("(f 1 2 3 4 5)", symbols);
    return form.has_value() &&
           !elaborate<4, 8>(form.value(), symbols).has_value();
}

constexpr auto too_many_subforms_is_an_error() -> bool {
    sym_table symbols;
    auto const form = read<datum_nodes, datum_list>("(f 1 2 3 4 5)", symbols);
    // Five arguments into a core node that holds three.
    return form.has_value() &&
           !elaborate<64, 3>(form.value(), symbols).has_value();
}

} // namespace

static_assert(elaborates_the_sample_core());
static_assert(self_evaluating_atoms());
static_assert(keywords_and_variables_carry_their_ids());
static_assert(string_literals_carry_their_own_characters());
static_assert(tower_literals_are_readable_but_not_executable());
static_assert(deferred_syntax_is_diagnosed());
static_assert(calls_put_the_callee_in_the_tag());
static_assert(calls_may_take_no_arguments());
static_assert(conditionals_take_two_or_three_arms());
static_assert(sequences_lower_to_progn());
static_assert(a_form_head_must_name_a_function());
static_assert(both_spellings_of_quote_agree());
static_assert(quoting_changes_the_namespace_not_the_symbol());
static_assert(quoted_constants_are_the_constants());
static_assert(quoted_lists_are_hermetic_pairs());
static_assert(quoted_lists_do_not_call_cons());
static_assert(nested_quotation_materializes_the_operator());
static_assert(quote_takes_exactly_one_argument());
static_assert(deferred_quoted_syntax_is_diagnosed());
static_assert(the_leftmost_atom_error_wins());
static_assert(exhausted_capacity_is_an_error());
static_assert(too_many_subforms_is_an_error());

TEST_CASE("ElaborateTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ElaborateTest - ElaboratesTheSampleCore") {
    CHECK(elaborates_the_sample_core());
}

TEST_CASE("ElaborateTest - Atoms") {
    CHECK(self_evaluating_atoms());
    CHECK(keywords_and_variables_carry_their_ids());
    CHECK(string_literals_carry_their_own_characters());
    CHECK(tower_literals_are_readable_but_not_executable());
    CHECK(deferred_syntax_is_diagnosed());
}

TEST_CASE("ElaborateTest - CompoundForms") {
    CHECK(calls_put_the_callee_in_the_tag());
    CHECK(calls_may_take_no_arguments());
    CHECK(conditionals_take_two_or_three_arms());
    CHECK(sequences_lower_to_progn());
    CHECK(a_form_head_must_name_a_function());
}

TEST_CASE("ElaborateTest - Quotation") {
    CHECK(both_spellings_of_quote_agree());
    CHECK(quoting_changes_the_namespace_not_the_symbol());
    CHECK(quoted_constants_are_the_constants());
    CHECK(quoted_lists_are_hermetic_pairs());
    CHECK(quoted_lists_do_not_call_cons());
    CHECK(nested_quotation_materializes_the_operator());
    CHECK(quote_takes_exactly_one_argument());
    CHECK(deferred_quoted_syntax_is_diagnosed());
}

TEST_CASE("ElaborateTest - ErrorPropagation") {
    CHECK(the_leftmost_atom_error_wins());
    CHECK(exhausted_capacity_is_an_error());
    CHECK(too_many_subforms_is_an_error());
}
