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
/// below; @c add/@c multiply/@c funcall/@c apply/@c values are handled
/// directly by @ref apply_function_value_values and never reach this
/// function.
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
    case builtin_op::values:
        break;
    }
    return list_op::cons; // Unreachable: see the doc comment above.
}

/// Converts an elaborated `tagbody` tag into the runtime @ref value that keys
/// it in the environment's tag namespace (step L23).
///
/// Shared verbatim by all three backends, and the whole reason tag comparison
/// needs no comparison code of its own: a symbol tag becomes a @ref symbol
/// value and an integer tag an @c int value, and @ref value's @c operator==
/// then compares them by name and by numeric value respectively -- which is
/// exactly what ANSI CL specifies (`eq` for symbol tags, `eql` for integer
/// ones).
template <class Core>
[[nodiscard]] constexpr auto tag_key(elaborator::core_tag const &t)
    -> value<Core> {
    if (std::holds_alternative<int>(t.key))
        return value<Core>{std::get<int>(t.key)};
    return value<Core>{symbol{std::get<std::string_view>(t.key)}};
}

// e0468b60-4a31-4526-bd12-722bbd15ceac
/// Undoes the innermost @p count dynamic bindings established by
/// @ref bind_lambda_parameters, writing each saved value back into the
/// special variable's single cell (step L16).
///
/// Shared verbatim by both backends (`eval_direct.hpp` and `cps_code.hpp`).
/// It is deliberately total -- no error channel, nothing to propagate -- so
/// that a caller can restore *unconditionally*, on the one path where a
/// value, an ordinary error, a `return-from` unwind and a `throw` unwind
/// have already been merged back into a single `result`. That is the same
/// shape `core_unwind_protect` has, and for the same reason: a dynamic
/// binding must be undone on every exit path out of its extent, and the
/// cheapest way to be sure of that is to have only one exit path.
///
/// @p callee_env is only needed for its @ref env::set_value, i.e. for the
/// @ref store it shares with every other environment naming the same cell;
/// which environment object is passed is immaterial as long as it resolves
/// the name to that cell, which the callee environment does by construction
/// (@ref bind_lambda_parameters never gives a special a shadowing binding).
template <class Env, class Envs>
constexpr auto unwind_dynamic_bindings(Env const &callee_env, Envs &envs,
                                       int count) -> void {
    for (int i = 0; i < count; ++i) {
        auto const rec = envs.pop_dynamic();
        // Cannot fail: establishing the binding already proved the name
        // resolves to a mutable cell, and nothing since could have unbound it.
        auto const restored = callee_env.set_value(rec.name, rec.saved);
        static_cast<void>(restored);
    }
}

/// Binds a lambda's parameters into @p callee_env, dynamically for those
/// naming a special variable and lexically for all the rest (step L16), and
/// returns how many dynamic bindings it established.
///
/// This is the single place where the `defvar`/`defparameter` mark
/// (@ref env::mark_special) starts to *mean* something. Every `smdlisp`
/// binding form -- `let`, `let*`, a `lambda` parameter list, a `defun`
/// parameter list -- is lowered by the elaborator to a lambda application,
/// so one decision here covers all of them, in both backends.
///
///  - **Not special:** @ref env::define_value, exactly as before: a fresh
///    binding in @p callee_env (a fresh @ref store cell in mutable mode),
///    shadowing anything outside. Purely lexical; invisible to a function
///    called from the body that did not capture this environment.
///  - **Special:** no binding is added at all. The variable's one existing
///    cell is saved (@ref env::lookup_value), overwritten with the argument
///    (@ref env::set_value), and the saved value handed to
///    @ref env_arena::push_dynamic for the matching
///    @ref unwind_dynamic_bindings. Because the cell is shared by every
///    environment that names the variable -- including captures made long
///    before this call -- the new value is what *any* function called during
///    the body's extent sees. That is dynamic scope, and it is the whole
///    mechanism.
///
/// A special with no value cell to save is a diagnosed error rather than a
/// fresh dynamic binding; see DIV-0014 (`docs/divergences/`). If that (or a
/// store-less environment) is hit partway through a parameter list, the
/// bindings already established are undone before returning, so the caller
/// never has to unwind a partially-bound frame.
template <class Params, class Val, class Env, class Envs>
[[nodiscard]] constexpr auto bind_lambda_parameters(Params const &params,
                                                    std::span<Val const> args,
                                                    Env &callee_env, Envs &envs)
    -> smd::smdscheme::foundation::result<int> {
    int established = 0;
    for (int i = 0; i < params.size(); ++i) {
        symbol const name{params[i]};
        if (!callee_env.is_special(name)) {
            callee_env.define_value(name, args[i]);
            continue;
        }
        auto const saved = callee_env.lookup_value(name);
        if (!saved.has_value()) {
            unwind_dynamic_bindings(callee_env, envs, established);
            return smd::smdscheme::foundation::parse_error{
                {}, "dynamic binding: special variable is unbound"};
        }
        auto const bound = callee_env.set_value(name, args[i]);
        if (!bound.has_value()) {
            unwind_dynamic_bindings(callee_env, envs, established);
            return bound.error();
        }
        envs.push_dynamic(name, saved.value());
        ++established;
    }
    return established;
}
// e0468b60-4a31-4526-bd12-722bbd15ceac end

} // namespace detail

