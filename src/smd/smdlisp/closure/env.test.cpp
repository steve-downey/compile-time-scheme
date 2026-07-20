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
using smd::smdlisp::closure::env;
using smd::smdlisp::closure::store;
using smd::smdlisp::closure::symbol;

// -- store (step L12) -----------------------------------------------------

static_assert([] {
    store<dummy_core, smd::smdlisp::closure::default_max_store> s;
    int loc = s.alloc(val{1});
    if (std::get<int>(s.get(loc)) != 1)
        return false;
    s.set(loc, val{2});
    return std::get<int>(s.get(loc)) == 2;
}());

// -- mutable-mode env / setq (step L12) -----------------------------------

static_assert([] {
    // With no store, define_value/define_function behave exactly as
    // before (functional mode): set_value errors rather than mutating.
    env<dummy_core, 4> e;
    e.define_value(symbol{"X"}, val{1});
    auto r = e.set_value(symbol{"X"}, val{2});
    return !r.has_value();
}());

static_assert([] {
    // Store-backed: define_value stores indirectly, set_value mutates in
    // place, and the mutation is visible through a *copy* of the
    // environment (mirroring how a closure's captured environment must
    // see a later setq -- the reason set!/setq needs a shared store at
    // all).
    store<dummy_core, smd::smdlisp::closure::default_max_store> s;
    env<dummy_core, 4> e{&s};
    e.define_value(symbol{"X"}, val{1});
    env<dummy_core, 4> copy = e; // shallow copy: shares the same store*
    auto set_r = e.set_value(symbol{"X"}, val{2});
    if (!set_r.has_value() || std::get<int>(set_r.value()) != 2)
        return false; // setq returns the assigned value (this step's
                      // decision -- see env::set_value's docs).
    auto v = copy.lookup_value(symbol{"X"});
    return v.has_value() && std::get<int>(v.value()) == 2;
}());

static_assert([] {
    // setq on an unbound variable is an error.
    store<dummy_core, smd::smdlisp::closure::default_max_store> s;
    env<dummy_core, 4> e{&s};
    auto r = e.set_value(symbol{"NOPE"}, val{1});
    return !r.has_value();
}());

// -- special-variable marking (step L12; behavior arrives in L16) --------

static_assert([] {
    env<dummy_core, 4> e;
    if (e.is_special(symbol{"X"}))
        return false;
    e.declare_special(symbol{"X"});
    return e.is_special(symbol{"X"}) && !e.is_special(symbol{"Y"});
}());

static_assert([] {
    // define_special_value both binds and marks special in one call
    // (defvar/defparameter's primitive).
    env<dummy_core, 4> e;
    e.define_special_value(symbol{"X"}, val{7});
    auto v = e.lookup_value(symbol{"X"});
    return v.has_value() && std::get<int>(v.value()) == 7 &&
           e.is_special(symbol{"X"});
}());

// -- defun self-recursion support: define_function/patch_function -------

static_assert([] {
    // Store-backed: define_function returns a store location; patching it
    // with the real value is visible through a captured copy taken
    // between the two calls (the shape defun's evaluator case relies on
    // for self-recursive functions -- see env::patch_function's docs).
    store<dummy_core, smd::smdlisp::closure::default_max_store> s;
    env<dummy_core, 4> e{&s};
    int loc = e.define_function(symbol{"F"}, val{0});
    env<dummy_core, 4> captured = e; // shares the store*
    e.patch_function(symbol{"F"}, loc, val{42});
    auto v = captured.lookup_function(symbol{"F"});
    return v.has_value() && std::get<int>(v.value()) == 42;
}());

static_assert([] {
    // Functional mode (no store): patch_function falls back to an
    // ordinary shadowing define_function, so the CURRENT environment
    // still ends up with the correct value (external callers see it),
    // even though a captured copy taken before the patch will not.
    env<dummy_core, 4> e;
    int loc = e.define_function(symbol{"F"}, val{0});
    e.patch_function(symbol{"F"}, loc, val{42});
    auto v = e.lookup_function(symbol{"F"});
    return v.has_value() && std::get<int>(v.value()) == 42;
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

// -- step L12: mutable-mode env, setq, defun self-recursion support ------

TEST_CASE("EnvTest - StoreBackedDefaultEnvInstallsBuiltinsAndSupportsSetq") {
    store<dummy_core, smd::smdlisp::closure::default_max_store> s;
    auto e = default_env<dummy_core, 16>(s);
    auto plus = e.lookup_function(symbol{"+"});
    REQUIRE(plus.has_value());

    e.define_value(symbol{"X"}, val{1});
    auto set_r = e.set_value(symbol{"X"}, val{9});
    REQUIRE(set_r.has_value());
    REQUIRE(std::get<int>(set_r.value()) == 9);
    auto v = e.lookup_value(symbol{"X"});
    REQUIRE(v.has_value());
    REQUIRE(std::get<int>(v.value()) == 9);
}

TEST_CASE("EnvTest - SetqOnUnboundVariableIsError") {
    store<dummy_core, smd::smdlisp::closure::default_max_store> s;
    env<dummy_core, 4> e{&s};
    auto r = e.set_value(symbol{"NOPE"}, val{1});
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("EnvTest - SetqWithNoStoreIsError") {
    env<dummy_core, 4> e; // functional mode, no store
    e.define_value(symbol{"X"}, val{1});
    auto r = e.set_value(symbol{"X"}, val{2});
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("EnvTest - DeclareSpecialAndIsSpecial") {
    env<dummy_core, 4> e;
    REQUIRE_FALSE(e.is_special(symbol{"X"}));
    e.declare_special(symbol{"X"});
    REQUIRE(e.is_special(symbol{"X"}));
    REQUIRE_FALSE(e.is_special(symbol{"Y"}));
}

TEST_CASE("EnvTest - DefineSpecialValueBindsAndMarks") {
    env<dummy_core, 4> e;
    e.define_special_value(symbol{"X"}, val{7});
    auto v = e.lookup_value(symbol{"X"});
    REQUIRE(v.has_value());
    REQUIRE(std::get<int>(v.value()) == 7);
    REQUIRE(e.is_special(symbol{"X"}));
}

TEST_CASE("EnvTest - PatchFunctionVisibleThroughCapturedCopy") {
    store<dummy_core, smd::smdlisp::closure::default_max_store> s;
    env<dummy_core, 4> e{&s};
    int loc = e.define_function(symbol{"F"}, val{0});
    env<dummy_core, 4> captured = e;
    e.patch_function(symbol{"F"}, loc, val{42});

    auto v = captured.lookup_function(symbol{"F"});
    REQUIRE(v.has_value());
    REQUIRE(std::get<int>(v.value()) == 42);

    // The original environment also observes the patch.
    auto v2 = e.lookup_function(symbol{"F"});
    REQUIRE(v2.has_value());
    REQUIRE(std::get<int>(v2.value()) == 42);
}

TEST_CASE("EnvTest - PatchFunctionFunctionalModeFallsBackToShadow") {
    env<dummy_core, 4> e; // no store
    int loc = e.define_function(symbol{"F"}, val{0});
    e.patch_function(symbol{"F"}, loc, val{42});
    auto v = e.lookup_function(symbol{"F"});
    REQUIRE(v.has_value());
    REQUIRE(std::get<int>(v.value()) == 42);
}
