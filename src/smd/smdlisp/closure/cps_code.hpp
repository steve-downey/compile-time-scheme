// src/smd/smdlisp/closure/cps_code.hpp                            -*-C++-*-
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
/// Adapted by copy from `smd::smdscheme::cps::cps_code` (step L13, per
/// `docs/cps-direction.md`: structural recursion over the flat arena, not
/// `fix<F>` folding). Placed in `smd::smdlisp::closure` rather than a
/// separate `smd::smdlisp::closure::cps` sub-namespace (the Scheme original
/// lives in `smd::smdscheme::cps` despite its file sitting in `closure/`) --
/// `AGENTS.md`'s namespace/directory-path alignment rule applies uniformly
/// to new work, and this file lives in `src/smd/smdlisp/closure/`. The
/// public names (`cps_code`, `cps_of`, `compile_cps`) are otherwise
/// unchanged from the Scheme original.
///
/// This header includes `eval_direct.hpp` solely to reuse its
/// `detail::to_list_op` bridge (`builtin_op` -> `pairs.hpp`'s `list_op`);
/// see that function's doc comment in `eval_direct.hpp` for why the two
/// enums are kept separate. No other coupling to @ref eval_direct exists --
/// @ref detail::cps_dispatch below is a complete, independent evaluator.
///
/// @c cps_code<F> wraps a callable @p F with signature
/// @code
///   (env<Core,MaxBindings>&, env_arena<Core,MaxBindings,MaxEnvs>&, K) ->
///   result<value_list<Core,MaxValues>>
/// @endcode
/// where @p K is the outermost continuation.  The payload is a
/// @ref value_list rather than a @ref value as of step L20: a Common Lisp
/// form produces a sequence of values, and making continuations n-ary is how
/// that sequence reaches `multiple-value-bind` without any ambient state.
/// Unlike the Scheme original's `Env const&` (a purely functional environment),
/// `smdlisp`'s
/// @ref env is threaded as a *mutable reference* -- `setq`/`defun`/`defvar`
/// mutate it in place (step L12), and the CPS threading below preserves
/// that: every recursive `cps_dispatch` call passes the same `environment`
/// object along, never a copy, so a later sibling in a `progn`/lambda-body
/// sequence observes an earlier sibling's side effect (see @ref
/// eval_direct's identical requirement, `eval_direct.hpp`). @p envs is the
/// caller-owned @ref env_arena that owns every environment a closure
/// materialized during evaluation captures; it must also outlive the call.
///
/// @tparam F The underlying callable type (deduced via the deduction guide).
template <class F>
struct cps_code {
    F f;

    /// Evaluates the CPS expression under @p environment/@p envs, invoking
    /// continuation @p k.
    template <class Env, class Envs, class K>
    constexpr auto operator()(Env &environment, Envs &envs, K k) const {
        return f(environment, envs, k);
    }
};

/// Deduction guide: @c cps_code(f) deduces @c cps_code<F>.
template <class F>
cps_code(F) -> cps_code<F>;

