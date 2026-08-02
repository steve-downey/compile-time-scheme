// src/smd/cl/reader/read.test.cpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/reader/read.hpp>
#include <smd/cl/reader/read.hpp> // test 2nd include OK

#include <smd/cl/foundation/foldable.hpp>
#include <smd/cl/foundation/result_instances.hpp>
#include <smd/cl/foundation/traversable.hpp>
#include <smd/cl/symbol/symbol_table.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

using smd::cl::foundation::length;
using smd::cl::foundation::result;
using smd::cl::foundation::traverse;
using smd::cl::reader::datum_atom;
using smd::cl::reader::datum_branch;
using smd::cl::reader::datum_character;
using smd::cl::reader::datum_fixnum;
using smd::cl::reader::datum_keyword;
using smd::cl::reader::datum_string;
using smd::cl::reader::datum_symbol;
using smd::cl::reader::datum_tower;
using smd::cl::reader::datum_tree;
using smd::cl::reader::read;
using smd::cl::reader::read_datum;
using smd::cl::reader::tower_kind;

namespace {

// Slot types are later stages' business (R2); ints stand in.
using sym_table = smd::cl::symbol::symbol_table<int, int, int, 64, 512>;

constexpr int test_nodes = 64;
constexpr int test_list = 8;
using tree = datum_tree<test_nodes, test_list>;

constexpr auto read_one(std::string_view text, sym_table &syms)
    -> result<tree> {
    return read<test_nodes, test_list>(text, syms);
}

constexpr auto root_fixnum_is(std::string_view text, int expected) -> bool {
    sym_table syms;
    auto const r = read_one(text, syms);
    if (!r.has_value() || !r.value().is_leaf(r.value().root())) {
        return false;
    }
    auto const *f =
        std::get_if<datum_fixnum>(&r.value().leaf(r.value().root()));
    return f != nullptr && f->value == expected;
}

constexpr auto root_symbol_named(std::string_view text, std::string_view name)
    -> bool {
    sym_table syms;
    auto const r = read_one(text, syms);
    if (!r.has_value() || !r.value().is_leaf(r.value().root())) {
        return false;
    }
    auto const *s =
        std::get_if<datum_symbol>(&r.value().leaf(r.value().root()));
    return s != nullptr && s->id.valid() && syms.name(s->id) == name &&
           syms.is_interned(s->id);
}

constexpr auto root_keyword_named(std::string_view text, std::string_view name)
    -> bool {
    sym_table syms;
    auto const r = read_one(text, syms);
    if (!r.has_value() || !r.value().is_leaf(r.value().root())) {
        return false;
    }
    auto const *k =
        std::get_if<datum_keyword>(&r.value().leaf(r.value().root()));
    return k != nullptr && k->id.valid() && syms.name(k->id) == name;
}

constexpr auto root_character_is(std::string_view text, char expected) -> bool {
    sym_table syms;
    auto const r = read_one(text, syms);
    if (!r.has_value() || !r.value().is_leaf(r.value().root())) {
        return false;
    }
    auto const *c =
        std::get_if<datum_character>(&r.value().leaf(r.value().root()));
    return c != nullptr && c->value == expected;
}

constexpr auto root_string_is(std::string_view text, std::string_view expected)
    -> bool {
    sym_table syms;
    auto const r = read_one(text, syms);
    if (!r.has_value() || !r.value().is_leaf(r.value().root())) {
        return false;
    }
    auto const *s =
        std::get_if<datum_string>(&r.value().leaf(r.value().root()));
    return s != nullptr && s->view() == expected;
}

constexpr auto root_tower_is(std::string_view text, tower_kind kind, int radix,
                             std::string_view spelling) -> bool {
    sym_table syms;
    auto const r = read_one(text, syms);
    if (!r.has_value() || !r.value().is_leaf(r.value().root())) {
        return false;
    }
    auto const *t = std::get_if<datum_tower>(&r.value().leaf(r.value().root()));
    return t != nullptr && t->kind == kind && t->radix == radix &&
           t->spelling.view() == spelling;
}

constexpr auto root_branch_arity(std::string_view text, datum_branch kind,
                                 int arity) -> bool {
    sym_table syms;
    auto const r = read_one(text, syms);
    if (!r.has_value() || r.value().is_leaf(r.value().root())) {
        return false;
    }
    auto const &b = r.value().branch(r.value().root());
    return b.tag == kind && b.children.size() == arity;
}

constexpr auto fails_with(std::string_view text, std::string_view message)
    -> bool {
    sym_table syms;
    auto const r = read_one(text, syms);
    return !r.has_value() && std::string_view{r.error().message} == message;
}

// --- atoms ---------------------------------------------------------------

constexpr auto reads_fixnums() -> bool {
    return root_fixnum_is("42", 42) && root_fixnum_is("-7", -7) &&
           root_fixnum_is("+9", 9) && root_fixnum_is("10.", 10) &&
           root_fixnum_is("2147483647", 2147483647) &&
           root_fixnum_is("-2147483648", -2147483647 - 1);
}

constexpr auto reads_symbols_folded() -> bool {
    return root_symbol_named("foo", "FOO") && root_symbol_named("cAr", "CAR") &&
           root_symbol_named("t", "T") && root_symbol_named("nil", "NIL") &&
           root_symbol_named("+", "+") && root_symbol_named("-", "-") &&
           root_symbol_named(":", ":");
}

constexpr auto whole_token_classification() -> bool {
    // DIV-0003, accepted-permanent: 1+ is the symbol it is in ANSI CL.
    return root_symbol_named("1+", "1+") &&
           root_symbol_named("2buffer", "2BUFFER") &&
           root_symbol_named("1.5x", "1.5X");
}

constexpr auto reads_escaped_symbols() -> bool {
    return root_symbol_named("|Foo Bar|", "Foo Bar") &&
           root_symbol_named("\\foo", "fOO") &&
           root_symbol_named("|42|", "42") && // escaped: never a number
           root_symbol_named("||", "");
}

constexpr auto reads_keywords() -> bool {
    // Keywords intern under their colon-carrying spelling until a package
    // system exists, so :foo and foo are distinct table entries.
    return root_keyword_named(":foo", ":FOO") &&
           root_keyword_named(":Bar", ":BAR");
}

constexpr auto keyword_and_symbol_are_distinct() -> bool {
    sym_table syms;
    auto const kw = read_one(":foo", syms);
    auto const sy = read_one("foo", syms);
    if (!kw.has_value() || !sy.has_value()) {
        return false;
    }
    auto const *k = std::get_if<datum_keyword>(&kw.value().leaf(0));
    auto const *s = std::get_if<datum_symbol>(&sy.value().leaf(0));
    return k != nullptr && s != nullptr && !(k->id == s->id);
}

constexpr auto interning_is_stable_across_reads() -> bool {
    // Merge criterion: reading the same symbol name twice anywhere in an
    // input yields the same symbol_id — within one datum, across data,
    // and across spellings that fold together.
    sym_table syms;
    auto const first = read_one("(foo (bar foo))", syms);
    auto const second = read_one("FOO", syms);
    if (!first.has_value() || !second.has_value()) {
        return false;
    }
    auto const &t = first.value();
    auto const *outer_foo = std::get_if<datum_symbol>(&t.leaf(0));
    auto const *inner_foo = std::get_if<datum_symbol>(&t.leaf(2));
    auto const *again = std::get_if<datum_symbol>(&second.value().leaf(0));
    return outer_foo != nullptr && inner_foo != nullptr && again != nullptr &&
           outer_foo->id == inner_foo->id && outer_foo->id == again->id &&
           syms.name(outer_foo->id) == "FOO";
}

constexpr auto reads_characters() -> bool {
    return root_character_is("#\\a", 'a') &&
           root_character_is("#\\A", 'A') && // single char: case matters
           root_character_is("#\\(", '(') && root_character_is("#\\1", '1') &&
           root_character_is("#\\Space", ' ') &&
           root_character_is("#\\newline", '\n') && // names: case-insensitive
           root_character_is("#\\TAB", '\t') &&
           root_character_is("#\\Rubout", '\x7f');
}

constexpr auto reads_strings() -> bool {
    return root_string_is("\"hello\"", "hello") && root_string_is("\"\"", "") &&
           root_string_is("\"a\\\"b\"", "a\"b") &&
           root_string_is("\"a\\\\b\"", "a\\b") &&
           root_string_is("\"two\nlines\"", "two\nlines") &&
           root_string_is("\"Case Kept\"", "Case Kept");
}

constexpr auto reads_tower_spellings() -> bool {
    // D19: readable before executable; the datum carries the spelling.
    return root_tower_is("1.5", tower_kind::floating, 10, "1.5") &&
           root_tower_is(".5e3", tower_kind::floating, 10, ".5E3") &&
           root_tower_is("1d0", tower_kind::floating, 10, "1D0") &&
           root_tower_is("1/2", tower_kind::ratio, 10, "1/2") &&
           root_tower_is("-3/4", tower_kind::ratio, 10, "-3/4") &&
           root_tower_is("2147483648", tower_kind::bignum, 10, "2147483648");
}

constexpr auto reads_radix_prefixes() -> bool {
    return root_fixnum_is("#b1010", 10) && root_fixnum_is("#B1010", 10) &&
           root_fixnum_is("#o777", 511) && root_fixnum_is("#xFF", 255) &&
           root_fixnum_is("#xff", 255) && root_fixnum_is("#x-FF", -255) &&
           root_fixnum_is("#3r102", 11) && root_fixnum_is("#36rZ", 35) &&
           root_tower_is("#b11/101", tower_kind::ratio, 2, "11/101") &&
           root_tower_is("#x100000000", tower_kind::bignum, 16, "100000000");
}

// --- compound data -------------------------------------------------------

constexpr auto reads_lists() -> bool {
    return root_branch_arity("()", datum_branch::list, 0) &&
           root_branch_arity("(1 2 3)", datum_branch::list, 3) &&
           root_branch_arity("( a ( b c ) d )", datum_branch::list, 3);
}

constexpr auto list_elements_in_source_order() -> bool {
    sym_table syms;
    auto const r = read_one("(1 2 3)", syms);
    if (!r.has_value()) {
        return false;
    }
    auto const &t = r.value();
    auto const &children = t.branch(t.root()).children;
    auto const value_at = [&](int i) {
        auto const *f = std::get_if<datum_fixnum>(&t.leaf(children[i]));
        return f != nullptr ? f->value : -1;
    };
    return children.size() == 3 && value_at(0) == 1 && value_at(1) == 2 &&
           value_at(2) == 3;
}

constexpr auto reads_nested_structure() -> bool {
    sym_table syms;
    auto const r = read_one("(a (b c) d)", syms);
    if (!r.has_value()) {
        return false;
    }
    auto const &t = r.value();
    auto const &outer = t.branch(t.root());
    if (outer.children.size() != 3 || !t.is_leaf(outer.children[0]) ||
        t.is_leaf(outer.children[1]) || !t.is_leaf(outer.children[2])) {
        return false;
    }
    auto const &inner = t.branch(outer.children[1]);
    return inner.tag == datum_branch::list && inner.children.size() == 2 &&
           length(t) == 4; // the four symbol leaves, via Foldable
}

constexpr auto reads_vectors() -> bool {
    return root_branch_arity("#(1 2)", datum_branch::vector, 2) &&
           root_branch_arity("#()", datum_branch::vector, 0);
}

constexpr auto reads_quote_family() -> bool {
    return root_branch_arity("'x", datum_branch::quote, 1) &&
           root_branch_arity("#'car", datum_branch::function, 1) &&
           root_branch_arity("`x", datum_branch::backquote, 1) &&
           root_branch_arity(",x", datum_branch::unquote, 1) &&
           root_branch_arity(",@x", datum_branch::unquote_splice, 1) &&
           root_branch_arity("''x", datum_branch::quote, 1);
}

constexpr auto backquote_template_shape() -> bool {
    sym_table syms;
    auto const r = read_one("`(a ,b ,@c)", syms);
    if (!r.has_value()) {
        return false;
    }
    auto const &t = r.value();
    auto const &bq = t.branch(t.root());
    if (bq.tag != datum_branch::backquote || bq.children.size() != 1) {
        return false;
    }
    auto const &list = t.branch(bq.children[0]);
    return list.children.size() == 3 &&
           t.branch(list.children[1]).tag == datum_branch::unquote &&
           t.branch(list.children[2]).tag == datum_branch::unquote_splice;
}

// --- intertoken space ----------------------------------------------------

constexpr auto skips_comments_and_whitespace() -> bool {
    return root_fixnum_is("  42", 42) && root_fixnum_is("; note\n42", 42) &&
           root_fixnum_is("#| block |# 42", 42) &&
           root_fixnum_is("#| nested #| deeper |# still |# 42", 42) &&
           root_branch_arity("(1 ; comment\n 2 #|mid|# 3)", datum_branch::list,
                             3);
}

// --- sequential reads ----------------------------------------------------

constexpr auto read_datum_leaves_the_rest() -> bool {
    sym_table syms;
    auto const first =
        read_datum<test_nodes, test_list>(smd::cl::reader::cursor{"1 2"}, syms);
    if (!first.has_value()) {
        return false;
    }
    auto const second =
        read_datum<test_nodes, test_list>(first.value().rest, syms);
    if (!second.has_value()) {
        return false;
    }
    auto const *one = std::get_if<datum_fixnum>(&first.value().value.leaf(0));
    auto const *two = std::get_if<datum_fixnum>(&second.value().value.leaf(0));
    return one != nullptr && two != nullptr && one->value == 1 &&
           two->value == 2;
}

// --- errors --------------------------------------------------------------

constexpr auto reports_errors() -> bool {
    return fails_with("", "unexpected end of input") &&
           fails_with("   ; only a comment", "unexpected end of input") &&
           fails_with(")", "unexpected ')'") &&
           fails_with("(1 2", "expected ')'") &&
           fails_with("'", "unexpected end of input") &&
           fails_with("\"abc", "unterminated string") &&
           fails_with("#z", "unsupported '#' syntax") &&
           fails_with("#", "unexpected end of input after '#'") &&
           fails_with("#\\Wrong", "unknown character name") &&
           fails_with("#xZZ", "expected a rational after radix prefix") &&
           fails_with("#x1.5", "expected a rational after radix prefix") &&
           fails_with("#37r1", "radix must be between 2 and 36") &&
           fails_with("#1r1", "radix must be between 2 and 36") &&
           fails_with("#5(1)", "sized #n(...) vectors not yet supported") &&
           fails_with("#5'x", "unexpected numeric argument after '#'") &&
           fails_with("1 2", "unexpected trailing input") &&
           fails_with("|abc", "unterminated |");
}

constexpr auto error_positions_track_lines() -> bool {
    sym_table syms;
    auto const r = read_one("\n  )", syms);
    return !r.has_value() && r.error().where.line == 2 &&
           r.error().where.column == 3;
}

constexpr auto capacity_errors_not_asserts() -> bool {
    // Untrusted input surfaces parse_errors; the asserts stay unreached.
    sym_table tiny_syms;
    smd::cl::symbol::symbol_table<int, int, int, 1, 8> one_symbol;
    smd::cl::symbol::symbol_table<int, int, int, 8, 3> three_chars;
    auto const tree_full = read<3, 8>("(1 2 3 4)", tiny_syms);
    auto const table_full = read<64, 8>("(a b)", one_symbol);
    auto const pool_full = read<64, 8>("abcd", three_chars);
    return !tree_full.has_value() &&
           std::string_view{tree_full.error().message} == "datum tree full" &&
           !table_full.has_value() &&
           std::string_view{table_full.error().message} ==
               "symbol table full" &&
           !pool_full.has_value() &&
           std::string_view{pool_full.error().message} ==
               "symbol name storage full";
}

constexpr auto list_capacity_error() -> bool {
    sym_table syms;
    auto const r = read<64, 2>("(1 2 3)", syms);
    return !r.has_value() &&
           std::string_view{r.error().message} == "too many elements";
}

// --- D15 in action -------------------------------------------------------

constexpr auto traverse_propagates_reader_facts() -> bool {
    // The elaborator's pattern (R4): traverse the datum tree's leaves
    // over the result applicative; the leftmost bad leaf's error wins.
    sym_table syms;
    auto const r = read_one("(1 x 2)", syms);
    if (!r.has_value()) {
        return false;
    }
    constexpr smd::cl::foundation::parse_error not_a_number{{}, "not a number"};
    auto const traversed = traverse(
        [](datum_atom const &atom) -> result<int> {
            if (auto const *f = std::get_if<datum_fixnum>(&atom)) {
                return f->value;
            }
            return not_a_number;
        },
        r.value());
    return !traversed.has_value() && traversed.error() == not_a_number;
}

} // namespace

