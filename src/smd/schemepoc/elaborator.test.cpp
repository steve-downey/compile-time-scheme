// src/smd/schemepoc/elaborator.test.cpp                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/elaborator.hpp>
#include <smd/schemepoc/elaborator.hpp> // test 2nd include OK

#include <smd/schemepoc/reader.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {
using namespace smd::schemepoc;
using namespace std::string_view_literals;

// "42" -> core_integer{42} at root
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"42"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    if (ct.size() != 1)
        return false;
    return std::holds_alternative<core_integer>(ct.get(ct.size() - 1)) &&
           std::get<core_integer>(ct.get(ct.size() - 1)).value == 42;
}());

// "foo" -> core_symbol{"foo"} at root
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"foo"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    if (ct.size() != 1)
        return false;
    return std::holds_alternative<core_symbol>(ct.get(ct.size() - 1)) &&
           std::get<core_symbol>(ct.get(ct.size() - 1)).name == "foo"sv;
}());

// "#t" -> core_boolean{true} at root
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"#t"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    if (ct.size() != 1)
        return false;
    return std::holds_alternative<core_boolean>(ct.get(ct.size() - 1)) &&
           std::get<core_boolean>(ct.get(ct.size() - 1)).value == true;
}());

// "'x" -> core_quote at root wrapping the datum_symbol{"x"}
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"'x"sv});
    if (!dr.has_value())
        return false;
    auto const &dt = dr.value().value;
    auto er = elaborate(dt);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    if (ct.size() != 1)
        return false;
    auto const &root_node = ct.get(ct.size() - 1);
    if (!std::holds_alternative<core_quote>(root_node))
        return false;
    node_id quoted_datum = std::get<core_quote>(root_node).datum;
    auto const &quoted_node = dt.get(quoted_datum);
    return std::holds_alternative<datum_symbol>(quoted_node) &&
           std::get<datum_symbol>(quoted_node).name == "x"sv;
}());

// "(if #t 1 2)" -> core_if at root with correct children
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"(if #t 1 2)"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto const &root_node = ct.get(ct.size() - 1);
    if (!std::holds_alternative<core_if>(root_node))
        return false;
    auto const &cif = std::get<core_if>(root_node);
    auto const &cond = ct.get(cif.condition);
    auto const &cons = ct.get(cif.consequent);
    auto const &alt  = ct.get(cif.alternative);
    if (!std::holds_alternative<core_boolean>(cond))
        return false;
    if (std::get<core_boolean>(cond).value != true)
        return false;
    if (!std::holds_alternative<core_integer>(cons))
        return false;
    if (std::get<core_integer>(cons).value != 1)
        return false;
    if (!std::holds_alternative<core_integer>(alt))
        return false;
    return std::get<core_integer>(alt).value == 2;
}());

// "(lambda (x) x)" -> core_lambda at root with param "x", body core_symbol{"x"}
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"(lambda (x) x)"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto const &root_node = ct.get(ct.size() - 1);
    if (!std::holds_alternative<core_lambda<16>>(root_node))
        return false;
    auto const &lam = std::get<core_lambda<16>>(root_node);
    if (lam.params.size() != 1)
        return false;
    if (lam.params[0] != "x"sv)
        return false;
    auto const &body_node = ct.get(lam.body);
    return std::holds_alternative<core_symbol>(body_node) &&
           std::get<core_symbol>(body_node).name == "x"sv;
}());

// "(foo 1 2)" -> core_application at root
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"(foo 1 2)"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto const &root_node = ct.get(ct.size() - 1);
    if (!std::holds_alternative<core_application<16>>(root_node))
        return false;
    auto const &app = std::get<core_application<16>>(root_node);
    if (app.args.size() != 2)
        return false;
    auto const &func_node = ct.get(app.func);
    return std::holds_alternative<core_symbol>(func_node) &&
           std::get<core_symbol>(func_node).name == "foo"sv;
}());

// "(define x 42)" -> core_define at root
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"(define x 42)"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return false;
    auto const &ct = er.value();
    auto const &root_node = ct.get(ct.size() - 1);
    if (!std::holds_alternative<core_define>(root_node))
        return false;
    auto const &def = std::get<core_define>(root_node);
    if (def.name != "x"sv)
        return false;
    auto const &val_node = ct.get(def.value);
    return std::holds_alternative<core_integer>(val_node) &&
           std::get<core_integer>(val_node).value == 42;
}());

// "()" -> elaboration error (empty application)
static_assert([] {
    auto dr = read_datum<32, 16>(cursor{"()"sv});
    if (!dr.has_value())
        return false;
    auto er = elaborate(dr.value().value);
    return !er.has_value();
}());

} // namespace

