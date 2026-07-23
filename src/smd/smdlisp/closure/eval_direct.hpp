// src/smd/smdlisp/closure/eval_direct.hpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_CLOSURE_EVAL_DIRECT_HPP
#define SRC_SMD_SMDLISP_CLOSURE_EVAL_DIRECT_HPP

#include <smd/smdlisp/closure/env.hpp>
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

namespace detail {

/// Bridges a @ref builtin_op naming one of the nine list primitives onto
/// the @ref list_op enum @ref pairs.hpp::apply_prim actually consumes.
///
/// The two enums are deliberately kept separate types (see @ref builtin_op's
/// docs: `pairs.hpp` includes `value.hpp`, so `value.hpp` cannot name
/// `pairs.hpp`'s `list_op` back without a header cycle), but they share the
/// same nine names by construction, so the bridge is a same-name lookup,
/// not a semantic mapping.  Only called for the nine @p op values listed
/// below; @c add/@c multiply/@c funcall/@c apply are handled directly by
/// @ref apply_function_value and never reach this function.
[[nodiscard]] constexpr auto to_list_op(builtin_op op) -> list_op {
    switch (op) {
    case builtin_op::cons:
        return list_op::cons;
    case builtin_op::car:
        return list_op::car;
    case builtin_op::cdr:
        return list_op::cdr;
    case builtin_op::list:
        return list_op::list;
    case builtin_op::null:
        return list_op::null;
    case builtin_op::eq:
        return list_op::eq;
    case builtin_op::eql:
        return list_op::eql;
    case builtin_op::atom:
        return list_op::atom;
    case builtin_op::append:
        return list_op::append;
    case builtin_op::add:
    case builtin_op::multiply:
    case builtin_op::funcall:
    case builtin_op::apply:
        break;
    }
    return list_op::cons; // Unreachable: see the doc comment above.
}

} // namespace detail

/// Forward declaration: @ref eval_direct and @ref apply_function_value
/// recurse into each other (a call evaluates a lambda body via
/// @ref eval_direct; an application dispatches the evaluated function
/// value via @ref apply_function_value).
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs>
[[nodiscard]] constexpr auto eval_direct(
    elaborator::core_type<MaxNodes, MaxList> const &node,
    smd::smdscheme::foundation::tree_arena<
        elaborator::core_type<MaxNodes, MaxList>, MaxNodes> const &arena,
    env<elaborator::core_type<MaxNodes, MaxList>, MaxBindings> &environment,
    env_arena<elaborator::core_type<MaxNodes, MaxList>, MaxBindings, MaxEnvs>
        &envs)
    -> smd::smdscheme::foundation::result<
        value<elaborator::core_type<MaxNodes, MaxList>>>;

