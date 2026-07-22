// src/smd/smdlisp/closure/env.test.cpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/closure/env.hpp>
#include <smd/smdlisp/closure/env.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <variant>

namespace {

/// Stand-in core AST type; env only needs some type to instantiate
/// @c value<Core>/@c env<Core, MaxBindings> with.  The real core arena
/// arrives in step L10.
struct dummy_core {};

using val = smd::smdlisp::closure::value<dummy_core>;
using smd::smdlisp::closure::builtin;
using smd::smdlisp::closure::builtin_op;
using smd::smdlisp::closure::default_env;
using smd::smdlisp::closure::default_max_store;
using smd::smdlisp::closure::env;
using smd::smdlisp::closure::store;
using smd::smdlisp::closure::symbol;

// Merge criteria (docs/cl-pivot-plan.md, step L12): `setq` mutates the
// nearest lexical variable binding through a shared @ref store, returns
// the assigned value (not Scheme's unspecified), and is a diagnosed error
// both when the environment has no store (functional mode) and when the
// target is unbound.

static_assert([] {
    // Functional mode (no store): setq is a diagnosed error, never a
    // silent no-op.
    env<dummy_core, 4> e;
    e.define_value(symbol{"X"}, val{1});
    auto r = e.set_value(symbol{"X"}, val{2});
    return !r.has_value();
}());

static_assert([] {
    store<dummy_core, default_max_store> st;
    env<dummy_core, 4> e{&st};
    e.define_value(symbol{"X"}, val{1});
    auto r = e.set_value(symbol{"X"}, val{2});
    // setq returns the ASSIGNED value per ANSI CL.
    if (!r.has_value() || std::get<int>(r.value()) != 2)
        return false;
    auto v = e.lookup_value(symbol{"X"});
    return v.has_value() && std::get<int>(v.value()) == 2;
}());

static_assert([] {
    // setq of an unbound target is a diagnosed error, even with a store.
    store<dummy_core, default_max_store> st;
    env<dummy_core, 4> e{&st};
    auto r = e.set_value(symbol{"NOPE"}, val{1});
    return !r.has_value();
}());

static_assert([] {
    // setq never touches the FUNCTION namespace: a name bound only as a
    // function is still "unbound" from setq's point of view.
    store<dummy_core, default_max_store> st;
    env<dummy_core, 4> e{&st};
    e.define_function(symbol{"F"}, val{builtin{builtin_op::add}});
    auto r = e.set_value(symbol{"F"}, val{1});
    return !r.has_value();
}());

static_assert([] {
    // A store is shared across env copies, so setq on a variable is
    // visible through every copy that shares the binding -- the same
    // reference-semantics guarantee `smd::smdscheme::closure::store`
    // gives `set!`, adapted for `setq`.
    store<dummy_core, default_max_store> st;
    env<dummy_core, 4> e{&st};
    e.define_value(symbol{"X"}, val{1});
    env<dummy_core, 4> copy = e;
    auto r = copy.set_value(symbol{"X"}, val{2});
    if (!r.has_value())
        return false;
    auto v = e.lookup_value(symbol{"X"});
    return v.has_value() && std::get<int>(v.value()) == 2;
}());

// Merge criteria (docs/cl-pivot-plan.md, step L12): `defvar`/`defparameter`
// mark a symbol special; the mark is recorded now, used in step L16.

static_assert([] {
    env<dummy_core, 4> e;
    e.mark_special(symbol{"*X*"});
    return e.is_special(symbol{"*X*"}) && !e.is_special(symbol{"*Y*"});
}());

static_assert([] {
    // Idempotent: marking twice does not error or duplicate.
    env<dummy_core, 4> e;
    e.mark_special(symbol{"*X*"});
    e.mark_special(symbol{"*X*"});
    return e.is_special(symbol{"*X*"});
}());

// Merge criteria (docs/cl-pivot-plan.md, step L9): `f` as a variable and
// `f` as a function coexist without shadowing each other; lookups fail
// with a proper error result for unbound names in each namespace
// independently.

static_assert([] {
    env<dummy_core, 4> e;
    e.define_value(symbol{"F"}, val{1});
    e.define_function(symbol{"F"}, val{2});
    auto v = e.lookup_value(symbol{"F"});
    auto f = e.lookup_function(symbol{"F"});
    return v.has_value() && std::get<int>(v.value()) == 1 && f.has_value() &&
           std::get<int>(f.value()) == 2;
}());

static_assert([] {
    // Defining F only as a function leaves the variable namespace unbound.
    env<dummy_core, 4> e;
    e.define_function(symbol{"F"}, val{2});
    auto v = e.lookup_value(symbol{"F"});
    auto f = e.lookup_function(symbol{"F"});
    return !v.has_value() && f.has_value();
}());

static_assert([] {
    // Defining F only as a variable leaves the function namespace unbound.
    env<dummy_core, 4> e;
    e.define_value(symbol{"F"}, val{1});
    auto v = e.lookup_value(symbol{"F"});
    auto f = e.lookup_function(symbol{"F"});
    return v.has_value() && !f.has_value();
}());

static_assert([] {
    // Unbound in both namespaces on an empty environment.
    env<dummy_core, 4> e;
    auto v = e.lookup_value(symbol{"UNDEFINED"});
    auto f = e.lookup_function(symbol{"UNDEFINED"});
    return !v.has_value() && !f.has_value();
}());

static_assert([] {
    // Most-recent-first shadowing within one namespace.
    env<dummy_core, 4> e;
    e.define_value(symbol{"X"}, val{1});
    e.define_value(symbol{"X"}, val{2});
    auto v = e.lookup_value(symbol{"X"});
    return v.has_value() && std::get<int>(v.value()) == 2;
}());

static_assert([] {
    // default_env installs the builtins in the function namespace only.
    auto e = default_env<dummy_core, 16>();
    auto plus = e.lookup_function(symbol{"+"});
    auto cons = e.lookup_function(symbol{"CONS"});
    auto funcall = e.lookup_function(symbol{"FUNCALL"});
    auto apply = e.lookup_function(symbol{"APPLY"});
    auto not_a_var = e.lookup_value(symbol{"+"});
    return plus.has_value() &&
           std::get<builtin>(plus.value()).op == builtin_op::add &&
           cons.has_value() &&
           std::get<builtin>(cons.value()).op == builtin_op::cons &&
           funcall.has_value() &&
           std::get<builtin>(funcall.value()).op == builtin_op::funcall &&
           apply.has_value() &&
           std::get<builtin>(apply.value()).op == builtin_op::apply &&
           !not_a_var.has_value();
}());

} // namespace

