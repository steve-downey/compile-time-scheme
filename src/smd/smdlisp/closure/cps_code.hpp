// src/smd/smdlisp/closure/cps_code.hpp                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_CLOSURE_CPS_CODE_HPP
#define SRC_SMD_SMDLISP_CLOSURE_CPS_CODE_HPP

#include <smd/smdlisp/closure/env.hpp>
#include <smd/smdlisp/closure/eval_direct.hpp>
#include <smd/smdlisp/closure/pairs.hpp>
#include <smd/smdlisp/closure/value.hpp>
#include <smd/smdlisp/elaborator/elaborate.hpp>
#include <smd/smdscheme/foundation/arena_box.hpp>
#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/foundation/static_vector.hpp>

#include <smd/fixpoint/overloaded.hpp>

#include <cstddef>
#include <span>
#include <string_view>
#include <variant>

namespace smd::smdlisp::closure {

/// A callable CPS representation of a compiled `smdlisp` expression.
///
/// @c cps_code<F> wraps a callable @p F with signature
/// @code
///   (Env &environment, Envs &envs, K k) -> result<value<Core>>
/// @endcode
/// where @p K is the outermost continuation.  Callers pass a (mutable --
/// see below) environment, the shared closure-capture arena, and a
/// continuation; the code evaluates the expression and invokes the
/// continuation with the resulting value.
///
/// **Why @p environment is mutable, unlike the Scheme original's
/// `cps_code` (which takes `Env const&`).** `setq`/`defun`/`defvar`/
/// `defparameter` (step L12) all mutate the environment object itself --
/// not merely a value cell reached through it -- exactly as
/// `eval_direct.hpp`'s `environment` parameter had to become `env<...>&`
/// for the same reason (see that file's doc comment). CPS conversion of a
/// mutable-store-backed interpreter is not a mechanical port of the
/// Scheme `cps_code`: threading @p environment by mutable reference,
/// rather than by `const&`/by-value copy, is what lets a `defun` inside a
/// `progn` (evaluated via @ref detail::cps_dispatch's `core_progn` case)
/// be visible to a sibling expression evaluated later in the very same
/// `progn` call -- the identical mechanism `eval_direct` uses, adapted to
/// CPS.
///
/// @tparam F The underlying callable type (deduced via the deduction
/// guide).
// 14e385d3-52c6-48d4-8ed2-6c6882ce1f33
template <class F>
struct cps_code {
    F f;

