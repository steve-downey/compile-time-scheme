// src/smd/smdlisp/closure/cps_code.test.cpp                      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Lifetime discipline (see closure_program.test.cpp's file comment for the
// full story): @ref lisp::closure::cps_of / @ref lisp::closure::compile_cps
// capture their @c node/@c arena arguments BY VALUE, so the returned
// `cps_code` must be declared directly in the TEST_CASE/static_assert body
// that evaluates it -- never returned out of a nested helper function,
// which would let the helper's stack frame (and the copies it captured)
// go away before the caller inspects a `symbol`/`keyword` result's
// `std::string_view`. Only "read + elaborate" (populating a caller-owned
// `root`) and "evaluate an already-compiled `code`" are factored into
// helpers below; compiling is always inline in the test body.

#include <smd/smdlisp/closure/cps_code.hpp>
#include <smd/smdlisp/closure/cps_code.hpp> // test 2nd include OK

#include <smd/smdlisp/reader/read_datum.hpp>
#include <smd/smdscheme/parser/cursor.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {
namespace lisp = smd::smdlisp;
namespace scm = smd::smdscheme;
using namespace std::string_view_literals;

constexpr int MaxNodes = 128;
constexpr int MaxList = 16;
constexpr int MaxBindings = 16;
constexpr int MaxEnvs = 32;

using Core = lisp::elaborator::core_type<MaxNodes, MaxList>;
using Val = lisp::closure::value<Core>;
using Res = scm::foundation::result<Val>;
using DatumArena =
    scm::foundation::tree_arena<lisp::reader::datum_type<MaxNodes, MaxList>,
                                MaxNodes>;
using CoreArena = scm::foundation::tree_arena<Core, MaxNodes>;
using Heap = lisp::closure::pair_heap<Core, lisp::closure::default_max_pairs>;
using Envs = lisp::closure::env_arena<Core, MaxBindings, MaxEnvs>;

/// Reads and elaborates @p src, populating caller-owned @p root on
/// success. Arenas and @p root are caller-owned (per L10/L11's
/// stack-use-after-return lesson): this function never returns a value
/// derived from them, only a plain @c bool.
constexpr auto elaborate_into(std::string_view src, DatumArena &datum_arena,
                              CoreArena &core_arena, Core &root) -> bool {
    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{src}, datum_arena);
    if (!dr.has_value())
        return false;
    auto er = lisp::elaborator::elaborate<MaxNodes, MaxList>(
        dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return false;
    root = er.value();
    return true;
}

/// Evaluates an already-compiled @p code under a default environment
/// sharing @p heap and @p envs.
template <class Code>
constexpr auto eval(Code &code, Heap &heap, Envs &envs) -> Res {
    auto environment = lisp::closure::default_env<Core, MaxBindings>(heap);
    return code(environment, envs, lisp::closure::detail::identity_k<Core>{});
}

} // namespace

TEST_CASE("CpsCodeTest - HeaderIsIdempotent") { REQUIRE(true); }

// -- breathing tests over compile_cps / cps_of ----------------------------

static_assert([] {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Envs envs;
    Core root;
    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{"42"sv}, da);
    if (!dr.has_value())
        return false;
    auto er = lisp::elaborator::elaborate<MaxNodes, MaxList>(dr.value().value,
                                                             da, ca);
    if (!er.has_value())
        return false;
    root = er.value();
    auto code = lisp::closure::compile_cps<MaxNodes, MaxList>(root, ca);
    auto environment = lisp::closure::default_env<Core, MaxBindings>(heap);
    auto vr =
        code(environment, envs, lisp::closure::detail::identity_k<Core>{});
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 42;
}());

static_assert([] {
    // cps_of with an explicit identity cont produces the same result as
    // compile_cps.
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Envs envs;
    Core root;
    auto dr = lisp::reader::read_datum<MaxNodes, MaxList>(
        scm::parser::cursor{"(+ 2 3)"sv}, da);
    if (!dr.has_value())
        return false;
    auto er = lisp::elaborator::elaborate<MaxNodes, MaxList>(dr.value().value,
                                                             da, ca);
    if (!er.has_value())
        return false;
    root = er.value();
    auto code = lisp::closure::cps_of<MaxNodes, MaxList>(
        root, ca, lisp::closure::detail::identity_k<Core>{});
    auto environment = lisp::closure::default_env<Core, MaxBindings>(heap);
    auto vr =
        code(environment, envs, lisp::closure::detail::identity_k<Core>{});
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 5;
}());

TEST_CASE("CpsCodeTest - Integer") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    REQUIRE(elaborate_into("42", da, ca, root));
    auto code = lisp::closure::compile_cps<MaxNodes, MaxList>(root, ca);
    auto r = eval(code, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 42);
}

TEST_CASE("CpsCodeTest - IfTakesConsequentInTailPosition") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    REQUIRE(elaborate_into("(if t (+ 1 2) 0)", da, ca, root));
    auto code = lisp::closure::compile_cps<MaxNodes, MaxList>(root, ca);
    auto r = eval(code, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
}

TEST_CASE("CpsCodeTest - IfTakesAlternative") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    REQUIRE(elaborate_into("(if nil 1 2)", da, ca, root));
    auto code = lisp::closure::compile_cps<MaxNodes, MaxList>(root, ca);
    auto r = eval(code, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
}

TEST_CASE("CpsCodeTest - PrognChainsIntoLastExpression") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    REQUIRE(elaborate_into("(progn 1 2 3)", da, ca, root));
    auto code = lisp::closure::compile_cps<MaxNodes, MaxList>(root, ca);
    auto r = eval(code, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 3);
}

TEST_CASE("CpsCodeTest - LambdaEvaluatesToClosure") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    REQUIRE(elaborate_into("(lambda (x) x)", da, ca, root));
    auto code = lisp::closure::compile_cps<MaxNodes, MaxList>(root, ca);
    auto r = eval(code, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::closure<Core>>(r.value()));
}

TEST_CASE("CpsCodeTest - ApplicationOfLambda") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    REQUIRE(elaborate_into("((lambda (x) (+ x 1)) 41)", da, ca, root));
    auto code = lisp::closure::compile_cps<MaxNodes, MaxList>(root, ca);
    auto r = eval(code, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 42);
}

TEST_CASE("CpsCodeTest - QuoteEvaluatesToSymbol") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    REQUIRE(elaborate_into("(quote abc)", da, ca, root));
    auto code = lisp::closure::compile_cps<MaxNodes, MaxList>(root, ca);
    auto r = eval(code, heap, envs);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<lisp::closure::symbol>(r.value()));
    REQUIRE(std::get<lisp::closure::symbol>(r.value()).name == "ABC");
}

TEST_CASE("CpsCodeTest - UnboundVariableIsError") {
    DatumArena da;
    CoreArena ca;
    Heap heap;
    Core root;
    Envs envs;
    REQUIRE(elaborate_into("nope", da, ca, root));
    auto code = lisp::closure::compile_cps<MaxNodes, MaxList>(root, ca);
    auto r = eval(code, heap, envs);
    REQUIRE_FALSE(r.has_value());
}
