// src/smd/smdlisp/sender/sender_eval.test.cpp                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/sender/sender_eval.hpp>
#include <smd/smdlisp/sender/sender_eval.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <span>
#include <string_view>
#include <variant>

namespace {
namespace lisp = smd::smdlisp;
namespace scm = smd::smdscheme;
namespace snd = smd::smdlisp::sender;

constexpr int MaxNodes = 128;
constexpr int MaxList = 16;
constexpr int MaxBindings = 16;
constexpr int MaxEnvs = 32;

using Core = lisp::elaborator::core_type<MaxNodes, MaxList>;
using CoreLayer =
    lisp::elaborator::core_f_factory<MaxNodes, MaxList>::template type<Core>;
using Val = lisp::closure::value<Core>;
using CoreArena = scm::foundation::tree_arena<Core, MaxNodes>;
using Heap = lisp::closure::pair_heap<Core, lisp::closure::default_max_pairs>;
using Envs = lisp::closure::env_arena<Core, MaxBindings, MaxEnvs>;
using Ctx = snd::eval_context<MaxNodes, MaxList, MaxBindings, MaxEnvs>;

/// Wraps one core layer as a core node, with no reader or elaborator in the
/// way.  Everything below builds its own AST for exactly that reason: this
/// file is about the evaluator's own interface, and `sender_program.test.cpp`
/// already owns the source-text pipeline.
constexpr auto node_of(CoreLayer layer) -> Core {
    return Core{std::move(layer)};
}

/// Builds a @ref lisp::reader::folded_name holding @p s.
///
/// Not `folded_name{s}`: that is an aggregate whose first member is the
/// character storage and whose second is the *length*, so brace elision
/// initialises the storage from the literal and leaves `length` at 0 -- a
/// silently empty name that compiles cleanly and then fails to match
/// anything.  @p s must already be case-folded (decision D2); the reader
/// would have done that.
constexpr auto folded(std::string_view s) -> lisp::reader::folded_name {
    lisp::reader::folded_name n{};
    for (char const c : s)
        n.storage[static_cast<std::size_t>(n.length++)] = c;
    return n;
}

static_assert(folded("X").view() == "X");
} // namespace

TEST_CASE("SenderEvalTest - HeaderIsIdempotent") { REQUIRE(true); }

// -- the Mendler bridge, independently of evaluation -------------------------
// `detail::mendler_para` is a recursion scheme, not part of the evaluator, and
// the point of Mendler style is that the algebra is handed a `recurse` rather
// than naming the traversal itself.  Driving it with an algebra that has
// nothing to do with evaluation is what shows that is actually true here.
// See DIV-0016 for why this is a local re-derivation rather than a call to
// `smd::fixpoint::mendler_para`.

TEST_CASE("SenderEvalTest - MendlerParaDrivesAnArbitraryAlgebra") {
    CoreArena arena;
    auto const left = scm::foundation::make_arena_box<Core, MaxNodes>(
        arena, node_of(CoreLayer{lisp::elaborator::core_integer{2}}));
    auto const right = scm::foundation::make_arena_box<Core, MaxNodes>(
        arena, node_of(CoreLayer{lisp::elaborator::core_integer{40}}));

    lisp::elaborator::core_progn<Core, MaxNodes, MaxList> progn{};
    progn.exprs.push_back(left);
    progn.exprs.push_back(right);
    Core const root = node_of(CoreLayer{progn});

    // Sums every core_integer in the tree. Nothing to do with senders.
    auto const sum_integers = [](auto const &recurse, CoreArena const *ctx,
                                 Core const &, auto const &layer) -> int {
        if (std::holds_alternative<lisp::elaborator::core_integer>(layer))
            return std::get<lisp::elaborator::core_integer>(layer).value;
        if (std::holds_alternative<
                lisp::elaborator::core_progn<Core, MaxNodes, MaxList>>(layer)) {
            auto const &p =
                std::get<lisp::elaborator::core_progn<Core, MaxNodes, MaxList>>(
                    layer);
            int total = 0;
            for (int i = 0; i < p.exprs.size(); ++i)
                total += recurse(ctx->get(p.exprs[i]), ctx);
            return total;
        }
        return 0;
    };

    CoreArena const *ctx = &arena;
    REQUIRE(snd::detail::mendler_para<int, Core>(sum_integers, ctx, root) ==
            42);
}

TEST_CASE("SenderEvalTest - MendlerParaPassesTheWholeNodeNotJustTheLayer") {
    // Paramorphic rather than catamorphic, and `core_lambda` depends on it:
    // the closure it builds stores a pointer to the node itself.
    Core const root = node_of(CoreLayer{lisp::elaborator::core_integer{7}});
    auto const identity_of_node = [](auto const &, int, Core const &whole,
                                     auto const &) -> Core const * {
        return &whole;
    };
    REQUIRE(snd::detail::mendler_para<Core const *, Core>(identity_of_node, 0,
                                                          root) == &root);
}

