// src/smd/cl/reader/readtable.test.cpp                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/reader/readtable.hpp>
#include <smd/cl/reader/readtable.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::cl::reader::macro_kind;
using smd::cl::reader::readtable;
using smd::cl::reader::sharpsign_kind;
using smd::cl::reader::standard_readtable;
using smd::cl::reader::syntax_type;

namespace {

constexpr auto default_table_is_all_constituent() -> bool {
    readtable const rt;
    return rt.syntax_of('(') == syntax_type::constituent &&
           rt.macro_of('(') == macro_kind::none &&
           rt.sharp_of('\'') == sharpsign_kind::none && !rt.is_whitespace(' ');
}

constexpr auto standard_whitespace() -> bool {
    return standard_readtable.is_whitespace(' ') &&
           standard_readtable.is_whitespace('\t') &&
           standard_readtable.is_whitespace('\n') &&
           standard_readtable.is_whitespace('\r') &&
           standard_readtable.is_whitespace('\f') &&
           !standard_readtable.is_whitespace('a');
}

constexpr auto standard_terminating_macros() -> bool {
    return standard_readtable.macro_of('(') == macro_kind::left_paren &&
           standard_readtable.macro_of(')') == macro_kind::right_paren &&
           standard_readtable.macro_of('\'') == macro_kind::single_quote &&
           standard_readtable.macro_of(';') == macro_kind::semicolon &&
           standard_readtable.macro_of('"') == macro_kind::double_quote &&
           standard_readtable.macro_of('`') == macro_kind::backquote &&
           standard_readtable.macro_of(',') == macro_kind::comma &&
           standard_readtable.syntax_of('(') == syntax_type::terminating_macro;
}

constexpr auto sharpsign_is_non_terminating() -> bool {
    // `#` is macro only at token start: mid-token it continues the token,
    // which is what is_token_constituent reports.
    return standard_readtable.macro_of('#') == macro_kind::sharpsign &&
           standard_readtable.syntax_of('#') ==
               syntax_type::non_terminating_macro &&
           standard_readtable.is_token_constituent('#') &&
           standard_readtable.is_token_constituent('a') &&
           !standard_readtable.is_token_constituent('(') &&
           !standard_readtable.is_token_constituent(' ');
}

constexpr auto standard_escapes() -> bool {
    return standard_readtable.syntax_of('\\') == syntax_type::single_escape &&
           standard_readtable.syntax_of('|') == syntax_type::multiple_escape;
}

constexpr auto standard_sharpsign_dispatch() -> bool {
    return standard_readtable.sharp_of('\'') ==
               sharpsign_kind::function_quote &&
           standard_readtable.sharp_of('(') == sharpsign_kind::vector_open &&
           standard_readtable.sharp_of('\\') ==
               sharpsign_kind::character_literal &&
           standard_readtable.sharp_of('b') == sharpsign_kind::radix_binary &&
           standard_readtable.sharp_of('B') == sharpsign_kind::radix_binary &&
           standard_readtable.sharp_of('o') == sharpsign_kind::radix_octal &&
           standard_readtable.sharp_of('x') == sharpsign_kind::radix_hex &&
           standard_readtable.sharp_of('X') == sharpsign_kind::radix_hex &&
           standard_readtable.sharp_of('r') == sharpsign_kind::radix_n &&
           standard_readtable.sharp_of('R') == sharpsign_kind::radix_n &&
           standard_readtable.sharp_of('|') == sharpsign_kind::block_comment &&
           standard_readtable.sharp_of('z') == sharpsign_kind::none;
}

constexpr auto table_is_mutable_data() -> bool {
    // set-macro-character's substrate: dispatch is table state, so
    // changing the table changes dispatch — no reader rewrite involved.
    readtable rt = standard_readtable;
    rt.set_macro('[', syntax_type::terminating_macro, macro_kind::left_paren);
    return rt.macro_of('[') == macro_kind::left_paren &&
           standard_readtable.macro_of('[') == macro_kind::none;
}

constexpr auto high_characters_are_constituents() -> bool {
    return standard_readtable.syntax_of('\x80') == syntax_type::constituent &&
           standard_readtable.macro_of('\xff') == macro_kind::none;
}

} // namespace

static_assert(default_table_is_all_constituent());
static_assert(standard_whitespace());
static_assert(standard_terminating_macros());
static_assert(sharpsign_is_non_terminating());
static_assert(standard_escapes());
static_assert(standard_sharpsign_dispatch());
static_assert(table_is_mutable_data());
static_assert(high_characters_are_constituents());

TEST_CASE("ReadtableTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ReadtableTest - StandardSyntax") {
    CHECK(standard_whitespace());
    CHECK(standard_terminating_macros());
    CHECK(sharpsign_is_non_terminating());
    CHECK(standard_escapes());
}

TEST_CASE("ReadtableTest - SharpsignDispatch") {
    CHECK(standard_sharpsign_dispatch());
}

TEST_CASE("ReadtableTest - TableIsData") {
    CHECK(default_table_is_all_constituent());
    CHECK(table_is_mutable_data());
    CHECK(high_characters_are_constituents());
}