    /// Evaluates the CPS expression under @p environment (mutable) and
    /// @p envs (the shared closure-capture arena), invoking continuation
    /// @p k.
    template <class Env, class Envs, class K>
    constexpr auto operator()(Env &environment, Envs &envs, K k) const {
        return f(environment, envs, k);
    }
};
// 14e385d3-52c6-48d4-8ed2-6c6882ce1f33 end

/// Deduction guide: @c cps_code(f) deduces @c cps_code<F>.
template <class F>
cps_code(F) -> cps_code<F>;

namespace detail {

/// The identity continuation: returns its argument unchanged.
///
/// Used as the terminal continuation in @ref cps_of and @ref compile_cps
/// to extract the final value, and as the "evaluate this subexpression
/// synchronously, right now" pair of continuations everywhere @ref
/// cps_dispatch needs a definite value before it can proceed (a
/// condition, an argument, a value form) -- exactly the role it plays in
/// the Scheme original.
///
/// @tparam Core Core AST type.
template <typename Core>
struct identity_k {
    constexpr auto operator()(value<Core> v) const
        -> smd::smdscheme::foundation::result<value<Core>> {
        return v;
    }
};

/// Forward declaration: @ref cps_dispatch and @ref cps_apply recurse into
/// each other (an application dispatches its evaluated function value via
/// @ref cps_apply; a called closure's body is evaluated via @ref
/// cps_dispatch, continuation-chained through the closure's own implicit
/// progn).
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs, class Cont,
          class K>
[[nodiscard]] constexpr auto cps_dispatch(
    elaborator::core_type<MaxNodes, MaxList> const &node,
    smd::smdscheme::foundation::tree_arena<
        elaborator::core_type<MaxNodes, MaxList>, MaxNodes> const &arena,
    Cont const &cont,
    env<elaborator::core_type<MaxNodes, MaxList>, MaxBindings> &environment,
    env_arena<elaborator::core_type<MaxNodes, MaxList>, MaxBindings, MaxEnvs>
        &envs,
    K const &k)
    -> smd::smdscheme::foundation::result<
        value<elaborator::core_type<MaxNodes, MaxList>>>;

/// Applies an already-evaluated function @ref value to already-evaluated
/// argument values, continuing via (@p cont, @p k) -- the CPS counterpart
/// of `eval_direct.hpp`'s `apply_function_value`, and the single
/// call-dispatch point shared by ordinary application (@ref cps_dispatch's
/// `core_application` case) and the `funcall`/`apply` builtins.
///
/// **This is where `defun` self-recursion actually gets exercised through
/// CPS**, not invented here: a self-recursive `defun`'s closure is built
/// exactly as @ref cps_dispatch's `core_defun` case builds it (reserve a
/// placeholder FUNCTION-namespace binding, capture the environment, patch
/// the placeholder -- see `env.hpp`'s `env::patch_function` docs for the
/// mechanism), and calling that closure -- here, in the @c closure
/// branch below -- looks the name up through the very same shared @ref
/// store the direct evaluator uses, so a recursive call inside the
/// function's own body finds the patched-in closure value regardless of
/// which backend evaluates it.
///
/// A called closure's body -- an implicit-progn sequence, per step L10's
/// core-node shape -- is walked exactly like @ref cps_dispatch's
/// `core_progn` case: every expression but the last is evaluated for
/// effect only (@ref identity_k / @ref identity_k), and the last is
/// evaluated in TAIL position, continuation-chained through the caller's
/// own (@p cont, @p k). This is the "progn compiles to continuation
/// chaining" structure the plan calls out, applied to a closure's body as
/// well as to an ordinary `progn` -- both are implicit-progn sequences at
/// the core-node level, so both compile the same way.
///
/// `funcall`/`apply` recurse into @ref cps_apply itself with the SAME
/// (@p cont, @p k) the caller supplied, so calling a closure through
/// `funcall`/`apply` is exactly as much a tail call, in this CPS
/// structure, as an ordinary application.
///
/// @tparam MaxNodes    Arena capacity.
/// @tparam MaxList     Maximum argument/list/body length.
/// @tparam MaxBindings Environment capacity; must be 16 (see
///                      `eval_direct.hpp`'s doc comment for why:
///                      `value<Core>`'s embedded @ref closure alternative
///                      is unconditionally `closure<Core, 16>`).
/// @tparam MaxEnvs     Capacity of the @ref env_arena capturing closures.
/// @param  func_val The callee: a @ref builtin, @ref closure, or
///                    @ref foreign_function value.  Any other value kind
///                    is a diagnosed "attempted to call non-function"
///                    error.
/// @param  args      The already-evaluated call arguments.
/// @param  arena     Core arena; must outlive the evaluation.
/// @param  cont      The intermediate continuation applied to the call's
///                    result.
/// @param  environment Current variable/function bindings and pair heap;
///                      mutable, threaded through exactly like
///                      `eval_direct`'s.
/// @param  envs      The shared environment arena that owns every
///                    environment a closure materialized during this call
///                    captures.
/// @param  k         The outer continuation applied to @p cont's result.
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs, class Cont,
          class K>
[[nodiscard]] constexpr auto cps_apply(
    value<elaborator::core_type<MaxNodes, MaxList>> const &func_val,
    std::span<value<elaborator::core_type<MaxNodes, MaxList>> const> args,
    smd::smdscheme::foundation::tree_arena<
        elaborator::core_type<MaxNodes, MaxList>, MaxNodes> const &arena,
    Cont const &cont,
    env<elaborator::core_type<MaxNodes, MaxList>, MaxBindings> &environment,
    env_arena<elaborator::core_type<MaxNodes, MaxList>, MaxBindings, MaxEnvs>
        &envs,
    K const &k)
    -> smd::smdscheme::foundation::result<
        value<elaborator::core_type<MaxNodes, MaxList>>> {
    static_assert(MaxBindings == 16,
                  "cps_apply currently requires closure::env<Core,16> "
                  "(value<Core>'s embedded closure alternative is "
                  "closure<Core,16>, unconditionally)");
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    using Val = value<Core>;
    using Res = smd::smdscheme::foundation::result<Val>;
    using smd::smdscheme::foundation::parse_error;

    return std::visit(
        smd::fixpoint::overloaded{
            [&](builtin const &bi) -> Res {
                switch (bi.op) {
                case builtin_op::add:
                case builtin_op::multiply: {
                    // ANSI CL `+`/`*` are variadic, `(+)` => 0, `(*)` => 1.
                    int acc = bi.op == builtin_op::add ? 0 : 1;
                    for (auto const &a : args) {
                        if (!std::holds_alternative<int>(a))
                            return parse_error{{}, "type error"};
                        acc = bi.op == builtin_op::add ? acc + std::get<int>(a)
                                                       : acc * std::get<int>(a);
                    }
                    Val v{acc};
                    auto r = cont(v);
                    if (!r.has_value())
                        return r;
                    return k(r.value());
                }
                case builtin_op::cons:
                case builtin_op::car:
                case builtin_op::cdr:
                case builtin_op::list:
                case builtin_op::null:
                case builtin_op::eq:
                case builtin_op::eql:
                case builtin_op::atom: {
                    auto pr = apply_prim<Core, default_max_pairs>(
                        to_list_op(bi.op), args, environment.pairs());
                    if (!pr.has_value())
                        return pr;
                    auto r = cont(pr.value());
                    if (!r.has_value())
                        return r;
                    return k(r.value());
                }
                case builtin_op::funcall: {
                    // (funcall f args...): the first argument IS the
                    // function to call; the rest are its call arguments.
                    if (args.empty())
                        return parse_error{
                            {}, "funcall: expected a function argument"};
                    return cps_apply<MaxNodes, MaxList>(
                        args[0], args.subspan(1), arena, cont, environment,
                        envs, k);
                }
                case builtin_op::apply: {
                    // (apply f args... list): every argument but the
                    // first (the function) and the last (a list) is
                    // passed through as-is; the last argument is spread.
                    if (args.size() < 2)
                        return parse_error{
                            {},
                            "apply: expected a function and at least one "
                            "list argument"};
                    smd::smdscheme::foundation::static_vector<Val, MaxList>
                        spread;
                    for (std::size_t i = 1; i + 1 < args.size(); ++i)
                        spread.push_back(args[i]);
                    Val cur = args[args.size() - 1];
                    while (!std::holds_alternative<nil_t>(cur)) {
                        if (!std::holds_alternative<pair_ref>(cur))
                            return parse_error{
                                {}, "apply: last argument must be a list"};
                        auto *heap = environment.pairs();
                        if (heap == nullptr)
                            return parse_error{
                                {}, "apply: environment has no pair heap"};
                        auto const &cell =
                            heap->get(std::get<pair_ref>(cur).loc);
                        spread.push_back(cell.car);
                        cur = cell.cdr;
                    }
                    return cps_apply<MaxNodes, MaxList>(
                        args[0],
                        std::span<Val const>(spread.begin(), spread.end()),
                        arena, cont, environment, envs, k);
                }
                }
                return parse_error{{}, "unknown builtin"};
            },
            // 39b1b1fa-eecc-4210-9dc7-4512aba2f6c3
            [&](closure<Core> const &clo) -> Res {
                if (clo.node == nullptr)
                    return parse_error{
                        {}, "internal error: closure has no lambda node"};
                auto const &lam_node = *clo.node;
                if (!std::holds_alternative<
                        elaborator::core_lambda<Core, MaxNodes, MaxList>>(
                        lam_node.inner))
                    return parse_error{
                        {},
                        "internal error: closure does not reference a "
                        "lambda"};
                auto const &lam =
                    std::get<elaborator::core_lambda<Core, MaxNodes, MaxList>>(
                        lam_node.inner);
                if (static_cast<int>(args.size()) != lam.params.size())
                    return parse_error{{}, "arity mismatch"};
                if (clo.captured == nullptr)
                    return parse_error{
                        {},
                        "internal error: closure has no captured "
                        "environment"};

                env<Core, MaxBindings> new_env = *clo.captured;
                for (int i = 0; i < lam.params.size(); ++i)
                    new_env.define_value(symbol{lam.params[i]}, args[i]);

                // Implicit progn, continuation-chained: every body
                // expression but the last is evaluated for effect only;
                // the last tail-chains into the caller's own (cont, k).
                int const n = lam.body.size();
                for (int i = 0; i < n - 1; ++i) {
                    auto r = cps_dispatch<MaxNodes, MaxList>(
                        arena.get(lam.body[i]), arena, identity_k<Core>{},
                        new_env, envs, identity_k<Core>{});
                    if (!r.has_value())
                        return r;
                }
                return cps_dispatch<MaxNodes, MaxList>(
                    arena.get(lam.body[n - 1]), arena, cont, new_env, envs, k);
            },
            // 39b1b1fa-eecc-4210-9dc7-4512aba2f6c3 end
            [&](foreign_function<Core> const &ff) -> Res {
                auto ff_r = ff.fn(args);
                if (!ff_r.has_value())
                    return Res{ff_r.error()};
                auto r = cont(ff_r.value());
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            [](nil_t const &) -> Res {
                return parse_error{{}, "attempted to call non-function"};
            },
            [](int const &) -> Res {
                return parse_error{{}, "attempted to call non-function"};
            },
            [](symbol const &) -> Res {
                return parse_error{{}, "attempted to call non-function"};
            },
            [](keyword const &) -> Res {
                return parse_error{{}, "attempted to call non-function"};
            },
            [](pair_ref const &) -> Res {
                return parse_error{{}, "attempted to call non-function"};
            }},
        func_val);
}

/// Internal CPS dispatch over a core AST node.
///
/// Evaluates @p node with outer continuation @p cont and inner
/// continuation @p k, threading a mutable @p environment and the shared
/// @p envs closure-capture arena, exactly as `eval_direct.hpp`'s
/// evaluator does (see @ref cps_code's doc comment for why @p environment
/// must be mutable). Handles all sixteen `elaborated_core.hpp` node
/// kinds, including the four step-L12 kinds
/// (`setq`/`defun`/`defvar`/`defparameter`) not present in the frozen
/// Scheme original this file is adapted from.
///
/// @tparam MaxNodes    Arena capacity.
/// @tparam MaxList     Maximum argument/list/body length.
/// @tparam MaxBindings Environment capacity; must be 16.
/// @tparam MaxEnvs     Capacity of the @ref env_arena capturing closures.
/// @param  node  Node to evaluate.
/// @param  arena Core arena.
/// @param  cont  Intermediate continuation applied to the node's value.
/// @param  environment Current variable/function bindings and pair heap.
/// @param  envs  Shared closure-capture arena.
/// @param  k     Outer continuation applied to @p cont's result.
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs, class Cont,
          class K>
[[nodiscard]] constexpr auto cps_dispatch(
    elaborator::core_type<MaxNodes, MaxList> const &node,
    smd::smdscheme::foundation::tree_arena<
        elaborator::core_type<MaxNodes, MaxList>, MaxNodes> const &arena,
    Cont const &cont,
    env<elaborator::core_type<MaxNodes, MaxList>, MaxBindings> &environment,
    env_arena<elaborator::core_type<MaxNodes, MaxList>, MaxBindings, MaxEnvs>
        &envs,
    K const &k)
    -> smd::smdscheme::foundation::result<
        value<elaborator::core_type<MaxNodes, MaxList>>> {
    static_assert(MaxBindings == 16,
                  "cps_dispatch currently requires closure::env<Core,16> "
                  "(see cps_apply's doc comment for why)");
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    using Val = value<Core>;
    using Res = smd::smdscheme::foundation::result<Val>;
    using smd::smdscheme::foundation::parse_error;

    return std::visit(
        smd::fixpoint::overloaded{
            [&](elaborator::core_integer const &ci) -> Res {
                Val v{ci.value};
                auto r = cont(v);
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            [&](elaborator::core_symbol const &cs) -> Res {
                // VARIABLE namespace only, per D4 -- never lookup_function.
                auto lr = environment.lookup_value(symbol{cs.name.view()});
                if (!lr.has_value())
                    return Res{lr.error()};
                auto r = cont(lr.value());
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            [&](elaborator::core_keyword const &ck) -> Res {
                Val v{keyword{ck.name.view()}};
                auto r = cont(v);
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            [&](elaborator::core_nil const &) -> Res {
                Val v{nil_t{}};
                auto r = cont(v);
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            [&](elaborator::core_true const &) -> Res {
                Val v{symbol{"T"}};
                auto r = cont(v);
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            [&](elaborator::core_quote const &cq) -> Res {
                Val v = std::visit(
                    smd::fixpoint::overloaded{
                        [](int i) -> Val { return Val{i}; },
                        [](elaborator::core_symbol const &sym) -> Val {
                            return Val{symbol{sym.name.view()}};
                        },
                        [](elaborator::core_keyword const &kw) -> Val {
                            return Val{keyword{kw.name.view()}};
                        }},
                    cq.atom);
                auto r = cont(v);
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            [&](elaborator::core_cons<Core, MaxNodes> const &cc) -> Res {
                auto car_r = cps_dispatch<MaxNodes, MaxList>(
                    arena.get(cc.car), arena, identity_k<Core>{}, environment,
                    envs, identity_k<Core>{});
                if (!car_r.has_value())
                    return car_r;
                auto cdr_r = cps_dispatch<MaxNodes, MaxList>(
                    arena.get(cc.cdr), arena, identity_k<Core>{}, environment,
                    envs, identity_k<Core>{});
                if (!cdr_r.has_value())
                    return cdr_r;
                Val const cons_args[2] = {car_r.value(), cdr_r.value()};
                auto pr = apply_prim<Core, default_max_pairs>(
                    list_op::cons, std::span<Val const>{cons_args},
                    environment.pairs());
                if (!pr.has_value())
                    return pr;
                auto r = cont(pr.value());
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            [&](elaborator::core_if<Core, MaxNodes> const &cif) -> Res {
                auto cond_r = cps_dispatch<MaxNodes, MaxList>(
                    arena.get(cif.condition), arena, identity_k<Core>{},
                    environment, envs, identity_k<Core>{});
                if (!cond_r.has_value())
                    return cond_r;
                if (is_true(cond_r.value()))
                    return cps_dispatch<MaxNodes, MaxList>(
                        arena.get(cif.consequent), arena, cont, environment,
                        envs, k);
                return cps_dispatch<MaxNodes, MaxList>(
                    arena.get(cif.alternative), arena, cont, environment, envs,
                    k);
            },
            // 60425319-c29d-44d1-b752-8945294942b9
            [&](elaborator::core_progn<Core, MaxNodes, MaxList> const &cp)
                -> Res {
                // "progn compiles to continuation chaining": every
                // expression but the last is evaluated for effect only
                // (identity/identity); the last tail-chains into the
                // CALLER's own (cont, k) rather than returning up through
                // this function -- the structure L14 (block/return-from)
                // hooks into.
                int const n = cp.exprs.size();
                for (int i = 0; i < n - 1; ++i) {
                    auto r = cps_dispatch<MaxNodes, MaxList>(
                        arena.get(cp.exprs[i]), arena, identity_k<Core>{},
                        environment, envs, identity_k<Core>{});
                    if (!r.has_value())
                        return r;
                }
                return cps_dispatch<MaxNodes, MaxList>(
                    arena.get(cp.exprs[n - 1]), arena, cont, environment, envs,
                    k);
            },
            // 60425319-c29d-44d1-b752-8945294942b9 end
            [&](elaborator::core_lambda<Core, MaxNodes, MaxList> const &)
                -> Res {
                // See env_arena's docs (env.hpp) for why this, rather
                // than an owning box or a call-stack-local, is how a
                // captured environment stays valid for as long as the
                // closure that captured it might be called.
                auto const *captured = envs.alloc(environment);
                Val v{closure<Core>{&node, captured}};
                auto r = cont(v);
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            [&](elaborator::core_function<Core, MaxNodes> const &cf) -> Res {
                return std::visit(
                    smd::fixpoint::overloaded{
                        [&](std::string_view name) -> Res {
                            // FUNCTION namespace only, per D4.
                            auto lr = environment.lookup_function(symbol{name});
                            if (!lr.has_value())
                                return Res{lr.error()};
                            auto r = cont(lr.value());
                            if (!r.has_value())
                                return r;
                            return k(r.value());
                        },
                        [&](smd::smdscheme::foundation::arena_box<
                            Core, MaxNodes> const &target) -> Res {
                            // An embedded (lambda ...): materializing it
                            // is itself the node's value, so tail-chain
                            // straight into it.
                            return cps_dispatch<MaxNodes, MaxList>(
                                arena.get(target), arena, cont, environment,
                                envs, k);
                        }},
                    cf.target);
            },
            [&](elaborator::core_application<Core, MaxNodes, MaxList> const
                    &app) -> Res {
                auto func_r = cps_dispatch<MaxNodes, MaxList>(
                    arena.get(app.func), arena, identity_k<Core>{}, environment,
                    envs, identity_k<Core>{});
                if (!func_r.has_value())
                    return func_r;

                smd::smdscheme::foundation::static_vector<Val, MaxList>
                    evaluated_args;
                for (int i = 0; i < app.args.size(); ++i) {
                    auto arg_r = cps_dispatch<MaxNodes, MaxList>(
                        arena.get(app.args[i]), arena, identity_k<Core>{},
                        environment, envs, identity_k<Core>{});
                    if (!arg_r.has_value())
                        return arg_r;
                    evaluated_args.push_back(arg_r.value());
                }
                return cps_apply<MaxNodes, MaxList>(
                    func_r.value(),
                    std::span<Val const>(evaluated_args.begin(),
                                         evaluated_args.end()),
                    arena, cont, environment, envs, k);
            },
            // Step L12 kinds: setq/defun/defvar/defparameter. The
            // reserve-then-patch defun sequence here is identical in
            // shape to eval_direct.hpp's core_defun case -- see
            // env::patch_function's docs for why that order is what lets
            // a self-recursive defun find its own name.
            // 842db09a-6e1d-4b69-b456-674c421cd693
            [&](elaborator::core_setq<Core, MaxNodes> const &cs) -> Res {
                auto val_r = cps_dispatch<MaxNodes, MaxList>(
                    arena.get(cs.value), arena, identity_k<Core>{}, environment,
                    envs, identity_k<Core>{});
                if (!val_r.has_value())
                    return val_r;
                auto assign_r =
                    environment.set_value(symbol{cs.name}, val_r.value());
                if (!assign_r.has_value())
                    return assign_r;
                auto r = cont(assign_r.value());
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            [&](elaborator::core_defun<Core, MaxNodes> const &cd) -> Res {
                auto const &lam_node = arena.get(cd.lambda);
                int loc =
                    environment.define_function(symbol{cd.name}, Val{nil_t{}});
                auto const *captured = envs.alloc(environment);
                Val fn_val{closure<Core>{&lam_node, captured}};
                environment.patch_function(symbol{cd.name}, loc, fn_val);
                // ANSI CL: defun returns the function name.
                Val name_val{symbol{cd.name}};
                auto r = cont(name_val);
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            // 842db09a-6e1d-4b69-b456-674c421cd693 end
            [&](elaborator::core_defvar<Core, MaxNodes> const &cdv) -> Res {
                // ANSI CL: defvar always proclaims special, but only
                // evaluates/binds the value form if not already bound.
                environment.declare_special(symbol{cdv.name});
                if (environment.lookup_value(symbol{cdv.name}).has_value()) {
                    Val name_val{symbol{cdv.name}};
                    auto r = cont(name_val);
                    if (!r.has_value())
                        return r;
                    return k(r.value());
                }
                auto val_r = cps_dispatch<MaxNodes, MaxList>(
                    arena.get(cdv.value), arena, identity_k<Core>{},
                    environment, envs, identity_k<Core>{});
                if (!val_r.has_value())
                    return val_r;
                environment.define_value(symbol{cdv.name}, val_r.value());
                Val name_val{symbol{cdv.name}};
                auto r = cont(name_val);
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            [&](elaborator::core_defparameter<Core, MaxNodes> const &cdp)
                -> Res {
                // ANSI CL: defparameter always proclaims special AND
                // always (re)evaluates and (re)binds.
                environment.declare_special(symbol{cdp.name});
                auto val_r = cps_dispatch<MaxNodes, MaxList>(
                    arena.get(cdp.value), arena, identity_k<Core>{},
                    environment, envs, identity_k<Core>{});
                if (!val_r.has_value())
                    return val_r;
                environment.define_value(symbol{cdp.name}, val_r.value());
                Val name_val{symbol{cdp.name}};
                auto r = cont(name_val);
                if (!r.has_value())
                    return r;
                return k(r.value());
            }},
        node.inner);
}

} // namespace detail

/// Wraps a core node and its arena in a @ref cps_code with an explicit
/// outer continuation @p cont.
///
/// The returned code, when called with a mutable environment, the shared
/// closure-capture arena, and a final continuation @p k, evaluates
/// @p node, passes the result to @p cont, then passes @p cont's result to
/// @p k.
///
/// @p arena is captured BY VALUE into the returned callable (matching the
/// Scheme original): the C++26 constexpr evaluation model requires
/// `tree_arena` elements captured inside a returned compiled expression to
/// outlive the source object's own lifetime (see `handoff.md`'s
/// "Architecture facts").
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum argument/list/body length.
/// @tparam Cont     Intermediate continuation type.
template <int MaxNodes, int MaxList, class Cont>
[[nodiscard]] constexpr auto
cps_of(elaborator::core_type<MaxNodes, MaxList> const &node,
       smd::smdscheme::foundation::tree_arena<
           elaborator::core_type<MaxNodes, MaxList>, MaxNodes>
           arena,
       Cont cont) {
    return cps_code{[node, arena, cont](auto &environment, auto &envs,
                                        auto k) constexpr {
        return detail::cps_dispatch<MaxNodes, MaxList>(node, arena, cont,
                                                       environment, envs, k);
    }};
}

/// Compiles a core node to a @ref cps_code using the identity as the
/// outermost intermediate continuation.
///
/// The resulting code evaluates the expression and returns the value
/// directly to the final continuation.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum argument/list/body length.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
compile_cps(elaborator::core_type<MaxNodes, MaxList> const &node,
            smd::smdscheme::foundation::tree_arena<
                elaborator::core_type<MaxNodes, MaxList>, MaxNodes>
                arena) {
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    return cps_of<MaxNodes, MaxList>(node, arena, detail::identity_k<Core>{});
}

} // namespace smd::smdlisp::closure

#endif
