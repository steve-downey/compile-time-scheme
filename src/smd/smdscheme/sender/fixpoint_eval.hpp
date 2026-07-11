// src/smd/smdscheme/sender/fixpoint_eval.hpp                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_FIXPOINT_EVAL_HPP
#define SRC_SMD_SMDSCHEME_SENDER_FIXPOINT_EVAL_HPP

#include <smd/smdscheme/sender/comp_tree.hpp>

#include <smd/smdscheme/closure/value.hpp>
#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/foundation/static_vector.hpp>

#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/overloaded.hpp>
#include <smd/fixpoint/recursion_schemes.hpp>

#include <span>
#include <variant>

namespace smd::smdscheme::fixpoint_eval {

/// Evaluates a computation tree (Comp) using a Mendler-style interpreter.
///
/// This is not a catamorphism: it explicitly controls which children to
/// evaluate (lazy branching in comp_if) and what environment to pass (extending
/// env for lambda bodies). The interpreter threads the environment explicitly
/// through recursive calls, implementing the reader monad pattern.
///
/// Independent arguments in comp_apply have no data dependencies and are
/// structurally visible as sibling Box nodes, enabling future sender-based
/// interpreters to evaluate them in parallel via when_all.
///
/// @tparam MaxList      Maximum list/argument length.
/// @tparam MaxBindings  Environment capacity; must be 16 (limitation of closure
///                      which hardcodes constexpr_box<env<Core, 16>>).
/// @param  comp         The computation tree root to evaluate.
/// @param  env          Current variable bindings.
/// @return The evaluated value or a parse_error.
// 64e39fd9-d4f4-4e2e-ac7e-a018fdaae728
template <int MaxList, int MaxBindings>
[[nodiscard]] constexpr auto
mendler_run(Comp<MaxList> const &comp,
            closure::env<Comp<MaxList>, MaxBindings> const &env)
    -> foundation::result<closure::value<Comp<MaxList>>> {

    static_assert(MaxBindings == 16,
                  "mendler_run currently requires closure::env<Core, 16>");

    using CompT = Comp<MaxList>;
    using Val = closure::value<CompT>;
    using Res = foundation::result<Val>;
    using Env = closure::env<CompT, MaxBindings>;

    auto const algebra =
        [](auto const &recurse, Env const &env, CompT const &node,
           comp_f_factory<MaxList>::template type<CompT> const &layer) -> Res {
        return std::visit(
            smd::fixpoint::overloaded{
                [](comp_pure const &p) -> Res {
                    return std::visit(smd::fixpoint::overloaded{
                                          [](int i) -> Res { return Val{i}; },
                                          [](bool b) -> Res { return Val{b}; },
                                          [](std::string_view sv) -> Res {
                                              return Val{closure::symbol{sv}};
                                          }},
                                      p.atom);
                },
                [&env](comp_lookup const &l) -> Res {
                    return env.lookup(l.name);
                },
                [&recurse, &env](comp_if<CompT> const &ci) -> Res {
                    auto cond_r = recurse(*ci.cond, env);
                    if (!cond_r.has_value())
                        return cond_r;

                    bool taken =
                        !std::holds_alternative<bool>(cond_r.value()) ||
                        std::get<bool>(cond_r.value());
                    return recurse(taken ? *ci.cons : *ci.alt, env);
                },
                [&env, &node](comp_lambda<CompT, MaxList> const &) -> Res {
                    return Val{closure::closure<CompT>{
                        &node, closure::constexpr_box<closure::env<CompT, 16>>{
                                   new closure::env<CompT, 16>{env}}}};
                },
                [&recurse, &env](comp_apply<CompT, MaxList> const &app) -> Res {
                    auto func_r = recurse(*app.func, env);
                    if (!func_r.has_value())
                        return func_r;

                    return std::visit(
                        smd::fixpoint::overloaded{
                            [&recurse, &env,
                             &app](closure::builtin const &bi) -> Res {
                                if (app.args.size() != 2)
                                    return Res{foundation::parse_error{
                                        {}, "arity mismatch"}};

                                auto arg0_r = recurse(*app.args[0], env);
                                if (!arg0_r.has_value())
                                    return arg0_r;
                                if (!std::holds_alternative<int>(
                                        arg0_r.value()))
                                    return Res{foundation::parse_error{
                                        {}, "type error"}};

                                auto arg1_r = recurse(*app.args[1], env);
                                if (!arg1_r.has_value())
                                    return arg1_r;
                                if (!std::holds_alternative<int>(
                                        arg1_r.value()))
                                    return Res{foundation::parse_error{
                                        {}, "type error"}};

                                int a = std::get<int>(arg0_r.value());
                                int b = std::get<int>(arg1_r.value());
                                if (bi.op == closure::builtin_op::add)
                                    return Val{a + b};
                                return Val{a * b};
                            },
                            [&recurse, &env,
                             &app](closure::closure<CompT> const &clo) -> Res {
                                auto const &lam_layer =
                                    smd::fixpoint::unwrap_fix(*clo.node);
                                if (!std::holds_alternative<
                                        comp_lambda<CompT, MaxList>>(lam_layer))
                                    return Res{foundation::parse_error{
                                        {}, "type error"}};

                                auto const &lam =
                                    std::get<comp_lambda<CompT, MaxList>>(
                                        lam_layer);

                                if (app.args.size() != lam.params.size())
                                    return Res{foundation::parse_error{
                                        {}, "arity mismatch"}};

                                auto new_env =
                                    clo.captured ? *clo.captured
                                                 : closure::env<CompT, 16>{env};

                                for (int i = 0; i < app.args.size(); ++i) {
                                    auto arg_r = recurse(*app.args[i], env);
                                    if (!arg_r.has_value())
                                        return arg_r;
                                    new_env.define(lam.params[i],
                                                   arg_r.value());
                                }

                                return recurse(*lam.body, new_env);
                            },
                            [&recurse, &env,
                             &app](closure::foreign_function<CompT> const &ff)
                                -> Res {
                                foundation::static_vector<Val, MaxList>
                                    evaluated_args;
                                for (auto const &arg : app.args) {
                                    auto arg_r = recurse(*arg, env);
                                    if (!arg_r.has_value())
                                        return arg_r;
                                    evaluated_args.push_back(arg_r.value());
                                }
                                return ff.fn(
                                    std::span<Val const>(evaluated_args.begin(),
                                                         evaluated_args.end()));
                            },
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
                            },
                            [](closure::unspecified const &) -> Res {
                                return Res{foundation::parse_error{
                                    {}, "attempted to call non-function"}};
                            }},
                        func_r.value());
                },
                [&recurse, &env](comp_set<CompT> const &s) -> Res {
                    auto val_r = recurse(*s.value, env);
                    if (!val_r.has_value())
                        return val_r;
                    return env.assign(s.name, val_r.value());
                },
                [&recurse, &env](comp_begin<CompT, MaxList> const &b) -> Res {
                    Res last{foundation::parse_error{{}, "begin: empty"}};
                    for (int i = 0; i < b.exprs.size(); ++i) {
                        last = recurse(*b.exprs[i], env);
                        if (!last.has_value())
                            return last;
                    }
                    return last;
                }},
            layer);
    };

    return smd::fixpoint::mendler_para<Res,
                                       comp_f_factory<MaxList>::template type>(
        algebra, env, comp);
}
// 64e39fd9-d4f4-4e2e-ac7e-a018fdaae728 end

} // namespace smd::smdscheme::fixpoint_eval

#endif
