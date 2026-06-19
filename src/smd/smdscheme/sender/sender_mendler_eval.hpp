// src/smd/smdscheme/sender/sender_mendler_eval.hpp               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_SENDER_MENDLER_EVAL_HPP
#define SRC_SMD_SMDSCHEME_SENDER_SENDER_MENDLER_EVAL_HPP

#include <smd/smdscheme/sender/comp_tree.hpp>
#include <smd/smdscheme/sender/sender_v.hpp>

#include <smd/smdscheme/closure/value.hpp>
#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/foundation/static_vector.hpp>

#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/overloaded.hpp>

#include <span>
#include <variant>

namespace smd::smdscheme::sender_mendler {

namespace detail {

template <typename T>
T unwrap_sender(auto sender) {
    return std::get<0>(sender_v::sync_wait(std::move(sender)).value());
}

} // namespace detail

/// Evaluates a computation tree using sender-structured Mendler-style
/// interpretation.
///
/// Same semantics as fixpoint_eval::mendler_run, but each sub-expression
/// evaluation is expressed as a deferred sender. For comp_apply with builtins,
/// the two argument evaluations are combined via when_all, expressing their
/// structural independence. With an inline sync_wait the execution is
/// sequential; a thread-pool scheduler would run the arguments concurrently
/// without code changes.
template <int MaxList, int MaxBindings>
[[nodiscard]] auto sender_mendler_run(
    fixpoint_eval::Comp<MaxList> const &comp,
    closure::env<fixpoint_eval::Comp<MaxList>, MaxBindings> const &env)
    -> foundation::result<closure::value<fixpoint_eval::Comp<MaxList>>> {

    static_assert(
        MaxBindings == 16,
        "sender_mendler_run currently requires closure::env<Core, 16>");

    using CompT = fixpoint_eval::Comp<MaxList>;
    using Val = closure::value<CompT>;
    using Res = foundation::result<Val>;

    return std::visit(
        smd::fixpoint::overloaded{
            // comp_pure: literal atom -> value via sender
            [](fixpoint_eval::comp_pure const &p) -> Res {
                return detail::unwrap_sender<Res>(sender_v::then(
                    sender_v::just(p),
                    [](fixpoint_eval::comp_pure const &pure) -> Res {
                        return std::visit(
                            smd::fixpoint::overloaded{
                                [](int i) -> Res { return Val{i}; },
                                [](bool b) -> Res { return Val{b}; },
                                [](std::string_view sv) -> Res {
                                    return Val{closure::symbol{sv}};
                                }},
                            pure.atom);
                    }));
            },

            // comp_lookup: variable lookup via sender
            [&env](fixpoint_eval::comp_lookup const &l) -> Res {
                return detail::unwrap_sender<Res>(sender_v::then(
                    sender_v::just(l.name), [&env](std::string_view n) -> Res {
                        return env.lookup(n);
                    }));
            },

            // comp_if: lazy branch selection — evaluate condition, then only
            // the chosen branch
            [&env](fixpoint_eval::comp_if<CompT> const &ci) -> Res {
                auto cond_r = detail::unwrap_sender<Res>(
                    sender_v::then(sender_v::just(0), [&](int) -> Res {
                        return sender_mendler_run<MaxList, MaxBindings>(
                            *ci.cond, env);
                    }));

                if (!cond_r.has_value())
                    return cond_r;

                bool taken = !std::holds_alternative<bool>(cond_r.value()) ||
                             std::get<bool>(cond_r.value());

                return detail::unwrap_sender<Res>(
                    sender_v::then(sender_v::just(0), [&](int) -> Res {
                        return sender_mendler_run<MaxList, MaxBindings>(
                            taken ? *ci.cons : *ci.alt, env);
                    }));
            },

            // comp_lambda: capture env and node pointer into closure
            [&env,
             &comp](fixpoint_eval::comp_lambda<CompT, MaxList> const &) -> Res {
                return detail::unwrap_sender<Res>(
                    sender_v::just(Res{Val{closure::closure<CompT>{
                        &comp, closure::constexpr_box<closure::env<CompT, 16>>{
                                   new closure::env<CompT, 16>{env}}}}}));
            },

            // comp_apply: evaluate function, dispatch on its type
            [&env, &comp](
                fixpoint_eval::comp_apply<CompT, MaxList> const &app) -> Res {
                auto func_r = detail::unwrap_sender<Res>(
                    sender_v::then(sender_v::just(0), [&](int) -> Res {
                        return sender_mendler_run<MaxList, MaxBindings>(
                            *app.func, env);
                    }));
                if (!func_r.has_value())
                    return func_r;

                return std::visit(
                    smd::fixpoint::overloaded{
                        // Builtin: when_all for parallel arg evaluation
                        [&env, &app](closure::builtin const &bi) -> Res {
                            if (app.args.size() != 2)
                                return Res{foundation::parse_error{
                                    {}, "arity mismatch"}};

                            // Deferred senders — evaluation happens inside
                            // when_all, not before it
                            auto s0 = sender_v::then(
                                sender_v::just(0), [&](int) -> Res {
                                    return sender_mendler_run<MaxList,
                                                              MaxBindings>(
                                        *app.args[0], env);
                                });
                            auto s1 = sender_v::then(
                                sender_v::just(0), [&](int) -> Res {
                                    return sender_mendler_run<MaxList,
                                                              MaxBindings>(
                                        *app.args[1], env);
                                });

                            return detail::unwrap_sender<Res>(sender_v::then(
                                sender_v::when_all(std::move(s0),
                                                   std::move(s1)),
                                [bi](Res a0r, Res a1r) -> Res {
                                    if (!a0r.has_value())
                                        return a0r;
                                    if (!a1r.has_value())
                                        return a1r;
                                    if (!std::holds_alternative<int>(
                                            a0r.value()) ||
                                        !std::holds_alternative<int>(
                                            a1r.value()))
                                        return Res{foundation::parse_error{
                                            {}, "type error"}};
                                    int a = std::get<int>(a0r.value());
                                    int b = std::get<int>(a1r.value());
                                    if (bi.op == closure::builtin_op::add)
                                        return Res{Val{a + b}};
                                    return Res{Val{a * b}};
                                }));
                        },

                        // Closure: sequential arg eval, extend env, eval body
                        [&env,
                         &app](closure::closure<CompT> const &clo) -> Res {
                            auto const &lam_layer =
                                smd::fixpoint::unwrap_fix(*clo.node);
                            if (!std::holds_alternative<
                                    fixpoint_eval::comp_lambda<CompT, MaxList>>(
                                    lam_layer))
                                return Res{
                                    foundation::parse_error{{}, "type error"}};

                            auto const &lam = std::get<
                                fixpoint_eval::comp_lambda<CompT, MaxList>>(
                                lam_layer);

                            if (app.args.size() != lam.params.size())
                                return Res{foundation::parse_error{
                                    {}, "arity mismatch"}};

                            auto new_env = clo.captured
                                               ? *clo.captured
                                               : closure::env<CompT, 16>{env};

                            for (int i = 0; i < app.args.size(); ++i) {
                                auto arg_r =
                                    detail::unwrap_sender<Res>(sender_v::then(
                                        sender_v::just(0), [&](int) -> Res {
                                            return sender_mendler_run<
                                                MaxList, MaxBindings>(
                                                *app.args[i], env);
                                        }));
                                if (!arg_r.has_value())
                                    return arg_r;
                                new_env.define(lam.params[i], arg_r.value());
                            }

                            return detail::unwrap_sender<Res>(sender_v::then(
                                sender_v::just(0),
                                [&new_env, &lam](int) -> Res {
                                    return sender_mendler_run<MaxList, 16>(
                                        *lam.body, new_env);
                                }));
                        },

                        // Foreign function: sequential arg eval, call fn
                        [&env, &app](
                            closure::foreign_function<CompT> const &ff) -> Res {
                            foundation::static_vector<Val, MaxList>
                                evaluated_args;
                            for (auto const &arg : app.args) {
                                auto arg_r =
                                    detail::unwrap_sender<Res>(sender_v::then(
                                        sender_v::just(0), [&](int) -> Res {
                                            return sender_mendler_run<
                                                MaxList, MaxBindings>(*arg,
                                                                      env);
                                        }));
                                if (!arg_r.has_value())
                                    return arg_r;
                                evaluated_args.push_back(arg_r.value());
                            }
                            return detail::unwrap_sender<Res>(sender_v::then(
                                sender_v::just(
                                    std::span<Val const>(evaluated_args.begin(),
                                                         evaluated_args.end())),
                                [&ff](std::span<Val const> args) -> Res {
                                    return ff.fn(args);
                                }));
                        },

                        // Non-callables: error
                        [](int const &) -> Res {
                            return Res{foundation::parse_error{
                                {}, "attempted to call non-function"}};
                        },
                        [](bool const &) -> Res {
                            return Res{foundation::parse_error{
                                {}, "attempted to call non-function"}};
                        },
                        [](closure::symbol const &) -> Res {
                            return Res{foundation::parse_error{
                                {}, "attempted to call non-function"}};
                        }},
                    func_r.value());
            }},
        smd::fixpoint::unwrap_fix(comp));
}

} // namespace smd::smdscheme::sender_mendler

#endif