// -- eval_node on hand-built nodes -------------------------------------------
// Leaf forms need no arena, so they exercise the algebra's own dispatch
// without any pipeline behind it.

TEST_CASE("SenderEvalTest - LeafFormsCompleteOnTheValueChannel") {
    CoreArena arena;
    Heap heap;
    Envs envs;
    auto env = lisp::closure::default_env<Core, MaxBindings>(heap);
    Ctx const ctx{&arena, &env, &envs};

    Core const n_int = node_of(CoreLayer{lisp::elaborator::core_integer{42}});
    auto const o_int =
        snd::eval_node<MaxNodes, MaxList, MaxBindings, MaxEnvs>(n_int, ctx);
    REQUIRE(o_int.is_value());
    REQUIRE(std::get<int>(o_int.val) == 42);

    Core const n_nil = node_of(CoreLayer{lisp::elaborator::core_nil{}});
    auto const o_nil =
        snd::eval_node<MaxNodes, MaxList, MaxBindings, MaxEnvs>(n_nil, ctx);
    REQUIRE(o_nil.is_value());
    REQUIRE(std::holds_alternative<lisp::closure::nil_t>(o_nil.val));

    // D3: `t` is the canonical true value, the symbol T.
    Core const n_true = node_of(CoreLayer{lisp::elaborator::core_true{}});
    auto const o_true =
        snd::eval_node<MaxNodes, MaxList, MaxBindings, MaxEnvs>(n_true, ctx);
    REQUIRE(o_true.is_value());
    REQUIRE(std::get<lisp::closure::symbol>(o_true.val).name == "T");
}

TEST_CASE("SenderEvalTest - UnboundVariableCompletesOnTheErrorChannel") {
    // D4: a `core_symbol` resolves through the VARIABLE namespace only, so a
    // name bound as a function is still unbound here. `CONS` is a default
    // FUNCTION binding.
    CoreArena arena;
    Heap heap;
    Envs envs;
    auto env = lisp::closure::default_env<Core, MaxBindings>(heap);
    Ctx const ctx{&arena, &env, &envs};

    Core const n =
        node_of(CoreLayer{lisp::elaborator::core_symbol{folded("CONS")}});
    auto const o =
        snd::eval_node<MaxNodes, MaxList, MaxBindings, MaxEnvs>(n, ctx);
    REQUIRE(o.is_error());
}

// -- apply_function_value at its own level -----------------------------------

TEST_CASE("SenderEvalTest - ApplyFunctionValueOnABuiltin") {
    CoreArena arena;
    Heap heap;
    Envs envs;
    auto env = lisp::closure::default_env<Core, MaxBindings>(heap);
    Ctx const ctx{&arena, &env, &envs};

    Val const args[3] = {Val{1}, Val{2}, Val{3}};
    auto const o =
        snd::apply_function_value<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            Val{lisp::closure::builtin{lisp::closure::builtin_op::add}},
            std::span<Val const>{args}, ctx);
    REQUIRE(o.is_value());
    REQUIRE(std::get<int>(o.val) == 6);
}

TEST_CASE("SenderEvalTest - ApplyFunctionValueOnANonFunction") {
    CoreArena arena;
    Heap heap;
    Envs envs;
    auto env = lisp::closure::default_env<Core, MaxBindings>(heap);
    Ctx const ctx{&arena, &env, &envs};

    auto const o =
        snd::apply_function_value<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            Val{5}, std::span<Val const>{}, ctx);
    REQUIRE(o.is_error());
    REQUIRE(std::string_view{o.err.message} ==
            "attempted to call non-function");
}

TEST_CASE("SenderEvalTest - ContextCanNameADifferentEnvironment") {
    // `apply_function_value` builds a fresh context naming the callee's
    // environment; the context is held as pointers precisely so it can.
    CoreArena arena;
    Heap heap;
    Envs envs;
    auto outer = lisp::closure::default_env<Core, MaxBindings>(heap);
    auto inner = lisp::closure::default_env<Core, MaxBindings>(heap);
    inner.define_value(lisp::closure::symbol{"X"}, Val{9});

    Core const n =
        node_of(CoreLayer{lisp::elaborator::core_symbol{folded("X")}});

    Ctx const outer_ctx{&arena, &outer, &envs};
    REQUIRE(
        snd::eval_node<MaxNodes, MaxList, MaxBindings, MaxEnvs>(n, outer_ctx)
            .is_error());

    Ctx const inner_ctx{&arena, &inner, &envs};
    auto const o =
        snd::eval_node<MaxNodes, MaxList, MaxBindings, MaxEnvs>(n, inner_ctx);
    REQUIRE(o.is_value());
    REQUIRE(std::get<int>(o.val) == 9);
}