/// Applies an already-evaluated function @ref value to already-evaluated
/// argument values.
///
/// This is the single call-dispatch point shared by ordinary application
/// (`core_application`, whose arguments are core expressions evaluated one
/// at a time) and the `funcall`/`apply` builtins (whose arguments are
/// already-evaluated values by the time this function needs them) — the
/// same unification `smd::smdscheme::eval_direct` gets implicitly by
/// having only one caller; `smdlisp` needs it explicitly because D4 gives
/// `funcall`/`apply` real call semantics rather than leaving them as
/// ordinary two-argument builtins.
///
/// @tparam MaxNodes    Arena capacity; bounds tree depth.
/// @tparam MaxList     Maximum argument/body/list length.
/// @tparam MaxBindings Environment capacity; must be 16 (see @ref
///                      eval_direct's doc comment for why).
/// @tparam MaxEnvs     Capacity of the @ref env_arena capturing closures.
/// @param  func_val The callee: a @ref builtin, @ref closure, or
///                   @ref foreign_function value.  Any other value kind is
///                   a diagnosed "attempted to call non-function" error.
/// @param  args      The already-evaluated call arguments.
/// @param  arena     Core arena; must outlive the evaluation.
/// @param  heap      The shared pair heap (from the calling environment),
///                    or null; needed by the list-primitive builtins.
/// @param  envs      The shared environment arena that owns every
///                    environment a closure materialized during this call
///                    captures (see @ref env_arena).
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs>
[[nodiscard]] constexpr auto apply_function_value(
    value<elaborator::core_type<MaxNodes, MaxList>> const &func_val,
    std::span<value<elaborator::core_type<MaxNodes, MaxList>> const> args,
    smd::smdscheme::foundation::tree_arena<
        elaborator::core_type<MaxNodes, MaxList>, MaxNodes> const &arena,
    pair_heap<elaborator::core_type<MaxNodes, MaxList>, default_max_pairs>
        *heap,
    env_arena<elaborator::core_type<MaxNodes, MaxList>, MaxBindings, MaxEnvs>
        &envs)
    -> smd::smdscheme::foundation::result<
        value<elaborator::core_type<MaxNodes, MaxList>>> {
    static_assert(MaxBindings == 16,
                  "apply_function_value currently requires "
                  "closure::env<Core,16> (value<Core>'s embedded closure "
                  "alternative is closure<Core,16>, unconditionally)");
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    using Val = value<Core>;
    using Res = smd::smdscheme::foundation::result<Val>;
    using smd::smdscheme::foundation::parse_error;

    return std::visit(
        smd::fixpoint::overloaded{
            // 1d30e953-16e2-4812-9eee-10934eafdec3
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
                    return Val{acc};
                }
                case builtin_op::cons:
                case builtin_op::car:
                case builtin_op::cdr:
                case builtin_op::list:
                case builtin_op::null:
                case builtin_op::eq:
                case builtin_op::eql:
                case builtin_op::atom:
                case builtin_op::append:
                    return apply_prim<Core, default_max_pairs>(
                        detail::to_list_op(bi.op), args, heap);
                case builtin_op::funcall: {
                    // (funcall f args...): the first argument IS the
                    // function to call; the rest are its call arguments.
                    if (args.empty())
                        return parse_error{
                            {}, "funcall: expected a function argument"};
                    return apply_function_value<MaxNodes, MaxList, MaxBindings,
                                                MaxEnvs>(
                        args[0], args.subspan(1), arena, heap, envs);
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
                        if (heap == nullptr)
                            return parse_error{
                                {}, "apply: environment has no pair heap"};
                        auto const &cell =
                            heap->get(std::get<pair_ref>(cur).loc);
                        spread.push_back(cell.car);
                        cur = cell.cdr;
                    }
                    return apply_function_value<MaxNodes, MaxList, MaxBindings,
                                                MaxEnvs>(
                        args[0],
                        std::span<Val const>(spread.begin(), spread.end()),
                        arena, heap, envs);
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

                env<Core, MaxBindings> new_env = *clo.captured;
                for (int i = 0; i < lam.params.size(); ++i)
                    new_env.define_value(symbol{lam.params[i]}, args[i]);

                // Implicit progn: at least one body expression is
                // guaranteed by the elaborator (elaborate_lambda).
                Res last{parse_error{{}, "lambda: empty body"}};
                for (int i = 0; i < lam.body.size(); ++i) {
                    last = eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                        arena.get(lam.body[i]), arena, new_env, envs);
                    if (!last.has_value())
                        return last;
                }
                return last;
            },
            [&](foreign_function<Core> const &ff) -> Res {
                return ff.fn(args);
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
    // 1d30e953-16e2-4812-9eee-10934eafdec3 end
}

