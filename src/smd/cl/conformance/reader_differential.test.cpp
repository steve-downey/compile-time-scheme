// src/smd/cl/conformance/reader_differential.test.cpp             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// The printer's first real oracle comparison (decision D16), landed in the
// same step as printer::prin1 itself rather than ahead of it: a printer
// whose only witness is a test written against its own output is exactly
// the evidence D16 rejects. A3 deleted reader/oracle_compare.test.cpp's
// other half along with the rest of smdlisp, so between A3 and this file
// the reader had no differential oracle at all.
//
// For each source string, this reads with this project's own reader,
// renders with printer::prin1, and compares against what SBCL's own reader
// and prin1 produce for the same string via sbcl_read_print. Neither side
// evaluates anything -- this is a differential on read+print alone, unlike
// sbcl_differential.test.cpp beside it, which compares evaluated values.
//
// A genuine runtime test: shelling out to a subprocess has no constexpr
// twin, and none is claimed. It is a member of the default suite
// (`make test`) per the orchestrator's decision, so it must SKIP rather
// than fail when `sbcl` is not on PATH -- a missing oracle is a fact about
// the environment, not a defect in this project.
//
// A5 broadens A4's eleven cases into a real corpus, closing the gap A3's
// deletion of oracle_compare.test.cpp opened: `inherited_cases` below is
// every input that retired file tested (fixnums, fold-alike symbols,
// keywords, lists, and the non-excluded quote family), now checked against
// SBCL instead of against smdlisp; `new_coverage_cases` is syntax smdlisp
// never had at all, because D10 put it out of that iteration's scope --
// strings with escapes, character literals, vectors, and canonical decimal
// tower spellings.
//
// Three kinds of input stay out of both tables, carried forward from A4's
// own exclusions so nobody re-adds them:
//   - backquote, unquote and unquote-splice: SBCL renders these as
//     `SB-INT:QUASIQUOTE` and `#S(SB-IMPL::COMMA ...)`, an implementation
//     detail ANSI permits and this project's own printer does not imitate;
//   - non-decimal and non-canonical numeric tower spellings (`#x1f`, `2/4`,
//     `1.50`): this printer renders the stored spelling, SBCL's `prin1`
//     renders the value the spelling denotes, and the two only have to
//     agree on canonical decimal spellings;
//   - `#\Space`: SBCL 2.2.9 prints it as `#\` followed by a literal space
//     rather than by name, while this printer has no character-name table
//     at all, so the two agree on ordinary characters by design and would
//     disagree here by accident of one SBCL build's naming choice.

#include <smd/cl/conformance/sbcl_oracle.hpp>
#include <smd/cl/conformance/sbcl_oracle.hpp> // test 2nd include OK

#include <smd/cl/printer/prin1.hpp>
#include <smd/cl/reader/read.hpp>
#include <smd/cl/symbol/symbol_table.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <string_view>

using smd::cl::conformance::find_sbcl_version;
using smd::cl::conformance::sbcl_read_print;
using smd::cl::printer::prin1;
using smd::cl::reader::datum_tree;
using smd::cl::reader::read;

