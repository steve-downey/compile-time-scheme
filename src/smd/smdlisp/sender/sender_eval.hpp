// src/smd/smdlisp/sender/sender_eval.hpp                         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_SENDER_SENDER_EVAL_HPP
#define SRC_SMD_SMDLISP_SENDER_SENDER_EVAL_HPP

#include <smd/smdlisp/closure/env.hpp>
#include <smd/smdlisp/closure/eval_direct.hpp>
#include <smd/smdlisp/closure/pairs.hpp>
#include <smd/smdlisp/closure/value.hpp>
#include <smd/smdlisp/elaborator/elaborated_core.hpp>
#include <smd/smdlisp/sender/defer_outcome.hpp>
#include <smd/smdlisp/sender/outcome.hpp>
#include <smd/smdlisp/sender/sender_v.hpp>
#include <smd/smdlisp/sender/unwind_protect.hpp>
#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/foundation/static_vector.hpp>

#include <smd/fixpoint/overloaded.hpp>

#include <cstddef>
#include <span>
#include <string_view>
#include <utility>
#include <variant>

namespace smd::smdlisp::sender {

// 59265482-a0f4-4b96-9820-89abdc54f268
namespace detail {

/// Mendler-style paramorphism over a @ref smd::smdscheme::foundation::fix
/// tree.
///
/// This is @ref smd::fixpoint::mendler_para, re-derived for the one type it
/// cannot be applied to.  The Scheme sender backend reaches
/// `smd::fixpoint::mendler_para` because its computation tree is a
/// `smd::fixpoint::Fix`; the `smdlisp` core AST is a
/// `smd::smdscheme::foundation::fix`, which is structurally the same functor
/// fixed point but a nominally distinct type, and `smd::smdscheme` is frozen
/// for semantic changes (decision D1) so `foundation::fix` cannot grow a
/// conversion to `Fix`.  See DIV-0016 for the full reasoning and for why the
/// bridge lives here rather than in `smd/fixpoint` (outside this step's lane)
/// or in `smdscheme/foundation` (frozen).
///
/// Two deliberate differences from the original, neither semantic:
///
///  - it is parameterised on the concrete fixed-point type @p Fixed rather
///    than on `fix<F>`, because deducing a template template argument that is
///    a dependent alias template (which `core_f_factory<...>::type` is) is
///    exactly the kind of deduction that does not survive contact with a real
///    compiler;
///  - it reads the layer as @c tree.inner rather than through @c unwrap_fix,
///    because `foundation::fix` exposes its layer directly and has no
///    unwrapping free function.
///
/// Mendler style means the algebra never recurses by naming this function: it
/// is *handed* a @p recurse it can call on children, which is what lets the
/// algebra stay a plain stateless struct while the recursion strategy stays
/// here.  Paramorphic (rather than catamorphic) because the algebra also
/// receives the whole @p tree node alongside its unwrapped layer -- `smdlisp`
/// needs that: evaluating `core_lambda` has to store a pointer to the node
/// itself in the resulting closure.
///
/// @tparam Result  The carrier type the algebra returns.
/// @tparam Fixed   The concrete fixed-point tree type.
/// @tparam Ctx     The context threaded through the recursion.
/// @tparam Algebra Callable as @c alg(recurse, ctx, tree, tree.inner).
template <typename Result, typename Fixed, typename Ctx, typename Algebra>
auto mendler_para(Algebra const &alg, Ctx const &ctx, Fixed const &tree)
    -> Result {
    auto recurse = [&alg](Fixed const &child, Ctx const &c) -> Result {
        return mendler_para<Result, Fixed>(alg, c, child);
    };
    return alg(recurse, ctx, tree, tree.inner);
}

} // namespace detail

/// Everything the algebra needs that is not the node it is looking at.
///
/// Held as pointers rather than references so the context is copyable, which
/// @ref detail::mendler_para's `Ctx const &` threading wants, and so that
/// @ref apply_function_value can build a *new* context naming the callee's
/// environment without disturbing the caller's.
///
/// @tparam MaxNodes    Arena capacity.
/// @tparam MaxList     Maximum argument/body/list length.
/// @tparam MaxBindings Environment capacity; must be 16.
/// @tparam MaxEnvs     Capacity of the closure-capture env_arena.
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs>
struct eval_context {
    using Core = elaborator::core_type<MaxNodes, MaxList>;