TEST_CASE("ElaboratorTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ElaboratorTest - Integer") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"42"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    REQUIRE(ct.size() == 1);
    REQUIRE(std::holds_alternative<core_integer>(ct.get(ct.size() - 1)));
    REQUIRE(std::get<core_integer>(ct.get(ct.size() - 1)).value == 42);
}

TEST_CASE("ElaboratorTest - Symbol") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"foo"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    REQUIRE(std::holds_alternative<core_symbol>(ct.get(ct.size() - 1)));
    REQUIRE(std::get<core_symbol>(ct.get(ct.size() - 1)).name == "foo"sv);
}

TEST_CASE("ElaboratorTest - Boolean") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"#t"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    REQUIRE(std::holds_alternative<core_boolean>(ct.get(ct.size() - 1)));
    REQUIRE(std::get<core_boolean>(ct.get(ct.size() - 1)).value == true);
}

TEST_CASE("ElaboratorTest - QuoteShorthand") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"'x"sv});
    REQUIRE(dr.has_value());
    auto const &dt = dr.value().value;
    auto er = elaborate(dt);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    REQUIRE(std::holds_alternative<core_quote>(ct.get(ct.size() - 1)));
    node_id quoted_datum = std::get<core_quote>(ct.get(ct.size() - 1)).datum;
    REQUIRE(std::holds_alternative<datum_symbol>(dt.get(quoted_datum)));
    REQUIRE(std::get<datum_symbol>(dt.get(quoted_datum)).name == "x"sv);
}

TEST_CASE("ElaboratorTest - If") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"(if #t 1 2)"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto const &root_node = ct.get(ct.size() - 1);
    REQUIRE(std::holds_alternative<core_if>(root_node));
    auto const &cif = std::get<core_if>(root_node);
    REQUIRE(std::holds_alternative<core_boolean>(ct.get(cif.condition)));
    REQUIRE(std::get<core_boolean>(ct.get(cif.condition)).value == true);
    REQUIRE(std::holds_alternative<core_integer>(ct.get(cif.consequent)));
    REQUIRE(std::get<core_integer>(ct.get(cif.consequent)).value == 1);
    REQUIRE(std::holds_alternative<core_integer>(ct.get(cif.alternative)));
    REQUIRE(std::get<core_integer>(ct.get(cif.alternative)).value == 2);
}

TEST_CASE("ElaboratorTest - Lambda") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"(lambda (x) x)"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto const &root_node = ct.get(ct.size() - 1);
    REQUIRE(std::holds_alternative<core_lambda<16>>(root_node));
    auto const &lam = std::get<core_lambda<16>>(root_node);
    REQUIRE(lam.params.size() == 1);
    REQUIRE(lam.params[0] == "x"sv);
    REQUIRE(std::holds_alternative<core_symbol>(ct.get(lam.body)));
    REQUIRE(std::get<core_symbol>(ct.get(lam.body)).name == "x"sv);
}

TEST_CASE("ElaboratorTest - Application") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"(foo 1 2)"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto const &root_node = ct.get(ct.size() - 1);
    REQUIRE(std::holds_alternative<core_application<16>>(root_node));
    auto const &app = std::get<core_application<16>>(root_node);
    REQUIRE(app.args.size() == 2);
    REQUIRE(std::holds_alternative<core_symbol>(ct.get(app.func)));
    REQUIRE(std::get<core_symbol>(ct.get(app.func)).name == "foo"sv);
}

TEST_CASE("ElaboratorTest - Define") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"(define x 42)"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto const &root_node = ct.get(ct.size() - 1);
    REQUIRE(std::holds_alternative<core_define>(root_node));
    auto const &def = std::get<core_define>(root_node);
    REQUIRE(def.name == "x"sv);
    REQUIRE(std::holds_alternative<core_integer>(ct.get(def.value)));
    REQUIRE(std::get<core_integer>(ct.get(def.value)).value == 42);
}

TEST_CASE("ElaboratorTest - EmptyListIsError") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"()"sv});
    REQUIRE(dr.has_value());
    auto er = elaborate(dr.value().value);
    REQUIRE_FALSE(er.has_value());
}

TEST_CASE("ElaboratorTest - QuoteSpecialForm") {
    using namespace smd::schemepoc;
    using namespace std::string_view_literals;
    auto dr = read_datum<32, 16>(cursor{"(quote x)"sv});
    REQUIRE(dr.has_value());
    auto const &dt = dr.value().value;
    auto er = elaborate(dt);
    REQUIRE(er.has_value());
    auto const &ct = er.value();
    auto const &root_node = ct.get(ct.size() - 1);
    REQUIRE(std::holds_alternative<core_quote>(root_node));
    node_id quoted_datum = std::get<core_quote>(root_node).datum;
    REQUIRE(std::holds_alternative<datum_symbol>(dt.get(quoted_datum)));
    REQUIRE(std::get<datum_symbol>(dt.get(quoted_datum)).name == "x"sv);
}