namespace {

// Slot types are later stages' business (R2); ints stand in, as
// reader/read.test.cpp's own sym_table does.
using sym_table = smd::cl::symbol::symbol_table<int, int, int, 64, 512>;

// Widened from A4's 32/8: a broadened corpus's vectors and nested lists
// push closer to the per-list element cap than a handful of cases ever
// did, and the failure mode of tripping it is an `add_leaf`/`add_branch`
// precondition, not a diagnosed error -- so the margin is deliberate.
constexpr int test_nodes = 64;
constexpr int test_list = 16;
using tree = datum_tree<test_nodes, test_list>;

/// Reads and renders @p source with this project's own reader and printer,
/// then checks the render against SBCL's own read+prin1 of the same
/// string.
void check_against_sbcl(std::string_view source) {
    INFO(source);
    sym_table syms;
    auto const parsed = read<test_nodes, test_list>(source, syms);
    REQUIRE(parsed.has_value());
    auto const printed = prin1(parsed.value(), syms);
    REQUIRE(printed.has_value());
    std::string const ours(printed.value().begin(), printed.value().end());
    auto const theirs = sbcl_read_print(source);
    REQUIRE(theirs.has_value());
    CHECK(ours == *theirs);
}

/// One corpus entry: a short label for the failing case a test run
/// reports, and the source string itself.
struct reader_case {
    std::string_view label;
    std::string_view source;
};

/// Checks every entry of @p table against SBCL.
void check_all(std::span<reader_case const> table) {
    std::ranges::for_each(table, [](reader_case const &c) {
        INFO(c.label);
        check_against_sbcl(c.source);
    });
}

// 05847dd9-bb9c-4e88-bb71-734ff3f3bf8a
// Every input `reader/oracle_compare.test.cpp` tested, before A3 deleted it
// along with the rest of smdlisp, now checked against SBCL instead.
constexpr std::array<reader_case, 21> inherited_cases{{
    {"Fixnums: bare", "42"},
    {"Fixnums: negative", "-7"},
    {"Fixnums: zero", "0"},
    {"Fixnums: leading whitespace", "  42"},
    {"Fixnums: leading comment", "; comment\n42"},
    {"SymbolsFoldAlike: lowercase", "foo"},
    {"SymbolsFoldAlike: mixed case", "cAr"},
    {"SymbolsFoldAlike: t", "t"},
    {"SymbolsFoldAlike: nil", "nil"},
    {"SymbolsFoldAlike: plus", "+"},
    {"SymbolsFoldAlike: whole-token 1+", "1+"},
    {"SymbolsFoldAlike: whole-token 2buffer", "2buffer"},
    {"Keywords: foo", ":foo"},
    {"Keywords: mixed case", ":Bar"},
    {"Lists: empty", "()"},
    {"Lists: flat", "(1 2 3)"},
    {"Lists: extra intertoken space", "( 1  2 )"},
    {"Lists: comment between elements", "(1 ; two\n 2)"},
    {"Lists: nested", "(1 (2 3) 4)"},
    {"QuoteFamily: quote", "'x"},
    {"QuoteFamily: function", "#'car"},
}};

// New coverage: syntax the retired oracle_compare.test.cpp never exercised,
// because smdlisp never had it (D10 put strings, characters, vectors and
// the numeric tower out of that iteration's scope).
constexpr std::array<reader_case, 20> new_coverage_cases{{
    {"Strings: empty", R"("")"},
    {"Strings: plain", R"("hello")"},
    {"Strings: embedded quote escape", R"("a\"b")"},
    {"Strings: embedded backslash escape", R"("a\\b")"},
    {"Strings: embedded space", R"("multi word")"},
    {"Strings: semicolon is not a comment inside a string",
     R"("; not a comment")"},
    {"Characters: lowercase letter", R"(#\a)"},
    {"Characters: uppercase letter", R"(#\A)"},
    {"Characters: digit", R"(#\1)"},
    {"Characters: open paren", R"(#\()"},
    {"Characters: semicolon", R"(#\;)"},
    {"Vectors: flat", "#(1 2 3)"},
    {"Vectors: empty", "#()"},
    {"Vectors: nested list element", "#(1 (2 3) 3)"},
    {"Vectors: keyword elements", "#(:a :b)"},
    {"DecimalTower: float", "1.5"},
    {"DecimalTower: negative float", "-0.5"},
    {"DecimalTower: trailing-zero float", "3.0"},
    {"DecimalTower: ratio", "1/2"},
    {"DecimalTower: negative ratio", "-3/4"},
}};
// 05847dd9-bb9c-4e88-bb71-734ff3f3bf8a end

/// Checks that @p source is diagnosed by this project's own reader, and
/// separately that SBCL's `read-from-string` signals on the same input --
/// without comparing message text, since `cl`'s diagnostics are its own
/// (DIV-0027).
void check_read_fails_and_sbcl_errors(std::string_view source) {
    INFO(source);
    sym_table syms;
    auto const parsed = read<test_nodes, test_list>(source, syms);
    CHECK_FALSE(parsed.has_value());
    auto const theirs = sbcl_read_print(source);
    REQUIRE(theirs.has_value());
    CHECK(*theirs == "SBCL-ERROR");
}

// b8d41e67-1ab8-4b59-b1be-e5f50d06af4b
// The retired oracle_compare.test.cpp's ErrorsAgree inputs: both readers
// must reject every one of these. sbcl_read_print reports a signalling
// read as the literal sentinel "SBCL-ERROR" (confirmed against SBCL
// 2.2.9.debian for all five), never as an absent optional -- nullopt there
// is reserved for the sbcl binary itself being unreachable.
constexpr std::array<std::string_view, 5> error_cases{"", ")", "(1 2", "'",
                                                      "("};
// b8d41e67-1ab8-4b59-b1be-e5f50d06af4b end

} // namespace

TEST_CASE("ReaderDifferentialTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ReaderDifferentialTest - ReadAndPrintAgreeWithSbcl") {
    auto const version = find_sbcl_version();
    if (!version.has_value()) {
        SKIP("sbcl not found on PATH; differential check skipped");
    }
    INFO("oracle: " << *version);
    check_all(inherited_cases);
    check_all(new_coverage_cases);
}

TEST_CASE("ReaderDifferentialTest - ErrorsAgreeWithSbcl") {
    auto const version = find_sbcl_version();
    if (!version.has_value()) {
        SKIP("sbcl not found on PATH; differential check skipped");
    }
    INFO("oracle: " << *version);
    std::ranges::for_each(error_cases, [](std::string_view source) {
        check_read_fails_and_sbcl_errors(source);
    });
}