TEST_CASE("EnvTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("EnvTest - VariableAndFunctionCoexist") {
    env<dummy_core, 4> e;
    e.define_value(symbol{"F"}, val{1});
    e.define_function(symbol{"F"}, val{2});
    auto v = e.lookup_value(symbol{"F"});
    auto f = e.lookup_function(symbol{"F"});
    REQUIRE(v.has_value());
    REQUIRE(std::get<int>(v.value()) == 1);
    REQUIRE(f.has_value());
    REQUIRE(std::get<int>(f.value()) == 2);
}

TEST_CASE("EnvTest - UnboundVariableIsError") {
    env<dummy_core, 4> e;
    auto v = e.lookup_value(symbol{"UNDEFINED"});
    REQUIRE_FALSE(v.has_value());
}

TEST_CASE("EnvTest - UndefinedFunctionIsError") {
    env<dummy_core, 4> e;
    auto f = e.lookup_function(symbol{"UNDEFINED"});
    REQUIRE_FALSE(f.has_value());
}

TEST_CASE("EnvTest - NamespacesAreIndependent") {
    env<dummy_core, 4> e;
    e.define_value(symbol{"X"}, val{10});
    REQUIRE(e.lookup_value(symbol{"X"}).has_value());
    REQUIRE_FALSE(e.lookup_function(symbol{"X"}).has_value());

    e.define_function(symbol{"Y"}, val{20});
    REQUIRE(e.lookup_function(symbol{"Y"}).has_value());
    REQUIRE_FALSE(e.lookup_value(symbol{"Y"}).has_value());
}