// 4f5a55a0-11e1-4ab6-83d2-01fbdbdd8eea
/// Forward declarations of this header's four mutually recursive entry
/// points.
///
/// Since step L20 each of the two operations comes in a pair, and the naming
/// convention is uniform across both closure backends:
///
///  - the `_values` spelling is the **primitive**: it returns the form's
///    whole @ref value_list, which is what Common Lisp's evaluation relation
///    actually produces;
///  - the plain spelling is a **single-value adapter** over it, taking
///    @ref primary_value of the result.
///
/// Every context in the language is a single-value context except one -- the
/// values form of a `multiple-value-bind` -- so the adapters are what almost
/// every caller wants, and they are what the pre-L20 public surface
/// (`macroexpand/expander.hpp`, the tests) keeps using unchanged.  What the
/// primitives buy is that multiple values *propagate* correctly out of every
/// tail position: an `if` branch, a `progn`'s last form, a lambda body's last
/// form, a `block`/`catch` body's last form, and the protected form of an
/// `unwind-protect` all recurse through `_values`, so
/// `(multiple-value-bind (a b) (if t (values 1 2) nil) ...)` sees both
/// values.
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs,
          int MaxValues = default_max_values>
[[nodiscard]] constexpr auto eval_direct_values(
    elaborator::core_type<MaxNodes, MaxList> const &node,
    smd::smdscheme::foundation::tree_arena<
        elaborator::core_type<MaxNodes, MaxList>, MaxNodes> const &arena,
    env<elaborator::core_type<MaxNodes, MaxList>, MaxBindings> &environment,
    env_arena<elaborator::core_type<MaxNodes, MaxList>, MaxBindings, MaxEnvs>
        &envs)
    -> smd::smdscheme::foundation::result<
        value_list<elaborator::core_type<MaxNodes, MaxList>, MaxValues>>;

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
// 4f5a55a0-11e1-4ab6-83d2-01fbdbdd8eea end

/// Applies an already-evaluated function @ref value to already-evaluated
/// argument values, returning **all** the values it produced.
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
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs,
          int MaxValues = default_max_values>
