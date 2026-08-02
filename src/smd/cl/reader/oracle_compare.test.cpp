// src/smd/cl/reader/oracle_compare.test.cpp                      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Differential evidence for the rebuild's reader against the pivot's
// reader (`src/smd/smdlisp/reader/**`, the behavioural oracle, never
// edited): for the syntax the pivot implemented — fixnums, symbols,
// keywords, lists, the quote family, comments — the two readers must
// agree. New R3 syntax (strings, characters, tower spellings, vectors)
// is outside the oracle's coverage; its authority is the ANSI
// specification plus the pending SBCL differential check (plan §4).

#include <smd/cl/reader/read.hpp>
#include <smd/cl/reader/read.hpp> // test 2nd include OK

#include <smd/cl/symbol/symbol_table.hpp>
#include <smd/smdlisp/reader/read_datum.hpp>
#include <smd/smdscheme/parser/cursor.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {

namespace cl = smd::cl;
namespace oracle = smd::smdlisp::reader;

using sym_table = cl::symbol::symbol_table<int, int, int, 64, 512>;
constexpr int test_nodes = 64;
constexpr int test_list = 8;
using new_tree = cl::reader::datum_tree<test_nodes, test_list>;
using oracle_datum = oracle::datum_type<test_nodes, test_list>;
using oracle_arena =
    smd::smdscheme::foundation::tree_arena<oracle_datum, test_nodes>;

struct oracle_read_result {
    bool ok = false;
    oracle_datum datum{};
};

auto oracle_read(std::string_view text, oracle_arena &arena)
    -> oracle_read_result {
    auto const r = oracle::read_datum<test_nodes, test_list>(
        smd::smdscheme::parser::cursor{text}, arena);
    if (!r.has_value()) {
        return {};
    }
    return {true, r.value().value};
}

auto new_read(std::string_view text, sym_table &syms)
    -> cl::foundation::result<new_tree> {
    return cl::reader::read<test_nodes, test_list>(text, syms);
}

auto both_fail(std::string_view text) -> bool {
    sym_table syms;
    oracle_arena arena;
    return !new_read(text, syms).has_value() && !oracle_read(text, arena).ok;
}

auto agree_on_fixnum(std::string_view text) -> bool {
    sym_table syms;
    oracle_arena arena;
    auto const mine = new_read(text, syms);
    auto const theirs = oracle_read(text, arena);
    if (!mine.has_value() || !theirs.ok) {
        return false;
    }
    auto const *my_fix = std::get_if<cl::reader::datum_fixnum>(
        &mine.value().leaf(mine.value().root()));
    auto const *their_fix =
        std::get_if<oracle::datum_integer>(&theirs.datum.inner);
    return my_fix != nullptr && their_fix != nullptr &&
           my_fix->value == their_fix->value;
}

auto agree_on_symbol(std::string_view text) -> bool {
    sym_table syms;
    oracle_arena arena;
    auto const mine = new_read(text, syms);
    auto const theirs = oracle_read(text, arena);
    if (!mine.has_value() || !theirs.ok) {
        return false;
    }
    auto const *my_sym = std::get_if<cl::reader::datum_symbol>(
        &mine.value().leaf(mine.value().root()));
    auto const *their_sym =
        std::get_if<oracle::datum_symbol>(&theirs.datum.inner);
    return my_sym != nullptr && their_sym != nullptr &&
           syms.name(my_sym->id) == their_sym->name.view();
}

auto agree_on_keyword(std::string_view text) -> bool {
    // Same classification, corresponding names: the oracle stores a
    // keyword's name colon-stripped, the rebuild interns it colon-kept
    // (a deliberate representation change, not a behaviour change).
    sym_table syms;
    oracle_arena arena;
    auto const mine = new_read(text, syms);
    auto const theirs = oracle_read(text, arena);
    if (!mine.has_value() || !theirs.ok) {
        return false;
    }
    auto const *my_kw = std::get_if<cl::reader::datum_keyword>(
        &mine.value().leaf(mine.value().root()));
    auto const *their_kw =
        std::get_if<oracle::datum_keyword>(&theirs.datum.inner);
    return my_kw != nullptr && their_kw != nullptr &&
           syms.name(my_kw->id).substr(1) == their_kw->name.view();
}

