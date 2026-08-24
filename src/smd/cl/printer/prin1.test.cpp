// src/smd/cl/printer/prin1.test.cpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/printer/prin1.hpp>
#include <smd/cl/printer/prin1.hpp> // test 2nd include OK

#include <smd/cl/reader/read.hpp>
#include <smd/cl/symbol/symbol_table.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <span>
#include <string_view>

using smd::cl::printer::prin1;
using smd::cl::printer::print_text;
using smd::cl::reader::datum_tree;
using smd::cl::reader::read;

namespace {

// Slot types are later stages' business (R2); ints stand in, as
// reader/read.test.cpp's own sym_table does.
using sym_table = smd::cl::symbol::symbol_table<int, int, int, 64, 512>;

constexpr int test_nodes = 96;
constexpr int test_list = 8;
using tree = datum_tree<test_nodes, test_list>;

[[nodiscard]] constexpr auto text_view(print_text const &text) -> std::string_view {
    return std::string_view(text.begin(), static_cast<std::size_t>(text.size()));
}

/// Reads @p source, renders it, and checks the render against @p expected —
/// the round trip every row of prin1.hpp's own doc table pins.
[[nodiscard]] constexpr auto prints_as(std::string_view source,
                                       std::string_view expected) -> bool {
    sym_table syms;
    auto const parsed = read<test_nodes, test_list>(source, syms);
    if (!parsed.has_value()) {
        return false;
    }
    auto const printed = prin1(parsed.value(), syms);
    return printed.has_value() && text_view(printed.value()) == expected;
}

constexpr auto fixnum_prints_decimal_digits() -> bool {
    return prints_as("42", "42") && prints_as("-7", "-7") &&
           prints_as("0", "0");
}

constexpr auto symbol_prints_interned_name() -> bool {
    return prints_as("foo", "FOO") && prints_as("Multi-Word", "MULTI-WORD");
}

constexpr auto keyword_prints_name_with_colon() -> bool {
    return prints_as(":foo", ":FOO");
}

constexpr auto string_escapes_quote_and_backslash() -> bool {
    return prints_as(R"("hello")", R"("hello")") &&
           prints_as("\"a\\\"b\"", "\"a\\\"b\"") &&
           prints_as("\"a\\\\b\"", "\"a\\\\b\"");
}

constexpr auto character_prints_sharp_backslash_then_char() -> bool {
    return prints_as("#\\a", "#\\a") && prints_as("#\\9", "#\\9");
}

// A tower renders its stored spelling, not its evaluated value (decision
// D19) -- see docs/compiler_architecture.org's printer subsection for why
// this disagrees with an oracle that evaluates.
constexpr auto tower_prints_its_spelling() -> bool {
    return prints_as("100000000000", "100000000000") &&
           prints_as("1/2", "1/2") && prints_as("1.5", "1.5");
}

constexpr auto empty_list_prints_as_nil() -> bool {
    return prints_as("()", "NIL");
}

constexpr auto nonempty_list_prints_space_separated() -> bool {
    return prints_as("(1 2 3)", "(1 2 3)") &&
           prints_as("(1 (2 3) 4)", "(1 (2 3) 4)");
}

constexpr auto vector_prints_sharp_paren() -> bool {
    return prints_as("#(1 2 3)", "#(1 2 3)");
}

constexpr auto quote_prints_as_quote_list() -> bool {
    return prints_as("'x", "(QUOTE X)") &&
           prints_as("''x", "(QUOTE (QUOTE X))");
}

constexpr auto function_prints_as_function_list() -> bool {
    return prints_as("#'car", "(FUNCTION CAR)");
}

// backquote/unquote/unquote-splice are total (every branch renders to
// something) but excluded from every SBCL comparison: SBCL 2.2.9 prints
// these as implementation-specific reader-macro objects, which ANSI
// permits, so there is nothing canonical to compare against.
constexpr auto backquote_family_is_total() -> bool {
    return prints_as("`x", "`X") && prints_as(",x", ",X") &&
           prints_as(",@x", ",@X");
}

/// 64 characters of `'` followed by `x`: nesting `(QUOTE ...)` 64 deep
/// grows the rendered text by 8 characters per level from "X" (1 char),
/// so the 64th wrap is the first to exceed max_print_chars (512):
/// 1 + 8*63 = 505 fits, 1 + 8*64 = 513 does not.
[[nodiscard]] constexpr auto sixty_four_nested_quotes() -> std::array<char, 65> {
    std::array<char, 65> source{};
    std::ranges::fill(std::span(source).first(64), '\'');
    source[64] = 'x';
    return source;
}

constexpr auto overflow_is_diagnosed() -> bool {
    constexpr auto source_chars = sixty_four_nested_quotes();
    sym_table syms;
    auto const parsed = read<test_nodes, test_list>(
        std::string_view(source_chars.data(), source_chars.size()), syms);
    if (!parsed.has_value()) {
        return false; // not what this test means to exercise
    }
    auto const printed = prin1(parsed.value(), syms);
    return !printed.has_value();
}

} // namespace

static_assert(fixnum_prints_decimal_digits());
static_assert(symbol_prints_interned_name());
static_assert(keyword_prints_name_with_colon());
static_assert(string_escapes_quote_and_backslash());
static_assert(character_prints_sharp_backslash_then_char());
static_assert(tower_prints_its_spelling());
static_assert(empty_list_prints_as_nil());
static_assert(nonempty_list_prints_space_separated());
static_assert(vector_prints_sharp_paren());
static_assert(quote_prints_as_quote_list());
static_assert(function_prints_as_function_list());
static_assert(backquote_family_is_total());
static_assert(overflow_is_diagnosed());

TEST_CASE("Prin1Test - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("Prin1Test - AtomsRenderPerTheDocTable") {
    CHECK(fixnum_prints_decimal_digits());
    CHECK(symbol_prints_interned_name());
    CHECK(keyword_prints_name_with_colon());
    CHECK(string_escapes_quote_and_backslash());
    CHECK(character_prints_sharp_backslash_then_char());
    CHECK(tower_prints_its_spelling());
}

TEST_CASE("Prin1Test - CompoundsRenderPerTheDocTable") {
    CHECK(empty_list_prints_as_nil());
    CHECK(nonempty_list_prints_space_separated());
    CHECK(vector_prints_sharp_paren());
    CHECK(quote_prints_as_quote_list());
    CHECK(function_prints_as_function_list());
    CHECK(backquote_family_is_total());
}

TEST_CASE("Prin1Test - OverflowIsDiagnosedNotAsserted") {
    CHECK(overflow_is_diagnosed());
}
