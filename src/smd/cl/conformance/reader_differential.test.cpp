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

#include <smd/cl/conformance/sbcl_oracle.hpp>
#include <smd/cl/conformance/sbcl_oracle.hpp> // test 2nd include OK

#include <smd/cl/printer/prin1.hpp>
#include <smd/cl/reader/read.hpp>
#include <smd/cl/symbol/symbol_table.hpp>

#include <catch2/catch_test_macros.hpp>

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

constexpr int test_nodes = 32;
constexpr int test_list = 8;
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

} // namespace

TEST_CASE("ReaderDifferentialTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ReaderDifferentialTest - ReadAndPrintAgreeWithSbcl") {
    auto const version = find_sbcl_version();
    if (!version.has_value()) {
        SKIP("sbcl not found on PATH; differential check skipped");
    }
    INFO("oracle: " << *version);
    check_against_sbcl("42");
    check_against_sbcl("foo");
    check_against_sbcl(":foo");
    check_against_sbcl(R"("a\"b")");
    check_against_sbcl("#\\a");
    check_against_sbcl("(1 2 3)");
    check_against_sbcl("(1 (2 3) 4)");
    check_against_sbcl("()");
    check_against_sbcl("#(1 2 3)");
    check_against_sbcl("'x");
    check_against_sbcl("1+");
}