auto agree_on_list_of_fixnums(std::string_view text) -> bool {
    sym_table syms;
    oracle_arena arena;
    auto const mine = new_read(text, syms);
    auto const theirs = oracle_read(text, arena);
    if (!mine.has_value() || !theirs.ok) {
        return false;
    }
    auto const &my_tree = mine.value();
    if (my_tree.is_leaf(my_tree.root())) {
        return false;
    }
    auto const &my_list = my_tree.branch(my_tree.root());
    using oracle_list = oracle::datum_list<oracle_datum, test_nodes, test_list>;
    auto const *their_list = std::get_if<oracle_list>(&theirs.datum.inner);
    if (my_list.tag != cl::reader::datum_branch::list ||
        their_list == nullptr ||
        my_list.children.size() != their_list->elements.size()) {
        return false;
    }
    for (int i = 0; i < my_list.children.size(); ++i) {
        auto const *my_fix = std::get_if<cl::reader::datum_fixnum>(
            &my_tree.leaf(my_list.children[i]));
        auto const *their_fix = std::get_if<oracle::datum_integer>(
            &arena.get(their_list->elements[i]).inner);
        if (my_fix == nullptr || their_fix == nullptr ||
            my_fix->value != their_fix->value) {
            return false;
        }
    }
    return true;
}

template <class OracleKind>
auto agree_on_wrapper(std::string_view text, cl::reader::datum_branch kind)
    -> bool {
    sym_table syms;
    oracle_arena arena;
    auto const mine = new_read(text, syms);
    auto const theirs = oracle_read(text, arena);
    if (!mine.has_value() || !theirs.ok) {
        return false;
    }
    auto const &my_tree = mine.value();
    return !my_tree.is_leaf(my_tree.root()) &&
           my_tree.branch(my_tree.root()).tag == kind &&
           my_tree.branch(my_tree.root()).children.size() == 1 &&
           std::holds_alternative<OracleKind>(theirs.datum.inner);
}

} // namespace

TEST_CASE("OracleCompareTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("OracleCompareTest - Fixnums") {
    CHECK(agree_on_fixnum("42"));
    CHECK(agree_on_fixnum("-7"));
    CHECK(agree_on_fixnum("0"));
    CHECK(agree_on_fixnum("  42"));
    CHECK(agree_on_fixnum("; comment\n42"));
}

TEST_CASE("OracleCompareTest - SymbolsFoldAlike") {
    CHECK(agree_on_symbol("foo"));
    CHECK(agree_on_symbol("cAr"));
    CHECK(agree_on_symbol("t"));
    CHECK(agree_on_symbol("nil"));
    CHECK(agree_on_symbol("+"));
    // DIV-0003, whole-token classification, must not regress: 1+ is a
    // symbol in both readers.
    CHECK(agree_on_symbol("1+"));
    CHECK(agree_on_symbol("2buffer"));
}

TEST_CASE("OracleCompareTest - Keywords") {
    CHECK(agree_on_keyword(":foo"));
    CHECK(agree_on_keyword(":Bar"));
}

TEST_CASE("OracleCompareTest - Lists") {
    CHECK(agree_on_list_of_fixnums("()"));
    CHECK(agree_on_list_of_fixnums("(1 2 3)"));
    CHECK(agree_on_list_of_fixnums("( 1  2 )"));
    CHECK(agree_on_list_of_fixnums("(1 ; two\n 2)"));
}

TEST_CASE("OracleCompareTest - QuoteFamily") {
    using oracle_quote = oracle::datum_quote<oracle_datum, test_nodes>;
    using oracle_function = oracle::datum_function<oracle_datum, test_nodes>;
    using oracle_backquote = oracle::datum_backquote<oracle_datum, test_nodes>;
    using oracle_unquote = oracle::datum_unquote<oracle_datum, test_nodes>;
    using oracle_splice =
        oracle::datum_unquote_splice<oracle_datum, test_nodes>;
    CHECK(
        agree_on_wrapper<oracle_quote>("'x", cl::reader::datum_branch::quote));
    CHECK(agree_on_wrapper<oracle_function>(
        "#'car", cl::reader::datum_branch::function));
    CHECK(agree_on_wrapper<oracle_backquote>(
        "`x", cl::reader::datum_branch::backquote));
    CHECK(agree_on_wrapper<oracle_unquote>(",x",
                                           cl::reader::datum_branch::unquote));
    CHECK(agree_on_wrapper<oracle_splice>(
        ",@x", cl::reader::datum_branch::unquote_splice));
}

TEST_CASE("OracleCompareTest - ErrorsAgree") {
    CHECK(both_fail(""));
    CHECK(both_fail(")"));
    CHECK(both_fail("(1 2"));
    CHECK(both_fail("'"));
    CHECK(both_fail("("));
}
