// src/smd/smdscheme/closure/value.test.cpp                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/elaborator/elaborated_core.hpp>
using core = smd::smdscheme::elaborator::core_type<32, 16>;
#include <smd/smdscheme/closure/value.hpp>
#include <smd/smdscheme/closure/value.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {
using namespace smd::smdscheme;
using namespace std::string_view_literals;

static_assert([] {
    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  8>();
    auto r = e.lookup("+"sv);
    return r.has_value() &&
           std::holds_alternative<smd::smdscheme::closure::builtin>(
               r.value()) &&
           std::get<smd::smdscheme::closure::builtin>(r.value()).op ==
               smd::smdscheme::closure::builtin_op::add;
}());

static_assert([] {
    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  8>();
    auto r = e.lookup("*"sv);
    return r.has_value() &&
           std::holds_alternative<smd::smdscheme::closure::builtin>(
               r.value()) &&
           std::get<smd::smdscheme::closure::builtin>(r.value()).op ==
               smd::smdscheme::closure::builtin_op::multiply;
}());

static_assert([] {
    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  8>();
    auto r = e.lookup("x"sv);
    return !r.has_value();
}());

static_assert([] {
    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  8>();
    e.define("x"sv,
             smd::smdscheme::closure::value<elaborator::core_type<32, 16>>{42});
    auto r = e.lookup("x"sv);
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 42;
}());

static_assert([] {
    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  8>();
    e.define("x"sv,
             smd::smdscheme::closure::value<elaborator::core_type<32, 16>>{1});
    e.define("x"sv,
             smd::smdscheme::closure::value<elaborator::core_type<32, 16>>{2});
    auto r = e.lookup("x"sv);
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 2;
}());

} // namespace

TEST_CASE("ValueTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ValueTest - DefaultEnvHasAdd") {
    using namespace smd::smdscheme;
    using namespace std::string_view_literals;
    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  8>();
    auto r = e.lookup("+"sv);
    REQUIRE(r.has_value());
    REQUIRE(
        std::holds_alternative<smd::smdscheme::closure::builtin>(r.value()));
    REQUIRE(std::get<smd::smdscheme::closure::builtin>(r.value()).op ==
            smd::smdscheme::closure::builtin_op::add);
}

TEST_CASE("ValueTest - DefaultEnvHasMultiply") {
    using namespace smd::smdscheme;
    using namespace std::string_view_literals;
    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  8>();
    auto r = e.lookup("*"sv);
    REQUIRE(r.has_value());
    REQUIRE(
        std::holds_alternative<smd::smdscheme::closure::builtin>(r.value()));
    REQUIRE(std::get<smd::smdscheme::closure::builtin>(r.value()).op ==
            smd::smdscheme::closure::builtin_op::multiply);
}

TEST_CASE("ValueTest - UnboundVariableIsError") {
    using namespace smd::smdscheme;
    using namespace std::string_view_literals;
    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  8>();
    auto r = e.lookup("x"sv);
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("ValueTest - DefineAndLookup") {
    using namespace smd::smdscheme;
    using namespace std::string_view_literals;
    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  8>();
    e.define("x"sv,
             smd::smdscheme::closure::value<elaborator::core_type<32, 16>>{42});
    auto r = e.lookup("x"sv);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 42);
}

TEST_CASE("ValueTest - DefineShadows") {
    using namespace smd::smdscheme;
    using namespace std::string_view_literals;
    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  8>();
    e.define("x"sv,
             smd::smdscheme::closure::value<elaborator::core_type<32, 16>>{1});
    e.define("x"sv,
             smd::smdscheme::closure::value<elaborator::core_type<32, 16>>{2});
    auto r = e.lookup("x"sv);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
}
