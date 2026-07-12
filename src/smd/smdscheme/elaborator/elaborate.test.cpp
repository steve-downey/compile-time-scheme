// src/smd/smdscheme/elaborator/elaborate.test.cpp -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/elaborator/elaborate.hpp>
#include <smd/smdscheme/elaborator/elaborate.hpp> // test 2nd include OK

#include <smd/smdscheme/parser/cursor.hpp>
#include <smd/smdscheme/reader/read_datum.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {
namespace scm = smd::smdscheme;
using Core = scm::elaborator::core_type<32, 16>;

/// Reads then elaborates @p src, returning the root core node or an error.
/// @p datum_arena and @p core_arena must outlive any use of the result.
auto elab(std::string_view src,
          scm::foundation::tree_arena<scm::reader::datum_type<32, 16>, 32>
              &datum_arena,
          scm::foundation::tree_arena<Core, 32> &core_arena)
    -> scm::foundation::result<Core> {
    auto dr =
        scm::reader::read_datum<32, 16>(scm::parser::cursor{src}, datum_arena);
    if (!dr.has_value())
        return dr.error();
    return scm::elaborator::elaborate<32, 16>(dr.value().value, datum_arena,
                                              core_arena);
}

} // namespace

TEST_CASE("ElaborateTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ElaborateTest - SetElaboratesToCoreSet") {
    scm::foundation::tree_arena<scm::reader::datum_type<32, 16>, 32> da;
    scm::foundation::tree_arena<Core, 32> ca;
    auto er = elab("(set! x 1)", da, ca);
    REQUIRE(er.has_value());
    REQUIRE(std::holds_alternative<scm::elaborator::core_set<Core, 32>>(
        er.value().inner));
}

TEST_CASE("ElaborateTest - BeginElaboratesToCoreBegin") {
    scm::foundation::tree_arena<scm::reader::datum_type<32, 16>, 32> da;
    scm::foundation::tree_arena<Core, 32> ca;
    auto er = elab("(begin 1 2 3)", da, ca);
    REQUIRE(er.has_value());
    REQUIRE(std::holds_alternative<scm::elaborator::core_begin<Core, 32, 16>>(
        er.value().inner));
}

TEST_CASE("ElaborateTest - SetWrongArityIsError") {
    scm::foundation::tree_arena<scm::reader::datum_type<32, 16>, 32> da;
    scm::foundation::tree_arena<Core, 32> ca;
    REQUIRE(!elab("(set! x)", da, ca).has_value());
}

TEST_CASE("ElaborateTest - SetNonSymbolNameIsError") {
    scm::foundation::tree_arena<scm::reader::datum_type<32, 16>, 32> da;
    scm::foundation::tree_arena<Core, 32> ca;
    REQUIRE(!elab("(set! 1 2)", da, ca).has_value());
}

TEST_CASE("ElaborateTest - BeginEmptyIsError") {
    scm::foundation::tree_arena<scm::reader::datum_type<32, 16>, 32> da;
    scm::foundation::tree_arena<Core, 32> ca;
    REQUIRE(!elab("(begin)", da, ca).has_value());
}

TEST_CASE("ElaborateTest - ConsElaboratesToCorePrim") {
    scm::foundation::tree_arena<scm::reader::datum_type<32, 16>, 32> da;
    scm::foundation::tree_arena<Core, 32> ca;
    auto er = elab("(cons 1 2)", da, ca);
    REQUIRE(er.has_value());
    using Prim = scm::elaborator::core_prim<Core, 32, 16>;
    REQUIRE(std::holds_alternative<Prim>(er.value().inner));
    REQUIRE(std::get<Prim>(er.value().inner).op ==
            scm::elaborator::prim_op::cons);
    REQUIRE(std::get<Prim>(er.value().inner).args.size() == 2);
}

TEST_CASE("ElaborateTest - QuotedListElaboratesWithoutError") {
    // '(1 2 3) previously errored ("lists/nested quotes not yet supported").
    // It now lowers to a construction expression (nested cons -> null).
    scm::foundation::tree_arena<scm::reader::datum_type<64, 16>, 64> da;
    scm::foundation::tree_arena<scm::elaborator::core_type<64, 16>, 64> ca;
    auto dr =
        scm::reader::read_datum<64, 16>(scm::parser::cursor{"'(1 2 3)"}, da);
    REQUIRE(dr.has_value());
    auto er = scm::elaborator::elaborate<64, 16>(dr.value().value, da, ca);
    REQUIRE(er.has_value());
    using Prim =
        scm::elaborator::core_prim<scm::elaborator::core_type<64, 16>, 64, 16>;
    REQUIRE(std::holds_alternative<Prim>(er.value().inner));
    // Outermost node is (cons 1 <rest>).
    REQUIRE(std::get<Prim>(er.value().inner).op ==
            scm::elaborator::prim_op::cons);
}

TEST_CASE("ElaborateTest - QuotedEmptyListElaboratesToNull") {
    scm::foundation::tree_arena<scm::reader::datum_type<32, 16>, 32> da;
    scm::foundation::tree_arena<Core, 32> ca;
    auto dr = scm::reader::read_datum<32, 16>(scm::parser::cursor{"'()"}, da);
    REQUIRE(dr.has_value());
    auto er = scm::elaborator::elaborate<32, 16>(dr.value().value, da, ca);
    REQUIRE(er.has_value());
    using Prim = scm::elaborator::core_prim<Core, 32, 16>;
    REQUIRE(std::holds_alternative<Prim>(er.value().inner));
    REQUIRE(std::get<Prim>(er.value().inner).op ==
            scm::elaborator::prim_op::null);
}