/// Directly evaluates a core AST node in the given environment.
///
/// A straightforward tree-walking interpreter over the `smdlisp` core AST
/// (adapted from `smd::smdscheme::eval_direct`; see that file's docs for
/// the shared architecture).  `smdlisp` deltas, all decided by this step:
///
///  - **Lisp-2 lookup (D4).** An application head (`core_function`) always
///    resolves through @ref env::lookup_function; an ordinary expression
///    symbol (`core_symbol`) always resolves through @ref env::lookup_value.
///    The two namespaces never interact — see @ref env's docs.
///  - **`nil`/`t` truthiness (D3).** Every branch (currently only
///    `core_if`) decides truth by calling @ref is_true, never by a
///    per-site encoding.  `core_nil` evaluates to the sole false value;
///    `core_true` evaluates to the canonical true value, the symbol `T`
///    (matching `pairs.hpp::apply_prim`'s predicates, so `(eq t 'T)` and
///    friends agree with what `if`/`and`/`or`-shaped code observes).
///  - **Implicit progn.** `core_lambda`'s body is already a sequence in
///    the elaborated core (unlike Scheme's single-expression body), so no
///    separate wrapping is needed; @ref apply_function_value walks it.
///  - **Closures capture both namespaces.** A closure's captured
///    environment is a full @ref env copy, which already carries both the
///    variable and function binding lists — one capture, two namespaces.
///  - **`funcall`/`apply` have call semantics**, not `apply_prim` cases;
///    see @ref apply_function_value.
///  - **`environment` is a mutable reference (step L12), not `const`.**
///    `setq`/`defun`/`defvar`/`defparameter` all need to mutate the
///    ambient environment in place (a new function/variable binding, a new
///    special mark, or -- for `setq` -- a write through the shared
///    @ref store) such that *later* expressions evaluated with the same
///    threaded environment observe the change. `core_progn`'s and a
///    closure body's sequential evaluation loops already pass the exact
///    same `environment` object across iterations (never a copy per
///    iteration), so this is the only change needed to make
///    `(progn (defun f ...) (f ...))` and `((lambda (x) (setq x 2) x) 1)`
///    both see their own earlier side effects. Evaluating sibling
///    subexpressions (e.g. `if`'s branches, or one application's
///    arguments) still shares this same reference, matching ANSI CL's
///    "side effects are visible in evaluation order" model.
///
/// @tparam MaxNodes    Arena capacity; bounds tree depth.
/// @tparam MaxList     Maximum argument/body/list length.
/// @tparam MaxBindings Environment capacity; must be 16.  `value<Core>`'s
///                      `closure<Core>` alternative is hard-wired to
///                      `MaxBindings == 16` (the same wart
///                      `smd::smdscheme::eval_direct` static_asserts
///                      against); step L9 parameterized the `closure`
///                      struct itself but `value<Core>`'s alias still
///                      names the unparameterized default, so this
///                      constraint survives until a later step fully
///                      threads `MaxBindings` through `value<Core>`.
/// @tparam MaxEnvs     Capacity of the @ref env_arena that owns every
///                      environment a `lambda`/`function` form captures
///                      while evaluating @p node (see @ref env_arena's
///                      docs for why this arena exists at all: it is how
///                      L9's open closure-capture-ownership question is
///                      resolved).
/// @param  node        The core node to evaluate.
/// @param  arena       Core arena; must outlive the evaluation.
/// @param  environment Current variable/function bindings and pair heap;
///                      mutated in place by `setq`/`defun`/`defvar`/
///                      `defparameter` (see the mutable-reference note
///                      above).
/// @param  envs        Shared, caller-owned storage for captured
///                      environments; must outlive every closure this
///                      call (or anything it returns) might produce.
/// @return The evaluated @ref value on success, or a
///         @ref smd::smdscheme::foundation::parse_error on type error,
///         arity mismatch, or unbound variable/undefined function.
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs>
[[nodiscard]] constexpr auto eval_direct(
    elaborator::core_type<MaxNodes, MaxList> const &node,
    smd::smdscheme::foundation::tree_arena<
        elaborator::core_type<MaxNodes, MaxList>, MaxNodes> const &arena,
    env<elaborator::core_type<MaxNodes, MaxList>, MaxBindings> &environment,
    env_arena<elaborator::core_type<MaxNodes, MaxList>, MaxBindings, MaxEnvs>
        &envs)
    -> smd::smdscheme::foundation::result<
        value<elaborator::core_type<MaxNodes, MaxList>>> {
    static_assert(MaxBindings == 16,
                  "eval_direct currently requires closure::env<Core,16> "
                  "(see the doc comment above)");
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    using Val = value<Core>;
    using Res = smd::smdscheme::foundation::result<Val>;
    using smd::smdscheme::foundation::parse_error;

    return std::visit(
        smd::fixpoint::overloaded{
            [&](elaborator::core_integer const &ci) -> Res {
                return Val{ci.value};
            },
            // bd5cf19a-ccb0-45a1-b208-b76970bcb0c6
            [&](elaborator::core_symbol const &cs) -> Res {
                // VARIABLE namespace only, per D4 -- never lookup_function.
                return environment.lookup_value(symbol{cs.name.view()});
            },
            [&](elaborator::core_keyword const &ck) -> Res {
                return Val{keyword{ck.name.view()}};
            },
            [&](elaborator::core_nil const &) -> Res { return Val{nil_t{}}; },
            [&](elaborator::core_true const &) -> Res {
                return Val{symbol{"T"}};
            },
            [&](elaborator::core_function<Core, MaxNodes> const &cf) -> Res {
                return std::visit(
                    smd::fixpoint::overloaded{
                        [&](std::string_view name) -> Res {
                            // FUNCTION namespace only, per D4 -- this is
                            // what makes an application head, `#'name`,
                            // and `(function name)` all resolve the same
                            // way, distinctly from a VARIABLE reference to
                            // the same spelling.
                            return environment.lookup_function(symbol{name});
                        },
                        [&](smd::smdscheme::foundation::arena_box<
                            Core, MaxNodes> const &target) -> Res {
                            // An embedded (lambda ...): evaluating it
                            // materializes the closure directly, no
                            // lookup.
                            return eval_direct<MaxNodes, MaxList, MaxBindings,
                                               MaxEnvs>(
                                arena.get(target), arena, environment, envs);
                        }},
                    cf.target);
            },
            // bd5cf19a-ccb0-45a1-b208-b76970bcb0c6 end
            [&](elaborator::core_quote const &cq) -> Res {
                return std::visit(
                    smd::fixpoint::overloaded{
                        [](int i) -> Val { return Val{i}; },
                        [](elaborator::core_symbol const &sym) -> Val {
                            return Val{symbol{sym.name.view()}};
                        },
                        [](elaborator::core_keyword const &kw) -> Val {
                            return Val{keyword{kw.name.view()}};
                        }},
                    cq.atom);
            },
            [&](elaborator::core_cons<Core, MaxNodes> const &cc) -> Res {
                auto car_r =
                    eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                        arena.get(cc.car), arena, environment, envs);
                if (!car_r.has_value())
                    return car_r.error();
                auto cdr_r =
                    eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                        arena.get(cc.cdr), arena, environment, envs);
                if (!cdr_r.has_value())
                    return cdr_r.error();
                Val const cons_args[2] = {car_r.value(), cdr_r.value()};
                return apply_prim<Core, default_max_pairs>(
                    list_op::cons, std::span<Val const>{cons_args},
                    environment.pairs());
            },
            // 6332b396-3733-4eb0-adc5-af7a57adb809
            [&](elaborator::core_if<Core, MaxNodes> const &cif) -> Res {
                auto cond_r =
                    eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                        arena.get(cif.condition), arena, environment, envs);
                if (!cond_r.has_value())
                    return cond_r.error();
                if (is_true(cond_r.value()))
                    return eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                        arena.get(cif.consequent), arena, environment, envs);
                return eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                    arena.get(cif.alternative), arena, environment, envs);
            },
            // 6332b396-3733-4eb0-adc5-af7a57adb809 end
            [&](elaborator::core_progn<Core, MaxNodes, MaxList> const &cp)
                -> Res {
                Res last{parse_error{{}, "progn: empty"}};
                for (int i = 0; i < cp.exprs.size(); ++i) {
                    last = eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                        arena.get(cp.exprs[i]), arena, environment, envs);
                    if (!last.has_value())
                        return last;
                }
                return last;
            },
            [&](elaborator::core_lambda<Core, MaxNodes, MaxList> const &)
                -> Res {
                // See env_arena's docs (env.hpp) for why this, rather than
                // an owning box or a call-stack-local, is how a captured
                // environment stays valid for as long as the closure that
                // captured it might be called.
                auto const *captured = envs.alloc(environment);
                return Val{closure<Core>{&node, captured}};
            },
            [&](elaborator::core_application<Core, MaxNodes, MaxList> const
                    &app) -> Res {
                auto func_r =
                    eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                        arena.get(app.func), arena, environment, envs);
                if (!func_r.has_value())
                    return func_r.error();

                smd::smdscheme::foundation::static_vector<Val, MaxList>
                    evaluated_args;
                for (int i = 0; i < app.args.size(); ++i) {
                    auto arg_r =
                        eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                            arena.get(app.args[i]), arena, environment, envs);
                    if (!arg_r.has_value())
                        return arg_r.error();
                    evaluated_args.push_back(arg_r.value());
                }
                return apply_function_value<MaxNodes, MaxList, MaxBindings,
                                            MaxEnvs>(
                    func_r.value(),
                    std::span<Val const>(evaluated_args.begin(),
                                         evaluated_args.end()),
                    arena, environment.pairs(), envs);
            },
            // e6a2c1de-3b2a-4c8d-9c6f-1a7e5b9d2f30
            [&](elaborator::core_setq<Core, MaxNodes, MaxList> const &sq)
                -> Res {
                // ANSI CL: assign each name/value pair left to right,
                // yielding the value of the LAST assignment -- never
                // Scheme's `unspecified` (see this function's own doc
                // comment / handoff for the return-type rationale).
                Res last{parse_error{{}, "setq: no assignments"}};
                for (int i = 0; i < sq.names.size(); ++i) {
                    auto val_r =
                        eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                            arena.get(sq.exprs[i]), arena, environment, envs);
                    if (!val_r.has_value())
                        return val_r.error();
                    auto set_r = environment.set_value(symbol{sq.names[i]},
                                                       val_r.value());
                    if (!set_r.has_value())
                        return set_r.error();
                    last = set_r;
                }
                return last;
            },
            [&](elaborator::core_defun<Core, MaxNodes> const &cd) -> Res {
                // The embedded lambda captures the environment in force
                // right now -- the same mechanism an ordinary `(lambda
                // ...)` expression uses (see the core_lambda case above).
                auto clo_r =
                    eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                        arena.get(cd.lambda_node), arena, environment, envs);
                if (!clo_r.has_value())
                    return clo_r.error();
                environment.define_function(symbol{cd.name}, clo_r.value());
                // ANSI CL: `defun` returns the function name, not the
                // closure value.
                return Val{symbol{cd.name}};
            },
            [&](elaborator::core_defvar<Core, MaxNodes> const &dv) -> Res {
                environment.mark_special(symbol{dv.name});
                if (dv.has_init) {
                    bool const already_bound =
                        environment.lookup_value(symbol{dv.name}).has_value();
                    // `defparameter` always (re)initializes; `defvar` only
                    // if `name` is not already bound.
                    if (dv.is_parameter || !already_bound) {
                        auto init_r = eval_direct<MaxNodes, MaxList,
                                                  MaxBindings, MaxEnvs>(
                            arena.get(dv.init), arena, environment, envs);
                        if (!init_r.has_value())
                            return init_r.error();
                        environment.define_value(symbol{dv.name},
                                                 init_r.value());
                    }
                }
                // ANSI CL: `defvar`/`defparameter` return the variable
                // name.
                return Val{symbol{dv.name}};
            }},
        // e6a2c1de-3b2a-4c8d-9c6f-1a7e5b9d2f30 end
        node.inner);
}

} // namespace smd::smdlisp::closure

#endif