static_assert(reads_fixnums());
static_assert(reads_symbols_folded());
static_assert(whole_token_classification());
static_assert(reads_escaped_symbols());
static_assert(reads_keywords());
static_assert(keyword_and_symbol_are_distinct());
static_assert(interning_is_stable_across_reads());
static_assert(reads_characters());
static_assert(reads_strings());
static_assert(reads_tower_spellings());
static_assert(reads_radix_prefixes());
static_assert(reads_lists());
static_assert(list_elements_in_source_order());
static_assert(reads_nested_structure());
static_assert(reads_vectors());
static_assert(reads_quote_family());
static_assert(backquote_template_shape());
static_assert(skips_comments_and_whitespace());
static_assert(read_datum_leaves_the_rest());
static_assert(reports_errors());
static_assert(error_positions_track_lines());
static_assert(capacity_errors_not_asserts());
static_assert(list_capacity_error());
static_assert(traverse_propagates_reader_facts());

TEST_CASE("ReadTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ReadTest - Atoms") {
    CHECK(reads_fixnums());
    CHECK(reads_symbols_folded());
    CHECK(whole_token_classification());
    CHECK(reads_escaped_symbols());
    CHECK(reads_keywords());
    CHECK(keyword_and_symbol_are_distinct());
    CHECK(reads_characters());
    CHECK(reads_strings());
}

TEST_CASE("ReadTest - Interning") { CHECK(interning_is_stable_across_reads()); }

TEST_CASE("ReadTest - TowerSpellings") {
    CHECK(reads_tower_spellings());
    CHECK(reads_radix_prefixes());
}

TEST_CASE("ReadTest - CompoundData") {
    CHECK(reads_lists());
    CHECK(list_elements_in_source_order());
    CHECK(reads_nested_structure());
    CHECK(reads_vectors());
    CHECK(reads_quote_family());
    CHECK(backquote_template_shape());
}

TEST_CASE("ReadTest - IntertokenSpace") {
    CHECK(skips_comments_and_whitespace());
}

TEST_CASE("ReadTest - SequentialReads") { CHECK(read_datum_leaves_the_rest()); }

TEST_CASE("ReadTest - Errors") {
    CHECK(reports_errors());
    CHECK(error_positions_track_lines());
    CHECK(capacity_errors_not_asserts());
    CHECK(list_capacity_error());
}

TEST_CASE("ReadTest - TraverseOverReadData") {
    CHECK(traverse_propagates_reader_facts());
}