    smd::smdscheme::foundation::tree_arena<Core, MaxNodes> const
        *arena;                                   ///< Owns every sub-node.
    closure::env<Core, MaxBindings> *environment; ///< Mutated in place by
                                                  ///< `setq`/`defun`/`defvar`.
    closure::env_arena<Core, MaxBindings, MaxEnvs>
        *envs; ///< Captured environments, exit records, catch frames and
               ///< dynamic bindings.
};
// 59265482-a0f4-4b96-9820-89abdc54f268 end

/// Forward declaration: @ref eval_node and @ref apply_function_value recurse
/// into each other, exactly as their `closure` counterparts do.
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs>
[[nodiscard]] auto
eval_node(elaborator::core_type<MaxNodes, MaxList> const &node,
          eval_context<MaxNodes, MaxList, MaxBindings, MaxEnvs> const &ctx)
    -> outcome<elaborator::core_type<MaxNodes, MaxList>>;

/// Applies an already-evaluated function @ref closure::value to
/// already-evaluated arguments, evaluating any lambda body through the
/// sender backend.
///
/// A near-transcription of @ref closure::apply_function_value, with two
/// changes and no third:
///
///  - the carrier is @ref outcome rather than @c result, so a `return-from`
///    or `throw` raised inside the callee arrives here as
///    @c channel::stopped rather than as a sentinel-carrying error;
///  - the body loop's `break` condition is therefore `!last.is_value()`
///    rather than `!last.has_value()`.
///
/// Everything else -- including the dynamic-binding discipline (step L16) --
/// is *called*, not re-implemented: @ref
/// closure::detail::bind_lambda_parameters and @ref
/// closure::detail::unwind_dynamic_bindings are templates on the environment
/// and arena types with no dependency on `eval_direct` itself, so the sender
/// backend gets special-variable binding by using the same two functions the
/// other two backends use.  That is deliberate; a second implementation of
/// shallow binding is a second place for it to be wrong.
///
/// The loop still **breaks rather than returns**, for the reason its
/// counterpart does: a value, an error and an unwind then all leave through
/// the single statement that restores the dynamic bindings.  Note that this
/// is the one place the sender backend still needs the closure backends'
/// only-one-path trick, because the restore has to happen in ordinary C++
/// control flow around a recursive call rather than inside a receiver.
///
/// @tparam MaxNodes    Arena capacity.
/// @tparam MaxList     Maximum argument/body/list length.
/// @tparam MaxBindings Environment capacity; must be 16.
/// @tparam MaxEnvs     Capacity of the closure-capture env_arena.
/// @param  func_val    The callee.
/// @param  args        The already-evaluated call arguments.
/// @param  ctx         The caller's evaluation context.
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs>
[[nodiscard]] auto apply_function_value(
    closure::value<elaborator::core_type<MaxNodes, MaxList>> const &func_val,
    std::span<closure::value<elaborator::core_type<MaxNodes, MaxList>> const>
        args,
    eval_context<MaxNodes, MaxList, MaxBindings, MaxEnvs> const &ctx)
    -> outcome<elaborator::core_type<MaxNodes, MaxList>> {
    static_assert(MaxBindings == 16,
                  "apply_function_value currently requires "
                  "closure::env<Core,16> (value<Core>'s embedded closure "
                  "alternative is closure<Core,16>, unconditionally)");
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    using Val = closure::value<Core>;
    using Out = outcome<Core>;
    using smd::smdscheme::foundation::parse_error;

    return std::visit(
        smd::fixpoint::overloaded{
            [&](closure::builtin const &bi) -> Out {
                switch (bi.op) {
                case closure::builtin_op::add:
                case closure::builtin_op::multiply: {
                    // ANSI CL `+`/`*` are variadic, `(+)` => 0, `(*)` => 1.
                    int acc = bi.op == closure::builtin_op::add ? 0 : 1;
                    for (auto const &a : args) {
                        if (!std::holds_alternative<int>(a))
                            return make_error<Core>(
                                parse_error{{}, "type error"});
                        acc = bi.op == closure::builtin_op::add
                                  ? acc + std::get<int>(a)
                                  : acc * std::get<int>(a);
                    }
                    return make_value<Core>(Val{acc});
                }
                case closure::builtin_op::cons:
                case closure::builtin_op::car:
                case closure::builtin_op::cdr:
                case closure::builtin_op::list:
                case closure::builtin_op::null:
                case closure::builtin_op::eq:
                case closure::builtin_op::eql:
                case closure::builtin_op::atom:
                case closure::builtin_op::append:
                    return from_result<Core>(
                        closure::apply_prim<Core, closure::default_max_pairs>(
                            closure::detail::to_list_op(bi.op), args,
                            ctx.environment->pairs()));
                case closure::builtin_op::values:
                    // Step L20 made `values` an ordinary builtin whose
                    // result is n-ary.  This backend's carrier -- `outcome`,
                    // and behind it `lisp_completions`' `set_value_t(value)`
                    // -- holds exactly one value, so there is nowhere to put
                    // the rest.  Diagnosed rather than silently truncated;
                    // see DIV-0018.
                    return make_error<Core>(parse_error{
                        {},
                        "values: multiple values are not supported by the "
                        "sender backend"});
                case closure::builtin_op::funcall: {
                    if (args.empty())
                        return make_error<Core>(parse_error{
                            {}, "funcall: expected a function argument"});
                    return apply_function_value<MaxNodes, MaxList, MaxBindings,
                                                MaxEnvs>(args[0],
                                                         args.subspan(1), ctx);
                }
                case closure::builtin_op::apply: {
                    if (args.size() < 2)
                        return make_error<Core>(parse_error{
                            {},
                            "apply: expected a function and at least one "
                            "list argument"});
                    smd::smdscheme::foundation::static_vector<Val, MaxList>
                        spread;
                    for (std::size_t i = 1; i + 1 < args.size(); ++i)
                        spread.push_back(args[i]);
                    Val cur = args[args.size() - 1];
                    auto *heap = ctx.environment->pairs();
                    while (!std::holds_alternative<closure::nil_t>(cur)) {
                        if (!std::holds_alternative<closure::pair_ref>(cur))
                            return make_error<Core>(parse_error{
                                {}, "apply: last argument must be a list"});
                        if (heap == nullptr)
                            return make_error<Core>(parse_error{
                                {}, "apply: environment has no pair heap"});
                        auto const &cell =
                            heap->get(std::get<closure::pair_ref>(cur).loc);
                        spread.push_back(cell.car);
                        cur = cell.cdr;
                    }
                    return apply_function_value<MaxNodes, MaxList, MaxBindings,
                                                MaxEnvs>(
                        args[0],
                        std::span<Val const>(spread.begin(), spread.end()),
                        ctx);
                }
                }
                return make_error<Core>(parse_error{{}, "unknown builtin"});
            },
            [&](closure::closure<Core> const &clo) -> Out {
                if (clo.node == nullptr)
                    return make_error<Core>(parse_error{
                        {}, "internal error: closure has no lambda node"});
                auto const &lam_node = *clo.node;
                if (!std::holds_alternative<
                        elaborator::core_lambda<Core, MaxNodes, MaxList>>(
                        lam_node.inner))
                    return make_error<Core>(parse_error{
                        {},
                        "internal error: closure does not reference a "
                        "lambda"});
                auto const &lam =
                    std::get<elaborator::core_lambda<Core, MaxNodes, MaxList>>(
                        lam_node.inner);
                if (static_cast<int>(args.size()) != lam.params.size())
                    return make_error<Core>(parse_error{{}, "arity mismatch"});
                if (clo.captured == nullptr)
                    return make_error<Core>(
                        parse_error{{},
                                    "internal error: closure has no captured "
                                    "environment"});

                closure::env<Core, MaxBindings> new_env = *clo.captured;
                auto const bound_r = closure::detail::bind_lambda_parameters(
                    lam.params, args, new_env, *ctx.envs);
                if (!bound_r.has_value())
                    return make_error<Core>(bound_r.error());
                int const dynamic_count = bound_r.value();

                eval_context<MaxNodes, MaxList, MaxBindings, MaxEnvs> const
                    callee_ctx{ctx.arena, &new_env, ctx.envs};

                Out last =
                    make_error<Core>(parse_error{{}, "lambda: empty body"});
                for (int i = 0; i < lam.body.size(); ++i) {
                    last = eval_node<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                        ctx.arena->get(lam.body[i]), callee_ctx);
                    if (!last.is_value())
                        break;
                }
                closure::detail::unwind_dynamic_bindings(new_env, *ctx.envs,
                                                         dynamic_count);
                return last;
            },
            [&](closure::foreign_function<Core> const &ff) -> Out {
                return from_result<Core>(ff.fn(args));
            },
            [](closure::nil_t const &) -> Out {
                return make_error<Core>(
                    parse_error{{}, "attempted to call non-function"});
            },
            [](int const &) -> Out {
                return make_error<Core>(
                    parse_error{{}, "attempted to call non-function"});
            },
            [](closure::symbol const &) -> Out {
                return make_error<Core>(
                    parse_error{{}, "attempted to call non-function"});
            },
            [](closure::keyword const &) -> Out {
                return make_error<Core>(
                    parse_error{{}, "attempted to call non-function"});
            },
            [](closure::pair_ref const &) -> Out {
                return make_error<Core>(
                    parse_error{{}, "attempted to call non-function"});
            }},
        func_val);
}