[[nodiscard]] constexpr auto apply_function_value_values(
    value<elaborator::core_type<MaxNodes, MaxList>> const &func_val,
    std::span<value<elaborator::core_type<MaxNodes, MaxList>> const> args,
    smd::smdscheme::foundation::tree_arena<
        elaborator::core_type<MaxNodes, MaxList>, MaxNodes> const &arena,
    pair_heap<elaborator::core_type<MaxNodes, MaxList>, default_max_pairs>
        *heap,
    env_arena<elaborator::core_type<MaxNodes, MaxList>, MaxBindings, MaxEnvs>
        &envs)
    -> smd::smdscheme::foundation::result<
        value_list<elaborator::core_type<MaxNodes, MaxList>, MaxValues>> {
    static_assert(MaxBindings == 16,
                  "apply_function_value currently requires "
                  "closure::env<Core,16> (value<Core>'s embedded closure "
                  "alternative is closure<Core,16>, unconditionally)");
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    using Val = value<Core>;
    using Vals = value_list<Core, MaxValues>;
    using Res = smd::smdscheme::foundation::result<Vals>;
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
                    return single_value<MaxValues>(Val{acc});
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
                        detail::to_list_op(bi.op), args, heap);
                    if (!pr.has_value())
                        return Res{pr.error()};
                    return single_value<MaxValues>(pr.value());
                }
                case builtin_op::values: {
                    // ANSI CL: `values` is an ordinary function that returns
                    // every argument it was given, as that many values. This
                    // is the *only* builtin whose result is n-ary, and the
                    // only reason this whole function returns a value_list
                    // rather than a value.
                    if (static_cast<int>(args.size()) > MaxValues)
                        return parse_error{
                            {}, "values: more values than MaxValues"};
                    Vals vs{};
                    for (auto const &a : args)
                        vs.push_back(a);
                    return vs;
                }
                case builtin_op::funcall: {
                    // (funcall f args...): the first argument IS the
                    // function to call; the rest are its call arguments.
                    // The callee's values pass straight through, so
                    // (multiple-value-bind (a b) (funcall #'values 1 2) ...)
                    // binds both.
                    if (args.empty())
                        return parse_error{
                            {}, "funcall: expected a function argument"};
                    return apply_function_value_values<
                        MaxNodes, MaxList, MaxBindings, MaxEnvs, MaxValues>(
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
                    return apply_function_value_values<
                        MaxNodes, MaxList, MaxBindings, MaxEnvs, MaxValues>(
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
                // A parameter naming a special variable binds DYNAMICALLY
                // (step L16); everything else binds lexically, as before.
                // `let`/`let*` lower to lambda applications, so this one
                // call is where all of them get their scoping decided.
                auto const bound_r = detail::bind_lambda_parameters(
                    lam.params, args, new_env, envs);
                if (!bound_r.has_value())
                    return Res{bound_r.error()};
                int const dynamic_count = bound_r.value();

                // Implicit progn: at least one body expression is
                // guaranteed by the elaborator (elaborate_lambda).  The loop
                // BREAKS rather than returns, so that -- exactly as in
                // core_unwind_protect below -- a value, an ordinary error, a
                // `return-from` unwind and a `throw` unwind all leave by the
                // single path that restores the dynamic bindings.
                // The LAST body form is in tail position, so it is evaluated
                // through `eval_direct_values`: a function whose body ends in
                // `(values ...)` returns those values to its caller, which is
                // what makes `(multiple-value-bind (a b) (f) ...)` work for a
                // user-defined `f`.
                Res last{parse_error{{}, "lambda: empty body"}};
                for (int i = 0; i < lam.body.size(); ++i) {
                    last = eval_direct_values<MaxNodes, MaxList, MaxBindings,
                                              MaxEnvs, MaxValues>(
                        arena.get(lam.body[i]), arena, new_env, envs);
                    if (!last.has_value())
                        break;
                }
                detail::unwind_dynamic_bindings(new_env, envs, dynamic_count);
                return last;
            },
            [&](foreign_function<Core> const &ff) -> Res {
                // A foreign function's C++ signature returns one value, so a
                // foreign callee can never produce multiple values.
                auto ff_r = ff.fn(args);
                if (!ff_r.has_value())
                    return Res{ff_r.error()};
                return single_value<MaxValues>(ff_r.value());
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

/// Single-value adapter over @ref apply_function_value_values: applies the
/// callee and keeps only the primary value.
///
/// This is the pre-L20 signature, unchanged, and it is what
/// `macroexpand/expander.hpp` (which invokes a macro's expander function and
/// wants one datum back) still calls.
///
/// @copydetails apply_function_value_values
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
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    auto r =
        apply_function_value_values<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            func_val, args, arena, heap, envs);
    if (!r.has_value())
        return smd::smdscheme::foundation::result<value<Core>>{r.error()};
    return primary_value(r.value());
}

/// Directly evaluates a core AST node in the given environment, returning
/// **all** the values it produced.
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
///  - **The return channel is n-ary (step L20).** Only the arms below that
///    are in *tail position* recurse through this function; everything else
///    (an `if` condition, an application's function and arguments, a `setq`
///    right-hand side, a `defvar` initializer, an `unwind-protect` cleanup,
///    a `throw` tag) recurses through the single-value @ref eval_direct
///    adapter, which is exactly ANSI CL's rule that those are single-value
///    contexts.
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
/// @tparam MaxValues   Maximum number of values one form may return; see
///                      @ref value_list.
/// @return The evaluated @ref value_list on success, or a
///         @ref smd::smdscheme::foundation::parse_error on type error,
///         arity mismatch, or unbound variable/undefined function.
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs,
          int MaxValues>
[[nodiscard]] constexpr auto eval_direct_values(
    elaborator::core_type<MaxNodes, MaxList> const &node,
    smd::smdscheme::foundation::tree_arena<
        elaborator::core_type<MaxNodes, MaxList>, MaxNodes> const &arena,
    env<elaborator::core_type<MaxNodes, MaxList>, MaxBindings> &environment,
    env_arena<elaborator::core_type<MaxNodes, MaxList>, MaxBindings, MaxEnvs>
        &envs)
    -> smd::smdscheme::foundation::result<
        value_list<elaborator::core_type<MaxNodes, MaxList>, MaxValues>> {
    static_assert(MaxBindings == 16,
                  "eval_direct currently requires closure::env<Core,16> "
                  "(see the doc comment above)");
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    using Val = value<Core>;
    using Vals = value_list<Core, MaxValues>;
    using Res = smd::smdscheme::foundation::result<Vals>;
    using smd::smdscheme::foundation::parse_error;

    return std::visit(
        smd::fixpoint::overloaded{
            [&](elaborator::core_integer const &ci) -> Res {
                return single_value<MaxValues>(Val{ci.value});
            },
            // bd5cf19a-ccb0-45a1-b208-b76970bcb0c6
            [&](elaborator::core_symbol const &cs) -> Res {
                // VARIABLE namespace only, per D4 -- never lookup_function.
                auto lr = environment.lookup_value(symbol{cs.name.view()});
                if (!lr.has_value())
                    return Res{lr.error()};
                return single_value<MaxValues>(lr.value());
            },
            [&](elaborator::core_keyword const &ck) -> Res {
                return single_value<MaxValues>(Val{keyword{ck.name.view()}});
            },
            [&](elaborator::core_nil const &) -> Res {
                return single_value<MaxValues>(Val{nil_t{}});
            },
            [&](elaborator::core_true const &) -> Res {
                return single_value<MaxValues>(Val{symbol{"T"}});
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
                            auto lr = environment.lookup_function(symbol{name});
                            if (!lr.has_value())
                                return Res{lr.error()};
                            return single_value<MaxValues>(lr.value());
                        },
                        [&](smd::smdscheme::foundation::arena_box<
                            Core, MaxNodes> const &target) -> Res {
                            // An embedded (lambda ...): evaluating it
                            // materializes the closure directly, no
                            // lookup.  Always exactly one value.
                            auto cr = eval_direct<MaxNodes, MaxList,
                                                  MaxBindings, MaxEnvs>(
                                arena.get(target), arena, environment, envs);
                            if (!cr.has_value())
                                return Res{cr.error()};
                            return single_value<MaxValues>(cr.value());
                        }},
                    cf.target);
            },
            // bd5cf19a-ccb0-45a1-b208-b76970bcb0c6 end
            [&](elaborator::core_quote const &cq) -> Res {
                return single_value<MaxValues>(std::visit(
                    smd::fixpoint::overloaded{
                        [](int i) -> Val { return Val{i}; },
                        [](elaborator::core_symbol const &sym) -> Val {
                            return Val{symbol{sym.name.view()}};
                        },
                        [](elaborator::core_keyword const &kw) -> Val {
                            return Val{keyword{kw.name.view()}};
                        }},
                    cq.atom));
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
                auto pr = apply_prim<Core, default_max_pairs>(
                    list_op::cons, std::span<Val const>{cons_args},
                    environment.pairs());
                if (!pr.has_value())
                    return Res{pr.error()};
                return single_value<MaxValues>(pr.value());
            },
            // 6332b396-3733-4eb0-adc5-af7a57adb809
            [&](elaborator::core_if<Core, MaxNodes> const &cif) -> Res {
                auto cond_r =
                    eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                        arena.get(cif.condition), arena, environment, envs);
                if (!cond_r.has_value())
                    return cond_r.error();
                // Both branches are in tail position, so both propagate the
                // whole value list outward (ANSI CL).
                if (is_true(cond_r.value()))
                    return eval_direct_values<MaxNodes, MaxList, MaxBindings,
                                              MaxEnvs, MaxValues>(
                        arena.get(cif.consequent), arena, environment, envs);
                return eval_direct_values<MaxNodes, MaxList, MaxBindings,
                                          MaxEnvs, MaxValues>(
                    arena.get(cif.alternative), arena, environment, envs);
            },
            // 6332b396-3733-4eb0-adc5-af7a57adb809 end
            [&](elaborator::core_progn<Core, MaxNodes, MaxList> const &cp)
                -> Res {
                // Only the last form is in tail position, but evaluating
                // every form n-arily is equivalent: the earlier results are
                // discarded whether they are one value or five.
                Res last{parse_error{{}, "progn: empty"}};
                for (int i = 0; i < cp.exprs.size(); ++i) {
                    last = eval_direct_values<MaxNodes, MaxList, MaxBindings,
                                              MaxEnvs, MaxValues>(
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
                return single_value<MaxValues>(
                    Val{closure<Core>{&node, captured}});
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
                return apply_function_value_values<
                    MaxNodes, MaxList, MaxBindings, MaxEnvs, MaxValues>(
                    func_r.value(),
                    std::span<Val const>(evaluated_args.begin(),
                                         evaluated_args.end()),
                    arena, environment.pairs(), envs);
            },
            // 152148ae-5a16-4a5b-9f88-258efb2bc5c7
            [&](elaborator::core_tagbody<Core, MaxNodes, MaxList> const &tb)
                -> Res {
                // One fresh, live record per dynamic activation -- allocated
                // exactly as core_block allocates its own, and installed into
                // the *ambient* environment for the identical reason (a
                // sibling setq/defun elsewhere in the enclosing sequence must
                // still mutate the one object every statement shares).
                auto *rec = envs.alloc_exit(symbol{"TAGBODY"});
                for (int i = 0; i < tb.labels.size(); ++i)
                    environment.define_tag(
                        detail::tag_key<Core>(tb.labels[i].tag), rec,
                        tb.labels[i].index);

                // The trampoline over the basic blocks the elaborator already
                // laid out (core_tagbody's docs). `pc` is the statement about
                // to run; running off the end of one block falls into the
                // next, and a `go` chooses a different one. Every statement
                // is an ordinary single-value context because its value is
                // discarded.
                int pc = 0;
                while (pc < tb.statements.size()) {
                    auto r =
                        eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                            arena.get(tb.statements[pc]), arena, environment,
                            envs);
                    if (r.has_value()) {
                        ++pc;
                        continue;
                    }
                    if (r.error().message == go_unwind_marker && !rec->live) {
                        // The transfer that just propagated up is aimed at
                        // THIS activation: resume at the requested block and
                        // re-arm the record, because -- unlike `return-from`
                        // -- a `go` does not end its target's extent (see
                        // env.hpp's go_unwind_marker docs).
                        if (!std::holds_alternative<int>(rec->payload))
                            return parse_error{
                                {},
                                "internal error: go target is not a block "
                                "index"};
                        pc = std::get<int>(rec->payload);
                        rec->live = true;
                        continue;
                    }
                    // An ordinary error, a `return-from`/`throw` unwind, or a
                    // `go` aimed at an enclosing tagbody: this activation is
                    // ending non-locally either way.
                    rec->live = false;
                    return Res{r.error()};
                }
                // Falling off the end ends the extent too (D5).
                rec->live = false;
                // ANSI CL: `tagbody` always evaluates to nil.
                return single_value<MaxValues>(Val{nil_t{}});
            },
            [&](elaborator::core_go const &cg) -> Res {
                auto target_r =
                    environment.lookup_tag(detail::tag_key<Core>(cg.tag));
                if (!target_r.has_value())
                    return Res{target_r.error()};
                auto const target = target_r.value();
                // The extent check. The tag's `tagbody` may have finished long
                // ago, with this `go` carried out of it inside a closure --
                // diagnosed, never UB (D5), by the same flag and the same test
                // `return-from` applies to a dead block.
                if (!target.record->live)
                    return parse_error{{}, "go: tagbody has already exited"};
                target.record->payload = Val{target.index};
                target.record->live = false;
                return parse_error{{}, go_unwind_marker};
            },
            // 152148ae-5a16-4a5b-9f88-258efb2bc5c7 end
            // e6a2c1de-3b2a-4c8d-9c6f-1a7e5b9d2f30
            [&](elaborator::core_setq<Core, MaxNodes, MaxList> const &sq)
                -> Res {
                // ANSI CL: assign each name/value pair left to right,
                // yielding the value of the LAST assignment -- never
                // Scheme's `unspecified` (see this function's own doc
                // comment / handoff for the return-type rationale).
                // A `setq` right-hand side is a single-value context.
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
                    last = single_value<MaxValues>(set_r.value());
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
                return single_value<MaxValues>(Val{symbol{cd.name}});
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
                return single_value<MaxValues>(Val{symbol{dv.name}});
            },
            [&](elaborator::core_block<Core, MaxNodes, MaxList> const &cb)
                -> Res {
                // One fresh, live exit record per dynamic activation (see
                // env.hpp's exit_record docs) -- installed directly into
                // the *ambient* `environment` (never a copy) so a sibling
                // setq/defun/defvar elsewhere in this same body sequence
                // still mutates the one object every statement shares,
                // exactly as core_progn already relies on.
                auto *rec = envs.alloc_exit(symbol{cb.name});
                environment.define_block(symbol{cb.name}, rec);

                Res last{parse_error{{}, "block: empty body"}};
                for (int i = 0; i < cb.body.size(); ++i) {
                    last = eval_direct_values<MaxNodes, MaxList, MaxBindings,
                                              MaxEnvs, MaxValues>(
                        arena.get(cb.body[i]), arena, environment, envs);
                    if (!last.has_value()) {
                        if (last.error().message == block_unwind_marker &&
                            !rec->live) {
                            // The unwind that just propagated up killed
                            // THIS record: it targets this activation.
                            // An exit_record payload is a single value: see
                            // DIV-0019 for why `return-from`/`throw` do not
                            // carry multiple values.
                            return single_value<MaxValues>(rec->payload);
                        }
                        // Either an ordinary error, or an unwind aimed at
                        // some other (enclosing) block: this activation's
                        // extent is ending non-locally either way.
                        rec->live = false;
                        return last;
                    }
                }
                // Falling off the end also ends the extent (D5).
                rec->live = false;
                return last;
            },
            [&](elaborator::core_return_from<Core, MaxNodes> const &rf) -> Res {
                auto val_r =
                    eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                        arena.get(rf.expr), arena, environment, envs);
                if (!val_r.has_value())
                    return val_r.error();
                auto rec_r = environment.lookup_block(symbol{rf.name});
                if (!rec_r.has_value())
                    return rec_r.error();
                auto *rec = rec_r.value();
                if (!rec->live)
                    return parse_error{{},
                                       "return-from: block has already exited"};
                rec->payload = val_r.value();
                rec->live = false;
                return parse_error{{}, block_unwind_marker};
            },
            [&](elaborator::core_catch<Core, MaxNodes, MaxList> const &cc)
                -> Res {
                // The dynamic counterpart of core_block above. The tag is an
                // ordinary expression evaluated once, on entry; the frame it
                // pushes lives on env_arena's catch stack (never in the
                // environment), because `catch` is found by evaluated tag
                // value at run time, not by name at elaboration time -- see
                // env.hpp's catch_record docs for the full contrast.
                auto tag_r =
                    eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                        arena.get(cc.tag), arena, environment, envs);
                if (!tag_r.has_value())
                    return tag_r.error();

                auto *rec = envs.push_catch(tag_r.value());
                Res last{parse_error{{}, "catch: empty body"}};
                for (int i = 0; i < cc.body.size(); ++i) {
                    last = eval_direct_values<MaxNodes, MaxList, MaxBindings,
                                              MaxEnvs, MaxValues>(
                        arena.get(cc.body[i]), arena, environment, envs);
                    if (!last.has_value()) {
                        if (last.error().message == throw_unwind_marker &&
                            !rec->live) {
                            // The throw that just propagated up killed THIS
                            // frame: it targets this activation.
                            Res const caught{
                                single_value<MaxValues>(rec->payload)};
                            envs.pop_catch();
                            return caught;
                        }
                        // An ordinary error, a `return-from` unwind, or a
                        // `throw` aimed at an outer frame: this activation's
                        // extent is ending non-locally either way, and the
                        // stack must be popped on that path too.
                        rec->live = false;
                        envs.pop_catch();
                        return last;
                    }
                }
                // Falling off the end also ends the extent (D5).
                rec->live = false;
                envs.pop_catch();
                return last;
            },
            [&](elaborator::core_throw<Core, MaxNodes> const &ct) -> Res {
                auto tag_r =
                    eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                        arena.get(ct.tag), arena, environment, envs);
                if (!tag_r.has_value())
                    return tag_r.error();
                auto res_r =
                    eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                        arena.get(ct.result), arena, environment, envs);
                if (!res_r.has_value())
                    return res_r.error();
                auto *rec = envs.find_catch(tag_r.value());
                if (rec == nullptr)
                    // Uncaught throw: a diagnosed error, never UB (D5).
                    return parse_error{{}, "throw: no catch for tag"};
                rec->payload = res_r.value();
                rec->live = false;
                return parse_error{{}, throw_unwind_marker};
            },
            [&](elaborator::core_unwind_protect<Core, MaxNodes, MaxList> const
                    &up) -> Res {
                // Every exit path out of the protected form -- a value, an
                // ordinary error, a `return-from` unwind, a `throw` unwind --
                // arrives here as one `Res`, because all four already travel
                // through the same result channel (env.hpp's marker docs).
                // That is what makes "run the cleanups on every path" a
                // single unconditional loop rather than four cases.
                // ANSI CL: `unwind-protect` returns the protected form's
                // values, all of them, so the protected form is evaluated
                // n-arily.  The cleanups are single-value contexts whose
                // values are discarded anyway.
                Res protected_r =
                    eval_direct_values<MaxNodes, MaxList, MaxBindings, MaxEnvs,
                                       MaxValues>(arena.get(up.protected_form),
                                                  arena, environment, envs);
                for (int i = 0; i < up.cleanup.size(); ++i) {
                    auto cl_r =
                        eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                            arena.get(up.cleanup[i]), arena, environment, envs);
                    // ANSI CL: a cleanup form that itself exits non-locally
                    // (or errors) supersedes whatever was already in flight.
                    if (!cl_r.has_value())
                        return Res{cl_r.error()};
                }
                // Cleanup values are discarded; the protected form's result
                // (values or in-flight unwind) is what propagates.
                return protected_r;
            },
            [&](elaborator::core_multiple_value_bind<Core, MaxNodes> const &mvb)
                -> Res {
                // The one genuinely n-ary *consumer* in the language: the
                // values form is the sole multiple-value context, and every
                // other arm above exists to make sure the values reach it.
                auto vals_r = eval_direct_values<MaxNodes, MaxList, MaxBindings,
                                                 MaxEnvs, MaxValues>(
                    arena.get(mvb.values_form), arena, environment, envs);
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

                // Materializing the lambda is what captures the ambient
                // environment; calling it is what binds -- including
                // dynamically, for any name a `defvar` has marked special
                // (step L16).  Neither rule is restated here.
                auto clo_r =
                    eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                        lam_node, arena, environment, envs);
                if (!clo_r.has_value())
                    return Res{clo_r.error()};

                // ANSI CL: missing values default to nil, surplus values are
                // discarded.  The arity the callee then sees is exact.
                smd::smdscheme::foundation::static_vector<Val, MaxList> bound;
                for (int i = 0; i < lam.params.size(); ++i)
                    bound.push_back(i < vals_r.value().size()
                                        ? vals_r.value()[i]
                                        : Val{nil_t{}});

                return apply_function_value_values<
                    MaxNodes, MaxList, MaxBindings, MaxEnvs, MaxValues>(
                    clo_r.value(),
                    std::span<Val const>(bound.begin(), bound.end()), arena,
                    environment.pairs(), envs);
            }},
        // e6a2c1de-3b2a-4c8d-9c6f-1a7e5b9d2f30 end
        node.inner);
}

/// Single-value adapter over @ref eval_direct_values: evaluates @p node and
/// keeps only the primary value.
///
/// This is the pre-L20 signature, unchanged.  It is both the public
/// single-value entry point and the way every single-value *context* inside
/// @ref eval_direct_values evaluates its subexpressions, so ANSI CL's
/// "single-value context takes the primary value, or `nil` if there were
/// none" rule is enforced in exactly one place: @ref primary_value.
///
/// @copydetails eval_direct_values
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
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    auto r = eval_direct_values<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
        node, arena, environment, envs);
    if (!r.has_value())
        return smd::smdscheme::foundation::result<value<Core>>{r.error()};
    return primary_value(r.value());
}

} // namespace smd::smdlisp::closure

#endif