namespace detail {

/// The identity continuation: returns its argument unchanged.
///
/// Used as the terminal continuation in @ref cps_of and @ref compile_cps to
/// extract the final value, and internally by @ref cps_dispatch /
/// @ref cps_apply_function_value wherever a sub-expression's value is
/// needed inline (a condition, an argument, a `setq` right-hand side) rather
/// than tail-passed onward -- the same non-tail-position idiom
/// `smd::smdscheme::cps::detail::cps_dispatch` uses.
///
/// **Continuations are n-ary as of step L20.** A continuation is invoked
/// with a whole @ref value_list, not a single @ref value, because that is
/// what a Common Lisp form actually produces. Every continuation this
/// backend builds -- including this one and the ones a caller supplies to
/// @ref cps_of -- therefore takes and returns a `value_list`. A caller that
/// wants one value calls @ref primary_value on the result; that is exactly
/// what @ref closure_program does.
///
/// @tparam Core      Core AST type.
/// @tparam MaxValues Width of the value payload; see @ref default_max_values.
template <class Core, int MaxValues = default_max_values>
struct identity_k {
    constexpr auto operator()(value_list<Core, MaxValues> v) const
        -> smd::smdscheme::foundation::result<value_list<Core, MaxValues>> {
        return v;
    }
};

/// Forward declaration: @ref cps_dispatch and @ref cps_apply_function_value
/// recurse into each other, the same mutual-recursion shape @ref
/// eval_direct and @ref apply_function_value use (`eval_direct.hpp`): an
/// application dispatches its evaluated function value through
/// @ref cps_apply_function_value, and applying a closure evaluates its body
/// via @ref cps_dispatch.
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs,
          int MaxValues = default_max_values, class Cont, class K>
[[nodiscard]] constexpr auto cps_dispatch(
    elaborator::core_type<MaxNodes, MaxList> const &node,
    smd::smdscheme::foundation::tree_arena<
        elaborator::core_type<MaxNodes, MaxList>, MaxNodes> const &arena,
    env<elaborator::core_type<MaxNodes, MaxList>, MaxBindings> &environment,
    env_arena<elaborator::core_type<MaxNodes, MaxList>, MaxBindings, MaxEnvs>
        &envs,
    Cont const &cont, K const &k)
    -> smd::smdscheme::foundation::result<
        value_list<elaborator::core_type<MaxNodes, MaxList>, MaxValues>>;

/// CPS counterpart of @ref apply_function_value (`eval_direct.hpp`):
/// applies an already-evaluated function @ref value to already-evaluated
/// argument values, invoking @p cont with the result and @p k with @p
/// cont's result -- the same two-continuation shape @ref cps_dispatch uses.
///
/// Shared by ordinary application (`core_application`, via @ref
/// cps_dispatch) and the `funcall`/`apply` builtins (whose arguments are
/// already-evaluated values by the time this function needs them), for the
/// identical reason `apply_function_value` is a standalone function rather
/// than inlined into `core_application`'s handling: D4 gives `funcall`/
/// `apply` real call semantics.
///
/// @tparam MaxNodes    Arena capacity; bounds tree depth.
/// @tparam MaxList     Maximum argument/body/list length.
/// @tparam MaxBindings Environment capacity; must be 16 (see @ref
///                      cps_dispatch's doc comment for why).
/// @tparam MaxEnvs     Capacity of the @ref env_arena capturing closures.
/// @param  func_val The callee: a @ref builtin, @ref closure, or
///                   @ref foreign_function value. Any other value kind is a
///                   diagnosed "attempted to call non-function" error.
/// @param  args      The already-evaluated call arguments.
/// @param  arena     Core arena; must outlive the evaluation.
/// @param  heap      The shared pair heap (from the calling environment), or
///                    null; needed by the list-primitive builtins.
/// @param  envs      The shared environment arena; see @ref cps_dispatch.
/// @param  cont      The intermediate continuation applied to the result.
/// @param  k         The outer continuation applied to @p cont's result.
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs,
          int MaxValues = default_max_values, class Cont, class K>
[[nodiscard]] constexpr auto cps_apply_function_value(
    value<elaborator::core_type<MaxNodes, MaxList>> const &func_val,
    std::span<value<elaborator::core_type<MaxNodes, MaxList>> const> args,
    smd::smdscheme::foundation::tree_arena<
        elaborator::core_type<MaxNodes, MaxList>, MaxNodes> const &arena,
    pair_heap<elaborator::core_type<MaxNodes, MaxList>, default_max_pairs>
        *heap,
    env_arena<elaborator::core_type<MaxNodes, MaxList>, MaxBindings, MaxEnvs>
        &envs,
    Cont const &cont, K const &k)
    -> smd::smdscheme::foundation::result<
        value_list<elaborator::core_type<MaxNodes, MaxList>, MaxValues>> {
    static_assert(MaxBindings == 16,
                  "cps_apply_function_value currently requires "
                  "closure::env<Core,16> (value<Core>'s embedded closure "
                  "alternative is closure<Core,16>, unconditionally)");
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    using Val = value<Core>;
    using Vals = value_list<Core, MaxValues>;
    using Res = smd::smdscheme::foundation::result<Vals>;
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
                    auto r = cont(single_value<MaxValues>(Val{acc}));
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
                case builtin_op::atom:
                case builtin_op::append: {
                    auto pr = apply_prim<Core, default_max_pairs>(
                        to_list_op(bi.op), args, heap);
                    if (!pr.has_value())
                        return Res{pr.error()};
                    auto r = cont(single_value<MaxValues>(pr.value()));
                    if (!r.has_value())
                        return r;
                    return k(r.value());
                }
                case builtin_op::values: {
                    // ANSI CL: `values` is an ordinary function returning
                    // every argument as that many values.  The continuation
                    // being n-ary is precisely what lets this arm exist at
                    // all -- there is nowhere else to put the extras.
                    if (static_cast<int>(args.size()) > MaxValues)
                        return parse_error{
                            {}, "values: more values than MaxValues"};
                    Vals vs{};
                    for (auto const &a : args)
                        vs.push_back(a);
                    auto r = cont(vs);
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
                    return cps_apply_function_value<
                        MaxNodes, MaxList, MaxBindings, MaxEnvs, MaxValues>(
                        args[0], args.subspan(1), arena, heap, envs, cont, k);
                }
                case builtin_op::apply: {
                    // (apply f args... list): every argument but the first
                    // (the function) and the last (a list) is passed
                    // through as-is; the last argument is spread.
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
                        if (heap == nullptr)
                            return parse_error{
                                {}, "apply: environment has no pair heap"};
                        auto const &cell =
                            heap->get(std::get<pair_ref>(cur).loc);
                        spread.push_back(cell.car);
                        cur = cell.cdr;
                    }
                    return cps_apply_function_value<
                        MaxNodes, MaxList, MaxBindings, MaxEnvs, MaxValues>(
                        args[0],
                        std::span<Val const>(spread.begin(), spread.end()),
                        arena, heap, envs, cont, k);
                }
                }
                return parse_error{{}, "unknown builtin"};
            },
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

                // A fresh copy per call: params bind into it, and it is
                // discarded when the call returns. It shares the captured
                // environment's pair heap / variable store by pointer (see
                // env.hpp), which is how `setq` inside this call stays
                // visible to any other closure sharing the same store-
                // backed binding.
                env<Core, MaxBindings> new_env = *clo.captured;
                // A parameter naming a special variable binds DYNAMICALLY
                // (step L16); everything else binds lexically, as before.
                // Same call, same semantics, as eval_direct.hpp's closure
                // case -- the shared helper is the parity guarantee.
                auto const bound_r =
                    bind_lambda_parameters(lam.params, args, new_env, envs);
                if (!bound_r.has_value())
                    return Res{bound_r.error()};
                int const dynamic_count = bound_r.value();

                // Implicit progn: at least one body expression is
                // guaranteed by the elaborator (elaborate_lambda).
                //
                // A call is a continuation barrier, for the same reason
                // core_block, core_catch and core_unwind_protect are: EVERY
                // body statement, including the last, is dispatched with
                // identity_k/identity_k rather than tail-passing the
                // caller's cont/k, so that this frame regains control on
                // every exit path and can restore the dynamic bindings
                // before cont/k ever run. Tail-passing cont/k out of the
                // last body form would run the continuation while the
                // bindings were still in effect, and then have nowhere left
                // to undo them.
                int const n = lam.body.size();
                Res last{parse_error{{}, "lambda: empty body"}};
                for (int i = 0; i < n; ++i) {
                    last = cps_dispatch<MaxNodes, MaxList, MaxBindings, MaxEnvs,
                                        MaxValues>(
                        arena.get(lam.body[i]), arena, new_env, envs,
                        identity_k<Core, MaxValues>{},
                        identity_k<Core, MaxValues>{});
                    if (!last.has_value())
                        break;
                }
                unwind_dynamic_bindings(new_env, envs, dynamic_count);
                if (!last.has_value())
                    return last;
                // `last` is the last body form's whole value list, so a
                // function whose body ends in `(values ...)` returns all of
                // them -- the barrier costs nothing here, because the values
                // are carried in the return, not in ambient state.
                auto r = cont(last.value());
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            [&](foreign_function<Core> const &ff) -> Res {
                auto ff_r = ff.fn(args);
                if (!ff_r.has_value())
                    return Res{ff_r.error()};
                auto r = cont(single_value<MaxValues>(ff_r.value()));
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

/// Internal CPS dispatch over an `smdlisp` core AST node.
///
/// Adapted by copy from `smd::smdscheme::cps::detail::cps_dispatch`, per
/// `docs/cps-direction.md`'s structural-recursion-over-the-arena decision.
/// Covers all twenty core kinds (the twelve carried over from the CL
/// elaborator, plus `core_setq`/`core_defun`/`core_defvar` from step L12,
/// `core_block`/`core_return_from` from step L14, and
/// `core_catch`/`core_throw`/`core_unwind_protect` from step L15), using
/// the same evaluation semantics as @ref eval_direct (`eval_direct.hpp`) --
/// this function must be a drop-in CPS replacement for it (the L13 merge
/// criterion: every L11/L12 end-to-end test also passes through this path).
///
/// The two-continuation style is unchanged from the Scheme original: @p
/// cont is applied to the node's value; if that succeeds, @p k is applied
/// to @p cont's result. Wherever a sub-expression's value is needed inline
/// rather than tail-passed (an `if` condition, an application's arguments,
/// a `setq` right-hand side, a `defvar` initializer), the recursive call
/// uses @ref identity_k for both continuations -- the same idiom the
/// Scheme original uses, and the mechanism that lets non-tail
/// sub-evaluation look like an ordinary nested call while everything that
/// *is* in tail position (an `if` branch, `progn`'s/a lambda body's last
/// expression, an application's dispatch) properly tail-passes @p cont/@p
/// k onward.
///
/// **Environment mutation across a sequence.** @p environment is a mutable
/// reference, exactly as in @ref eval_direct, and every recursive call
/// below threads the *same* `environment` object -- never a copy -- through
/// sibling sub-expressions (a `progn`/lambda-body sequence, `if`'s
/// branches, one application's arguments). This is what makes
/// `(progn (defun f ...) (f ...))` and `((lambda (x) (setq x 2) x) 1)`
/// evaluate identically under CPS and under @ref eval_direct: a `defun`/
/// `defvar`/`setq` mutates `environment` in place, and a later sibling
/// evaluated with the same threaded reference observes it.
///
/// `setq`'s own continuation, per ANSI CL and unlike Scheme's `set!`,
/// receives the *assigned* value (the value of the last name/value pair),
/// never an unspecified marker -- there is no `unspecified` value kind in
/// `smdlisp`.
///
/// @tparam MaxNodes    Arena capacity; bounds tree depth.
/// @tparam MaxList     Maximum argument/body/list length.
/// @tparam MaxBindings Environment capacity; must be 16. `value<Core>`'s
///                      `closure<Core>` alternative is hard-wired to
///                      `MaxBindings == 16` (see @ref eval_direct's
///                      identical static_assert).
/// @tparam MaxEnvs     Capacity of the @ref env_arena that owns every
///                      environment a `lambda`/`function` form captures.
/// @tparam Cont        Intermediate continuation type.
/// @tparam K           Outer continuation type.
/// @param  node        Node to evaluate.
/// @param  arena       Core arena; must outlive the evaluation.
/// @param  environment Current variable/function bindings and pair heap;
///                      mutated in place by `setq`/`defun`/`defvar`/
///                      `defparameter` (see above).
/// @param  envs        Shared, caller-owned storage for captured
///                      environments; must outlive every closure this call
///                      (or anything it returns) might produce.
/// @param  cont        Intermediate continuation applied to the node's
///                      value.
/// @param  k           Outer continuation applied to @p cont's result.
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs,
          int MaxValues, class Cont, class K>
[[nodiscard]] constexpr auto cps_dispatch(
    elaborator::core_type<MaxNodes, MaxList> const &node,
    smd::smdscheme::foundation::tree_arena<
        elaborator::core_type<MaxNodes, MaxList>, MaxNodes> const &arena,
    env<elaborator::core_type<MaxNodes, MaxList>, MaxBindings> &environment,
    env_arena<elaborator::core_type<MaxNodes, MaxList>, MaxBindings, MaxEnvs>
        &envs,
    Cont const &cont, K const &k)
    -> smd::smdscheme::foundation::result<
        value_list<elaborator::core_type<MaxNodes, MaxList>, MaxValues>> {
    static_assert(MaxBindings == 16,
                  "cps_dispatch currently requires closure::env<Core,16> "
                  "(see the doc comment above)");
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    using Val = value<Core>;
    using Vals = value_list<Core, MaxValues>;
    using Res = smd::smdscheme::foundation::result<Vals>;
    using Id = identity_k<Core, MaxValues>;
    using smd::smdscheme::foundation::parse_error;

    return std::visit(
        smd::fixpoint::overloaded{
            [&](elaborator::core_integer const &ci) -> Res {
                auto r = cont(single_value<MaxValues>(Val{ci.value}));
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            [&](elaborator::core_symbol const &cs) -> Res {
                // VARIABLE namespace only, per D4 -- never lookup_function.
                auto lr = environment.lookup_value(symbol{cs.name.view()});
                if (!lr.has_value())
                    return Res{lr.error()};
                auto r = cont(single_value<MaxValues>(lr.value()));
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            [&](elaborator::core_keyword const &ck) -> Res {
                auto r =
                    cont(single_value<MaxValues>(Val{keyword{ck.name.view()}}));
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            [&](elaborator::core_nil const &) -> Res {
                auto r = cont(single_value<MaxValues>(Val{nil_t{}}));
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            [&](elaborator::core_true const &) -> Res {
                auto r = cont(single_value<MaxValues>(Val{symbol{"T"}}));
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
                            auto r = cont(single_value<MaxValues>(lr.value()));
                            if (!r.has_value())
                                return r;
                            return k(r.value());
                        },
                        [&](smd::smdscheme::foundation::arena_box<
                            Core, MaxNodes> const &target) -> Res {
                            // An embedded (lambda ...): evaluating it
                            // materializes the closure directly, tail-
                            // passing cont/k onward.
                            return cps_dispatch<MaxNodes, MaxList, MaxBindings,
                                                MaxEnvs, MaxValues>(
                                arena.get(target), arena, environment, envs,
                                cont, k);
                        }},
                    cf.target);
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
                auto r = cont(single_value<MaxValues>(v));
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            [&](elaborator::core_cons<Core, MaxNodes> const &cc) -> Res {
                auto car_r = cps_dispatch<MaxNodes, MaxList, MaxBindings,
                                          MaxEnvs, MaxValues>(
                    arena.get(cc.car), arena, environment, envs, Id{}, Id{});
                if (!car_r.has_value())
                    return car_r;
                auto cdr_r = cps_dispatch<MaxNodes, MaxList, MaxBindings,
                                          MaxEnvs, MaxValues>(
                    arena.get(cc.cdr), arena, environment, envs, Id{}, Id{});
                if (!cdr_r.has_value())
                    return cdr_r;
                Val const cons_args[2] = {primary_value(car_r.value()),
                                          primary_value(cdr_r.value())};
                auto cons_r = apply_prim<Core, default_max_pairs>(
                    list_op::cons, std::span<Val const>{cons_args},
                    environment.pairs());
                if (!cons_r.has_value())
                    return Res{cons_r.error()};
                auto r = cont(single_value<MaxValues>(cons_r.value()));
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            [&](elaborator::core_if<Core, MaxNodes> const &cif) -> Res {
                auto cond_r =
                    cps_dispatch<MaxNodes, MaxList, MaxBindings, MaxEnvs,
                                 MaxValues>(arena.get(cif.condition), arena,
                                            environment, envs, Id{}, Id{});
                if (!cond_r.has_value())
                    return cond_r;
                // The condition is a single-value context; the branch is in
                // tail position and tail-passes cont/k, so its whole value
                // list reaches the caller.
                auto const &branch = is_true(primary_value(cond_r.value()))
                                         ? cif.consequent
                                         : cif.alternative;
                return cps_dispatch<MaxNodes, MaxList, MaxBindings, MaxEnvs,
                                    MaxValues>(arena.get(branch), arena,
                                               environment, envs, cont, k);
            },
            // e01d7672-71a4-4b75-ba56-618003e4a28d
            [&](elaborator::core_progn<Core, MaxNodes, MaxList> const &cp)
                -> Res {
                int const n = cp.exprs.size();
                if (n == 0)
                    return parse_error{{}, "progn: empty"};
                for (int i = 0; i < n - 1; ++i) {
                    auto r =
                        cps_dispatch<MaxNodes, MaxList, MaxBindings, MaxEnvs,
                                     MaxValues>(arena.get(cp.exprs[i]), arena,
                                                environment, envs, Id{}, Id{});
                    if (!r.has_value())
                        return r;
                }
                return cps_dispatch<MaxNodes, MaxList, MaxBindings, MaxEnvs,
                                    MaxValues>(arena.get(cp.exprs[n - 1]),
                                               arena, environment, envs, cont,
                                               k);
            },
            // e01d7672-71a4-4b75-ba56-618003e4a28d end
            [&](elaborator::core_lambda<Core, MaxNodes, MaxList> const &)
                -> Res {
                // See env_arena's docs (env.hpp) for why this, rather than
                // an owning box or a call-stack-local, is how a captured
                // environment stays valid for as long as the closure that
                // captured it might be called.
                auto const *captured = envs.alloc(environment);
                auto r = cont(single_value<MaxValues>(
                    Val{closure<Core>{&node, captured}}));
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            [&](elaborator::core_application<Core, MaxNodes, MaxList> const
                    &app) -> Res {
                auto func_r = cps_dispatch<MaxNodes, MaxList, MaxBindings,
                                           MaxEnvs, MaxValues>(
                    arena.get(app.func), arena, environment, envs, Id{}, Id{});
                if (!func_r.has_value())
                    return func_r;

                // Each argument is a single-value context (ANSI CL): only the
                // primary value of `(values 1 2)` is passed, which is what
                // makes `(+ (values 1 2) 10)` be 11.
                smd::smdscheme::foundation::static_vector<Val, MaxList>
                    evaluated_args;
                for (int i = 0; i < app.args.size(); ++i) {
                    auto arg_r =
                        cps_dispatch<MaxNodes, MaxList, MaxBindings, MaxEnvs,
                                     MaxValues>(arena.get(app.args[i]), arena,
                                                environment, envs, Id{}, Id{});
                    if (!arg_r.has_value())
                        return arg_r;
                    evaluated_args.push_back(primary_value(arg_r.value()));
                }
                return cps_apply_function_value<MaxNodes, MaxList, MaxBindings,
                                                MaxEnvs, MaxValues>(
                    primary_value(func_r.value()),
                    std::span<Val const>(evaluated_args.begin(),
                                         evaluated_args.end()),
                    arena, environment.pairs(), envs, cont, k);
            },
            // feb43c72-2f43-42a8-ad92-fc070070a838
            [&](elaborator::core_setq<Core, MaxNodes, MaxList> const &sq)
                -> Res {
                // ANSI CL: assign each name/value pair left to right; the
                // *continuation* receives the value of the LAST assignment
                // -- never Scheme's `unspecified` (see eval_direct.hpp's
                // identical doc note on the return-type rationale).
                Res last{parse_error{{}, "setq: no assignments"}};
                for (int i = 0; i < sq.names.size(); ++i) {
                    auto val_r =
                        cps_dispatch<MaxNodes, MaxList, MaxBindings, MaxEnvs,
                                     MaxValues>(arena.get(sq.exprs[i]), arena,
                                                environment, envs, Id{}, Id{});
                    if (!val_r.has_value())
                        return val_r;
                    auto set_r = environment.set_value(
                        symbol{sq.names[i]}, primary_value(val_r.value()));
                    if (!set_r.has_value())
                        return Res{set_r.error()};
                    last = single_value<MaxValues>(set_r.value());
                }
                if (!last.has_value())
                    return last;
                auto r = cont(last.value());
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            // feb43c72-2f43-42a8-ad92-fc070070a838 end
            // 5b8f0e2c-3bfb-4f1d-9a92-6c2cea4e8994
            [&](elaborator::core_defun<Core, MaxNodes> const &cd) -> Res {
                // Reuses the lambda CPS lowering directly -- `defun`'s
                // embedded lambda node is a real core_lambda, evaluated the
                // same way an ordinary `(lambda ...)` expression is (see
                // the core_lambda case above); only the function-namespace
                // definition side effect and the name-as-return-value are
                // added here.
                auto clo_r =
                    cps_dispatch<MaxNodes, MaxList, MaxBindings, MaxEnvs,
                                 MaxValues>(arena.get(cd.lambda_node), arena,
                                            environment, envs, Id{}, Id{});
                if (!clo_r.has_value())
                    return clo_r;
                environment.define_function(symbol{cd.name},
                                            primary_value(clo_r.value()));
                // ANSI CL: `defun` returns the function name, not the
                // closure value.
                auto r = cont(single_value<MaxValues>(Val{symbol{cd.name}}));
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            // 5b8f0e2c-3bfb-4f1d-9a92-6c2cea4e8994 end
            // 180a37f4-6ab1-4657-a7c7-35bac23d150e
            [&](elaborator::core_defvar<Core, MaxNodes> const &dv) -> Res {
                environment.mark_special(symbol{dv.name});
                if (dv.has_init) {
                    bool const already_bound =
                        environment.lookup_value(symbol{dv.name}).has_value();
                    // `defparameter` always (re)initializes; `defvar` only
                    // if `name` is not already bound.
                    if (dv.is_parameter || !already_bound) {
                        auto init_r =
                            cps_dispatch<MaxNodes, MaxList, MaxBindings,
                                         MaxEnvs, MaxValues>(arena.get(dv.init),
                                                             arena, environment,
                                                             envs, Id{}, Id{});
                        if (!init_r.has_value())
                            return init_r;
                        environment.define_value(symbol{dv.name},
                                                 primary_value(init_r.value()));
                    }
                }
                // ANSI CL: `defvar`/`defparameter` return the variable
                // name.
                auto r = cont(single_value<MaxValues>(Val{symbol{dv.name}}));
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            [&](elaborator::core_block<Core, MaxNodes, MaxList> const &cb)
                -> Res {
                // A block is a continuation barrier: every body statement,
                // including the last, is dispatched with identity_k/
                // identity_k (never the caller's cont/k directly) so this
                // frame gets a chance to intercept a return-from targeting
                // its own exit_record before cont/k ever run -- see
                // eval_direct.hpp's identical core_block handling and
                // env.hpp's exit_record docs for the full mechanism this
                // mirrors exactly, just phrased in continuation-passing
                // terms.
                auto *rec = envs.alloc_exit(symbol{cb.name});
                environment.define_block(symbol{cb.name}, rec);

                Res last{parse_error{{}, "block: empty body"}};
                for (int i = 0; i < cb.body.size(); ++i) {
                    last =
                        cps_dispatch<MaxNodes, MaxList, MaxBindings, MaxEnvs,
                                     MaxValues>(arena.get(cb.body[i]), arena,
                                                environment, envs, Id{}, Id{});
                    if (!last.has_value()) {
                        if (last.error().message == block_unwind_marker &&
                            !rec->live) {
                            // A single value: an exit_record payload is not
                            // n-ary (DIV-0019).
                            auto r =
                                cont(single_value<MaxValues>(rec->payload));
                            if (!r.has_value())
                                return r;
                            return k(r.value());
                        }
                        rec->live = false;
                        return last;
                    }
                }
                rec->live = false;
                auto r = cont(last.value());
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            [&](elaborator::core_return_from<Core, MaxNodes> const &rf) -> Res {
                auto val_r = cps_dispatch<MaxNodes, MaxList, MaxBindings,
                                          MaxEnvs, MaxValues>(
                    arena.get(rf.expr), arena, environment, envs, Id{}, Id{});
                if (!val_r.has_value())
                    return val_r;
                auto rec_r = environment.lookup_block(symbol{rf.name});
                if (!rec_r.has_value())
                    return Res{rec_r.error()};
                auto *rec = rec_r.value();
                if (!rec->live)
                    return parse_error{{},
                                       "return-from: block has already exited"};
                rec->payload = primary_value(val_r.value());
                rec->live = false;
                return parse_error{{}, block_unwind_marker};
            },
            [&](elaborator::core_catch<Core, MaxNodes, MaxList> const &cc)
                -> Res {
                // Like core_block above, a catch is a continuation barrier:
                // every body statement, including the last, is dispatched
                // with identity_k/identity_k rather than the caller's
                // cont/k, so this frame regains control and can decide
                // whether an in-flight throw is aimed at it before cont/k
                // ever run -- and, just as importantly, so that the catch
                // stack is popped exactly once on every path out. See
                // eval_direct.hpp's identical core_catch handling and
                // env.hpp's catch_record docs.
                auto tag_r = cps_dispatch<MaxNodes, MaxList, MaxBindings,
                                          MaxEnvs, MaxValues>(
                    arena.get(cc.tag), arena, environment, envs, Id{}, Id{});
                if (!tag_r.has_value())
                    return tag_r;

                auto *rec = envs.push_catch(primary_value(tag_r.value()));
                Res last{parse_error{{}, "catch: empty body"}};
                for (int i = 0; i < cc.body.size(); ++i) {
                    last =
                        cps_dispatch<MaxNodes, MaxList, MaxBindings, MaxEnvs,
                                     MaxValues>(arena.get(cc.body[i]), arena,
                                                environment, envs, Id{}, Id{});
                    if (!last.has_value()) {
                        if (last.error().message == throw_unwind_marker &&
                            !rec->live) {
                            Val const caught = rec->payload;
                            envs.pop_catch();
                            auto r = cont(single_value<MaxValues>(caught));
                            if (!r.has_value())
                                return r;
                            return k(r.value());
                        }
                        rec->live = false;
                        envs.pop_catch();
                        return last;
                    }
                }
                rec->live = false;
                envs.pop_catch();
                auto r = cont(last.value());
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            [&](elaborator::core_throw<Core, MaxNodes> const &ct) -> Res {
                auto tag_r = cps_dispatch<MaxNodes, MaxList, MaxBindings,
                                          MaxEnvs, MaxValues>(
                    arena.get(ct.tag), arena, environment, envs, Id{}, Id{});
                if (!tag_r.has_value())
                    return tag_r;
                auto res_r = cps_dispatch<MaxNodes, MaxList, MaxBindings,
                                          MaxEnvs, MaxValues>(
                    arena.get(ct.result), arena, environment, envs, Id{}, Id{});
                if (!res_r.has_value())
                    return res_r;
                auto *rec = envs.find_catch(primary_value(tag_r.value()));
                if (rec == nullptr)
                    // Uncaught throw: a diagnosed error, never UB (D5).
                    // Neither cont nor k runs -- there is no value to hand
                    // onward, which is the whole point of an unwind.
                    return parse_error{{}, "throw: no catch for tag"};
                rec->payload = primary_value(res_r.value());
                rec->live = false;
                return parse_error{{}, throw_unwind_marker};
            },
            [&](elaborator::core_unwind_protect<Core, MaxNodes, MaxList> const
                    &up) -> Res {
                // The protected form is dispatched with identity_k/
                // identity_k, never the caller's cont/k: this frame must
                // regain control to run the cleanups on the NORMAL path
                // before cont/k see the value, as well as on the escape
                // path. Wrapping only one of the two would leak an exit
                // path uncleaned, which is exactly what `unwind-protect`
                // exists to prevent (see core_unwind_protect's docs and
                // eval_direct.hpp's identical handling).
                Res protected_r =
                    cps_dispatch<MaxNodes, MaxList, MaxBindings, MaxEnvs,
                                 MaxValues>(arena.get(up.protected_form), arena,
                                            environment, envs, Id{}, Id{});
                for (int i = 0; i < up.cleanup.size(); ++i) {
                    auto cl_r =
                        cps_dispatch<MaxNodes, MaxList, MaxBindings, MaxEnvs,
                                     MaxValues>(arena.get(up.cleanup[i]), arena,
                                                environment, envs, Id{}, Id{});
                    // ANSI CL: a cleanup form that itself exits non-locally
                    // (or errors) supersedes whatever was already in flight.
                    if (!cl_r.has_value())
                        return cl_r;
                }
                if (!protected_r.has_value())
                    return protected_r;
                // The protected form's WHOLE value list propagates.
                auto r = cont(protected_r.value());
                if (!r.has_value())
                    return r;
                return k(r.value());
            },
            [&](elaborator::core_multiple_value_bind<Core, MaxNodes> const &mvb)
                -> Res {
                // The values form is the language's only multiple-value
                // context; it is dispatched with Id/Id so that this frame,
                // not the caller's continuation, receives the value list.
                auto vals_r =
                    cps_dispatch<MaxNodes, MaxList, MaxBindings, MaxEnvs,
                                 MaxValues>(arena.get(mvb.values_form), arena,
                                            environment, envs, Id{}, Id{});
                if (!vals_r.has_value())
                    return vals_r;

                auto const &lam_node = arena.get(mvb.lambda_node);
                if (!std::holds_alternative<
                        elaborator::core_lambda<Core, MaxNodes, MaxList>>(
                        lam_node.inner))
                    return parse_error{
                        {},
                        "internal error: multiple-value-bind without a "
                        "lambda"};
                auto const &lam =
                    std::get<elaborator::core_lambda<Core, MaxNodes, MaxList>>(
                        lam_node.inner);

                auto clo_r = cps_dispatch<MaxNodes, MaxList, MaxBindings,
                                          MaxEnvs, MaxValues>(
                    lam_node, arena, environment, envs, Id{}, Id{});
                if (!clo_r.has_value())
                    return clo_r;

                // ANSI CL: pad with nil, drop the surplus.
                smd::smdscheme::foundation::static_vector<Val, MaxList> bound;
                for (int i = 0; i < lam.params.size(); ++i)
                    bound.push_back(i < vals_r.value().size()
                                        ? vals_r.value()[i]
                                        : Val{nil_t{}});

                // The call itself is a continuation barrier (see
                // cps_apply_function_value), so cont/k can be tail-passed
                // into it: the barrier is inside, around the body.
                return cps_apply_function_value<MaxNodes, MaxList, MaxBindings,
                                                MaxEnvs, MaxValues>(
                    primary_value(clo_r.value()),
                    std::span<Val const>(bound.begin(), bound.end()), arena,
                    environment.pairs(), envs, cont, k);
            }},
        // 180a37f4-6ab1-4657-a7c7-35bac23d150e end
        node.inner);
}

} // namespace detail

/// Wraps a core node and its arena in a @ref cps_code with an explicit
/// outer continuation @p cont.
///
/// The returned code, when called with an environment, an env_arena, and a
/// final continuation @p k, evaluates @p node, passes the result to @p
/// cont, then passes @p cont's result to @p k.
///
/// @tparam MaxNodes    Arena capacity.
/// @tparam MaxList     Maximum argument/list length.
/// @tparam MaxBindings Environment capacity; must be 16.
/// @tparam MaxEnvs     Capacity of the closure-capture env_arena.
/// @tparam Cont        Intermediate continuation type.
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs,
          int MaxValues = default_max_values, class Cont>
[[nodiscard]] constexpr auto
cps_of(elaborator::core_type<MaxNodes, MaxList> const &node,
       smd::smdscheme::foundation::tree_arena<
           elaborator::core_type<MaxNodes, MaxList>, MaxNodes>
           arena,
       Cont cont) {
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    return cps_code{
        [node, arena, cont](env<Core, MaxBindings> &environment,
                            env_arena<Core, MaxBindings, MaxEnvs> &envs,
                            auto k) constexpr {
            return detail::cps_dispatch<MaxNodes, MaxList, MaxBindings, MaxEnvs,
                                        MaxValues>(node, arena, environment,
                                                   envs, cont, k);
        }};
}

/// Compiles a core node to a @ref cps_code using the identity as the
/// outermost intermediate continuation.
///
/// The resulting code evaluates the expression and returns the value
/// directly to the final continuation.
///
/// @tparam MaxNodes    Arena capacity.
/// @tparam MaxList     Maximum argument/list length.
/// @tparam MaxBindings Environment capacity; must be 16.
/// @tparam MaxEnvs     Capacity of the closure-capture env_arena.
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs,
          int MaxValues = default_max_values>
[[nodiscard]] constexpr auto
compile_cps(elaborator::core_type<MaxNodes, MaxList> const &node,
            smd::smdscheme::foundation::tree_arena<
                elaborator::core_type<MaxNodes, MaxList>, MaxNodes>
                arena) {
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    return cps_of<MaxNodes, MaxList, MaxBindings, MaxEnvs, MaxValues>(
        node, arena, detail::identity_k<Core, MaxValues>{});
}

} // namespace smd::smdlisp::closure

#endif