/// The Mendler algebra that evaluates one core node over senders.
///
/// Stateless: everything it needs arrives as @p ctx, and it recurses only
/// through the @p recurse it is handed.
///
/// @tparam MaxNodes    Arena capacity.
/// @tparam MaxList     Maximum argument/body/list length.
/// @tparam MaxBindings Environment capacity; must be 16.
/// @tparam MaxEnvs     Capacity of the closure-capture env_arena.
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs>
struct eval_algebra {
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    using Val = closure::value<Core>;
    using Out = outcome<Core>;
    using Ctx = eval_context<MaxNodes, MaxList, MaxBindings, MaxEnvs>;

    /// Evaluates @p layer, the unwrapped outermost functor layer of @p node.
    ///
    /// @param recurse Evaluates a child node under a context.
    /// @param ctx     The current context.
    /// @param node    The whole node (paramorphic: `core_lambda` needs it).
    /// @param layer   @p node's variant layer.
    template <typename Recurse, typename Layer>
    auto operator()(Recurse const &recurse, Ctx const &ctx, Core const &node,
                    Layer const &layer) const -> Out {
        using smd::smdscheme::foundation::parse_error;

        /// Evaluates the child behind arena handle @p h.
        auto child = [&](auto const &h) -> Out {
            return recurse(ctx.arena->get(h), ctx);
        };

        return std::visit(
            smd::fixpoint::overloaded{
                [&](elaborator::core_integer const &ci) -> Out {
                    return make_value<Core>(Val{ci.value});
                },
                [&](elaborator::core_symbol const &cs) -> Out {
                    // VARIABLE namespace only, per D4.
                    return from_result<Core>(ctx.environment->lookup_value(
                        closure::symbol{cs.name.view()}));
                },
                [&](elaborator::core_keyword const &ck) -> Out {
                    return make_value<Core>(
                        Val{closure::keyword{ck.name.view()}});
                },
                [&](elaborator::core_nil const &) -> Out {
                    return make_value<Core>(Val{closure::nil_t{}});
                },
                [&](elaborator::core_true const &) -> Out {
                    return make_value<Core>(Val{closure::symbol{"T"}});
                },
                [&](elaborator::core_quote const &cq) -> Out {
                    return make_value<Core>(std::visit(
                        smd::fixpoint::overloaded{
                            [](int i) -> Val { return Val{i}; },
                            [](elaborator::core_symbol const &sym) -> Val {
                                return Val{closure::symbol{sym.name.view()}};
                            },
                            [](elaborator::core_keyword const &kw) -> Val {
                                return Val{closure::keyword{kw.name.view()}};
                            }},
                        cq.atom));
                },
                // 44233d7a-ff4b-4c3d-b5ff-006c7ee0fd1d
                [&](elaborator::core_cons<Core, MaxNodes> const &cc) -> Out {
                    // The one node whose two children are structurally
                    // independent, and so the one place `when_all` says
                    // something true.  The two evaluations still happen in
                    // source order, *before* the graph is built, because CL
                    // fixes left-to-right evaluation order and a `when_all`
                    // over genuinely deferred children would start the second
                    // even when the first has already unwound.  What the
                    // `when_all` expresses is that the two *results* are
                    // combined by a join, not a sequence -- the same shape,
                    // and the same honesty about it, as
                    // `smdscheme/sender/sender_program.hpp`'s builtin case.
                    Out car_o = child(cc.car);
                    if (!car_o.is_value())
                        return car_o;
                    Out cdr_o = child(cc.cdr);
                    if (!cdr_o.is_value())
                        return cdr_o;

                    auto joined = sender_v::sync_wait(sender_v::then(
                        sender_v::when_all(sender_v::just(car_o.val),
                                           sender_v::just(cdr_o.val)),
                        [&ctx](Val a, Val b) -> Out {
                            Val const cons_args[2] = {a, b};
                            return from_result<Core>(
                                closure::apply_prim<Core,
                                                    closure::default_max_pairs>(
                                    closure::list_op::cons,
                                    std::span<Val const>{cons_args},
                                    ctx.environment->pairs()));
                        }));
                    return std::get<0>(joined.value());
                },
                // 44233d7a-ff4b-4c3d-b5ff-006c7ee0fd1d end
                [&](elaborator::core_if<Core, MaxNodes> const &cif) -> Out {
                    Out cond_o = child(cif.condition);
                    if (!cond_o.is_value())
                        return cond_o;
                    // D3: anything but nil is true, via the single is_true.
                    return closure::is_true(cond_o.val)
                               ? child(cif.consequent)
                               : child(cif.alternative);
                },
                [&](elaborator::core_progn<Core, MaxNodes, MaxList> const &cp)
                    -> Out {
                    Out last =
                        make_error<Core>(parse_error{{}, "progn: empty"});
                    for (int i = 0; i < cp.exprs.size(); ++i) {
                        last = child(cp.exprs[i]);
                        if (!last.is_value())
                            return last;
                    }
                    return last;
                },
                [&](elaborator::core_lambda<Core, MaxNodes, MaxList> const &)
                    -> Out {
                    // Paramorphic: the closure stores the node itself.
                    auto const *captured = ctx.envs->alloc(*ctx.environment);
                    return make_value<Core>(
                        Val{closure::closure<Core>{&node, captured}});
                },
                [&](elaborator::core_function<Core, MaxNodes> const &cf)
                    -> Out {
                    return std::visit(
                        smd::fixpoint::overloaded{
                            [&](std::string_view name) -> Out {
                                // FUNCTION namespace only, per D4.
                                return from_result<Core>(
                                    ctx.environment->lookup_function(
                                        closure::symbol{name}));
                            },
                            [&](smd::smdscheme::foundation::arena_box<
                                Core, MaxNodes> const &target) -> Out {
                                return child(target);
                            }},
                        cf.target);
                },
                [&](elaborator::core_application<Core, MaxNodes, MaxList> const
                        &app) -> Out {
                    Out func_o = child(app.func);
                    if (!func_o.is_value())
                        return func_o;

                    smd::smdscheme::foundation::static_vector<Val, MaxList>
                        evaluated_args;
                    for (int i = 0; i < app.args.size(); ++i) {
                        Out arg_o = child(app.args[i]);
                        if (!arg_o.is_value())
                            return arg_o;
                        evaluated_args.push_back(arg_o.val);
                    }
                    return apply_function_value<MaxNodes, MaxList, MaxBindings,
                                                MaxEnvs>(
                        func_o.val,
                        std::span<Val const>(evaluated_args.begin(),
                                             evaluated_args.end()),
                        ctx);
                },
                [&](elaborator::core_setq<Core, MaxNodes, MaxList> const &sq)
                    -> Out {
                    Out last = make_error<Core>(
                        parse_error{{}, "setq: no assignments"});
                    for (int i = 0; i < sq.names.size(); ++i) {
                        Out val_o = child(sq.exprs[i]);
                        if (!val_o.is_value())
                            return val_o;
                        auto set_r = ctx.environment->set_value(
                            closure::symbol{sq.names[i]}, val_o.val);
                        if (!set_r.has_value())
                            return make_error<Core>(set_r.error());
                        last = make_value<Core>(set_r.value());
                    }
                    return last;
                },
                [&](elaborator::core_defun<Core, MaxNodes> const &cd) -> Out {
                    Out clo_o = child(cd.lambda_node);
                    if (!clo_o.is_value())
                        return clo_o;
                    ctx.environment->define_function(closure::symbol{cd.name},
                                                     clo_o.val);
                    // ANSI CL: `defun` returns the function name.
                    return make_value<Core>(Val{closure::symbol{cd.name}});
                },
                [&](elaborator::core_defvar<Core, MaxNodes> const &dv) -> Out {
                    ctx.environment->mark_special(closure::symbol{dv.name});
                    if (dv.has_init) {
                        bool const already_bound =
                            ctx.environment
                                ->lookup_value(closure::symbol{dv.name})
                                .has_value();
                        if (dv.is_parameter || !already_bound) {
                            Out init_o = child(dv.init);
                            if (!init_o.is_value())
                                return init_o;
                            ctx.environment->define_value(
                                closure::symbol{dv.name}, init_o.val);
                        }
                    }
                    // ANSI CL: `defvar`/`defparameter` return the name.
                    return make_value<Core>(Val{closure::symbol{dv.name}});
                },
                // 02f32bd6-6a27-4b63-b0b8-744347bc6484
                [&](elaborator::core_block<Core, MaxNodes, MaxList> const &cb)
                    -> Out {
                    // One fresh, live exit record per dynamic activation,
                    // installed into the *ambient* environment so a sibling
                    // setq/defun elsewhere in this body still mutates the one
                    // object every statement shares.
                    auto *rec = ctx.envs->alloc_exit(closure::symbol{cb.name});
                    ctx.environment->define_block(closure::symbol{cb.name},
                                                  rec);

                    Out last =
                        make_error<Core>(parse_error{{}, "block: empty body"});
                    for (int i = 0; i < cb.body.size(); ++i) {
                        last = child(cb.body[i]);
                        if (last.is_value())
                            continue;
                        // Note what is NOT here: the closure backends compare
                        // `last.error().message` against
                        // `closure::block_unwind_marker` before testing the
                        // record, because in those backends an unwind and an
                        // ordinary error arrive on the same channel and are
                        // otherwise indistinguishable.  Here they cannot be
                        // confused -- an error arrives on the error channel
                        // and an unwind on the stopped channel -- so the
                        // sentinel comparison is deleted, and `live` alone
                        // answers the only remaining question: is this unwind
                        // aimed at *this* activation?  A `throw` passing
                        // through never touches an exit record, so it leaves
                        // `live` set and propagates untouched.
                        if (last.is_stopped() && !rec->live)
                            return make_value<Core>(rec->payload);
                        // Either an ordinary error, or an unwind aimed at an
                        // enclosing scope: this extent is ending either way.
                        rec->live = false;
                        return last;
                    }
                    // Falling off the end also ends the extent (D5).
                    rec->live = false;
                    return last;
                },
                [&](elaborator::core_return_from<Core, MaxNodes> const &rf)
                    -> Out {
                    Out val_o = child(rf.expr);
                    if (!val_o.is_value())
                        return val_o;
                    auto rec_r =
                        ctx.environment->lookup_block(closure::symbol{rf.name});
                    if (!rec_r.has_value())
                        return make_error<Core>(rec_r.error());
                    auto *rec = rec_r.value();
                    if (!rec->live)
                        // Using a dead exit is a diagnosed error, never UB
                        // (D5) -- and it stays on the *error* channel, since
                        // it is not an unwind at all.
                        return make_error<Core>(parse_error{
                            {}, "return-from: block has already exited"});
                    rec->payload = val_o.val;
                    rec->live = false;
                    // The escape itself: complete the enclosing scope's
                    // sender early, with no value.
                    return make_stopped<Core>();
                },
                [&](elaborator::core_catch<Core, MaxNodes, MaxList> const &cc)
                    -> Out {
                    // The dynamic counterpart of core_block: the tag is an
                    // ordinary expression evaluated once on entry, and the
                    // frame lives on the env_arena's catch stack rather than
                    // in the environment, because `catch` is found by
                    // evaluated tag value at run time.
                    Out tag_o = child(cc.tag);
                    if (!tag_o.is_value())
                        return tag_o;

                    auto *rec = ctx.envs->push_catch(tag_o.val);
                    Out last =
                        make_error<Core>(parse_error{{}, "catch: empty body"});
                    for (int i = 0; i < cc.body.size(); ++i) {
                        last = child(cc.body[i]);
                        if (last.is_value())
                            continue;
                        if (last.is_stopped() && !rec->live) {
                            Out const caught = make_value<Core>(rec->payload);
                            ctx.envs->pop_catch();
                            return caught;
                        }
                        rec->live = false;
                        ctx.envs->pop_catch();
                        return last;
                    }
                    rec->live = false;
                    ctx.envs->pop_catch();
                    return last;
                },
                [&](elaborator::core_throw<Core, MaxNodes> const &ct) -> Out {
                    Out tag_o = child(ct.tag);
                    if (!tag_o.is_value())
                        return tag_o;
                    Out res_o = child(ct.result);
                    if (!res_o.is_value())
                        return res_o;
                    auto *rec = ctx.envs->find_catch(tag_o.val);
                    if (rec == nullptr)
                        // D5: an uncaught throw surfaces on the ERROR
                        // channel, not the stopped one. It is not an unwind
                        // looking for a target; it is the diagnosis that no
                        // target exists.
                        return make_error<Core>(
                            parse_error{{}, "throw: no catch for tag"});
                    rec->payload = res_o.val;
                    rec->live = false;
                    return make_stopped<Core>();
                },
                // 02f32bd6-6a27-4b63-b0b8-744347bc6484 end
                // 06514953-0711-4ac8-b392-9fad1aed130e
                [&](elaborator::core_unwind_protect<Core, MaxNodes,
                                                    MaxList> const &up) -> Out {
                    // The one form in the backend that is not evaluated by
                    // this algebra at all: it is *delegated to a sender
                    // adapter*.  The protected form becomes a lazy child
                    // sender, the cleanup forms become the adapter's cleanup
                    // thunk, and which of the three channels the child
                    // completes on -- and therefore what `unwind-protect`
                    // must do about it -- is decided inside
                    // `unwind_protect_receiver`, not here.  That is decision
                    // D5's "cleanup on all three completion channels" made
                    // structural: this arm cannot forget a channel, because
                    // it does not enumerate them.
                    return run_sender<Core>(unwind_protect<Core>(
                        defer_outcome<Core>(
                            [&] { return child(up.protected_form); }),
                        [&]() -> Out {
                            for (int i = 0; i < up.cleanup.size(); ++i) {
                                Out cl_o = child(up.cleanup[i]);
                                // ANSI CL: a cleanup that itself exits
                                // non-locally supersedes what was in flight.
                                if (!cl_o.is_value())
                                    return cl_o;
                            }
                            // Cleanup values are discarded; returning a value
                            // outcome is how this thunk says "nothing to
                            // report", and the adapter then forwards the
                            // protected form's own completion unchanged.
                            return make_value<Core>(Val{closure::nil_t{}});
                        }));
                },
                // 06514953-0711-4ac8-b392-9fad1aed130e end
                [&](elaborator::core_multiple_value_bind<Core, MaxNodes> const
                        &) -> Out {
                    // Step L20's binding form, deliberately not ported.  The
                    // closure backends carry multiple values in an n-ary
                    // return channel (`closure::value_list`); this backend
                    // carries a single value per completion, so porting the
                    // form means widening `outcome`, `outcome_receiver`,
                    // `lisp_completions` and both hand-written senders.  That
                    // is real work with its own design questions, and it is
                    // out of this step's scope: see DIV-0018.
                    return make_error<Core>(parse_error{
                        {},
                        "multiple-value-bind is not supported by the sender "
                        "backend"});
                }},
            layer);
    }
};

/// Evaluates @p node over senders, returning which completion channel it
/// reached.
///
/// @tparam MaxNodes    Arena capacity.
/// @tparam MaxList     Maximum argument/body/list length.
/// @tparam MaxBindings Environment capacity; must be 16.
/// @tparam MaxEnvs     Capacity of the closure-capture env_arena.
/// @param  node        The core node to evaluate.
/// @param  ctx         Arena, environment and environment arena.
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs>
[[nodiscard]] auto
eval_node(elaborator::core_type<MaxNodes, MaxList> const &node,
          eval_context<MaxNodes, MaxList, MaxBindings, MaxEnvs> const &ctx)
    -> outcome<elaborator::core_type<MaxNodes, MaxList>> {
    static_assert(MaxBindings == 16,
                  "eval_node currently requires closure::env<Core,16>");
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    return detail::mendler_para<outcome<Core>, Core>(
        eval_algebra<MaxNodes, MaxList, MaxBindings, MaxEnvs>{}, ctx, node);
}

} // namespace smd::smdlisp::sender

#endif