TEST_CASE("EnvTest - MostRecentDefinitionShadows") {
    env<dummy_core, 4> e;
    e.define_function(symbol{"F"}, val{1});
    e.define_function(symbol{"F"}, val{2});
    auto f = e.lookup_function(symbol{"F"});
    REQUIRE(f.has_value());
    REQUIRE(std::get<int>(f.value()) == 2);
}

TEST_CASE("EnvTest - DefaultEnvInstallsBuiltinsInFunctionNamespace") {
    auto e = default_env<dummy_core, 16>();

    struct expectation {
        char const *name;
        builtin_op op;
    };
    std::array<expectation, 12> expected{{
        {"+", builtin_op::add},
        {"*", builtin_op::multiply},
        {"CONS", builtin_op::cons},
        {"CAR", builtin_op::car},
        {"CDR", builtin_op::cdr},
        {"LIST", builtin_op::list},
        {"NULL", builtin_op::null},
        {"EQ", builtin_op::eq},
        {"EQL", builtin_op::eql},
        {"ATOM", builtin_op::atom},
        {"FUNCALL", builtin_op::funcall},
        {"APPLY", builtin_op::apply},
    }};
    for (auto const &exp : expected) {
        auto f = e.lookup_function(symbol{exp.name});
        REQUIRE(f.has_value());
        REQUIRE(std::holds_alternative<builtin>(f.value()));
        REQUIRE(std::get<builtin>(f.value()).op == exp.op);
    }

    // None of the builtins leak into the variable namespace.
    REQUIRE_FALSE(e.lookup_value(symbol{"CONS"}).has_value());
}

TEST_CASE("EnvTest - SetValueMutatesStoreBackedBinding") {
    store<dummy_core, default_max_store> st;
    env<dummy_core, 4> e{&st};
    e.define_value(symbol{"X"}, val{1});
    auto r = e.set_value(symbol{"X"}, val{99});
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 99);
    REQUIRE(std::get<int>(e.lookup_value(symbol{"X"}).value()) == 99);
}

TEST_CASE("EnvTest - SetValueWithoutStoreIsError") {
    env<dummy_core, 4> e;
    e.define_value(symbol{"X"}, val{1});
    REQUIRE_FALSE(e.set_value(symbol{"X"}, val{2}).has_value());
}

TEST_CASE("EnvTest - SetValueOfUnboundIsError") {
    store<dummy_core, default_max_store> st;
    env<dummy_core, 4> e{&st};
    REQUIRE_FALSE(e.set_value(symbol{"NOPE"}, val{1}).has_value());
}

TEST_CASE("EnvTest - MarkSpecialAndIsSpecial") {
    env<dummy_core, 4> e;
    REQUIRE_FALSE(e.is_special(symbol{"*X*"}));
    e.mark_special(symbol{"*X*"});
    REQUIRE(e.is_special(symbol{"*X*"}));
}

TEST_CASE("EnvTest - CopyingEnvCopiesBothNamespaces") {
    env<dummy_core, 4> e;
    e.define_value(symbol{"X"}, val{1});
    e.define_function(symbol{"F"}, val{2});

    env<dummy_core, 4> copy = e; // exercises the copy-and-extend scoping model
    copy.define_value(symbol{"Y"}, val{3});

    REQUIRE(copy.lookup_value(symbol{"X"}).has_value());
    REQUIRE(copy.lookup_value(symbol{"Y"}).has_value());
    REQUIRE(copy.lookup_function(symbol{"F"}).has_value());
    // The original is unaffected by the copy's extension.
    REQUIRE_FALSE(e.lookup_value(symbol{"Y"}).has_value());
}
