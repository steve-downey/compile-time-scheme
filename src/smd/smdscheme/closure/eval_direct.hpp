// src/smd/smdscheme/closure/eval_direct.hpp                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_CLOSURE_EVAL_DIRECT_HPP
#define SRC_SMD_SMDSCHEME_CLOSURE_EVAL_DIRECT_HPP

#include <smd/smdscheme/closure/value.hpp>
#include <smd/smdscheme/elaborator/elaborate.hpp>
#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/foundation/static_vector.hpp>

#include <smd/fixpoint/overloaded.hpp>

#include <span>
#include <variant>

namespace smd::smdscheme {

/// Directly evaluates a core AST node in the given environment.
///
/// This is a straightforward tree-walking interpreter over the core AST,
/// without CPS transformation.  It is simpler than @ref cps::cps_dispatch
/// but does not support first-class continuations.
///
/// Recursion is bounded by the tree depth (which is bounded by @p MaxNodes).
/// Only one environment size is supported at runtime; the static_assert
/// enforces this.
///
/// @tparam MaxNodes    Arena capacity; bounds tree depth.
/// @tparam MaxList     Maximum argument/list length.
/// @tparam MaxBindings Environment capacity; must be 16.
/// @param  node        The core node to evaluate.
/// @param  arena       Core arena; must outlive the evaluation.
/// @param  environment Current variable bindings.
/// @return The evaluated @ref closure::value on success, or a
///         @ref foundation::parse_error on type error, arity mismatch, or
///         unbound variable.
template <int MaxNodes, int MaxList, int MaxBindings>
[[nodiscard]] constexpr auto eval_direct(
    elaborator::core_type<MaxNodes, MaxList> const &node,
    const foundation::tree_arena<elaborator::core_type<MaxNodes, MaxList>,
                                 MaxNodes> &arena,
    closure::env<elaborator::core_type<MaxNodes, MaxList>, MaxBindings> const
        &environment)
    -> foundation::result<
        closure::value<elaborator::core_type<MaxNodes, MaxList>>> {
    static_assert(MaxBindings == 16,
                  "eval_direct currently requires closure::env<Core,16>");
    using Core = elaborator::core_type<MaxNodes, MaxList>;

    using Val = closure::value<Core>;
    using Res = foundation::result<Val>;

    return std::visit(
        smd::fixpoint::overloaded{
            [&](elaborator::core_integer const &ci) -> Res {
                return Val{ci.value};
            },
            [&](elaborator::core_boolean const &cb) -> Res {
                return Val{cb.value};
            },
            [&](elaborator::core_symbol const &cs) -> Res {
                return environment.lookup(cs.name);
            },
            [&](elaborator::core_quote const &cq) -> Res {
                return std::visit(smd::fixpoint::overloaded{
                                      [](int i) -> Val { return Val{i}; },
                                      [](bool b) -> Val { return Val{b}; },
                                      [](std::string_view sv) -> Val {
                                          return Val{closure::symbol{sv}};
                                      }},
                                  cq.atom);
            },
            [&](elaborator::core_if<Core, MaxNodes> const &cif) -> Res {
                // e9bae797-69b6-4682-b77f-2110138bf5ab
                auto cond_r = eval_direct<MaxNodes, MaxList, MaxBindings>(
                    arena.get(cif.condition), arena, environment);
                if (!cond_r.has_value()) {
                    return cond_r.error();
                }
                auto const &cond_val = cond_r.value();
                if (std::holds_alternative<bool>(cond_val) &&
                    !std::get<bool>(cond_val)) {
                    return eval_direct<MaxNodes, MaxList, MaxBindings>(
                        arena.get(cif.alternative), arena, environment);
                }
                return eval_direct<MaxNodes, MaxList, MaxBindings>(
                    arena.get(cif.consequent), arena, environment);
                // e9bae797-69b6-4682-b77f-2110138bf5ab end
            },
            [&](elaborator::core_lambda<Core, MaxNodes, MaxList> const &)
                -> Res {
                return Val{closure::closure<Core>{
                    &node, closure::constexpr_box<closure::env<Core, 16>>{
                               new closure::env<Core, 16>{environment}}}};
            },
            [&](elaborator::core_application<Core, MaxNodes, MaxList> const
                    &app) -> Res {
                auto func_r = eval_direct<MaxNodes, MaxList, MaxBindings>(
                    arena.get(app.func), arena, environment);
                if (!func_r.has_value()) {
                    return func_r.error();
                }

                return std::visit(
                    smd::fixpoint::overloaded{
                        [&](closure::builtin const &bi) -> Res {
                            if (app.args.size() != 2) {
                                return foundation::parse_error{
                                    {}, "arity mismatch"};
                            }

                            auto arg0_r =
                                eval_direct<MaxNodes, MaxList, MaxBindings>(
                                    arena.get(app.args[0]), arena, environment);
                            if (!arg0_r.has_value()) {
                                return arg0_r.error();
                            }
                            if (!std::holds_alternative<int>(arg0_r.value())) {
                                return foundation::parse_error{{},
                                                               "type error"};
                            }

                            auto arg1_r =
                                eval_direct<MaxNodes, MaxList, MaxBindings>(
                                    arena.get(app.args[1]), arena, environment);
                            if (!arg1_r.has_value()) {
                                return arg1_r.error();
                            }
                            if (!std::holds_alternative<int>(arg1_r.value())) {
                                return foundation::parse_error{{},
                                                               "type error"};
                            }

                            int a = std::get<int>(arg0_r.value());
                            int b = std::get<int>(arg1_r.value());
                            if (bi.op == closure::builtin_op::add) {
                                return Val{a + b};
                            }
                            return Val{a * b};
                        },
                        [&](closure::closure<Core> const &clo) -> Res {
                            auto const &lam_node = *clo.node;
                            if (!std::holds_alternative<elaborator::core_lambda<
                                    Core, MaxNodes, MaxList>>(lam_node.inner)) {
                                return foundation::parse_error{{},
                                                               "type error"};
                            }

                            auto const &lam = std::get<elaborator::core_lambda<
                                Core, MaxNodes, MaxList>>(lam_node.inner);
                            if (app.args.size() != lam.params.size()) {
                                return foundation::parse_error{
                                    {}, "arity mismatch"};
                            }

                            auto new_env =
                                clo.captured
                                    ? *clo.captured
                                    : closure::env<Core, 16>{environment};
                            for (int i = 0; i < app.args.size(); ++i) {
                                auto arg_r =
                                    eval_direct<MaxNodes, MaxList, MaxBindings>(
                                        arena.get(app.args[i]), arena,
                                        environment);
                                if (!arg_r.has_value()) {
                                    return arg_r.error();
                                }
                                new_env.define(lam.params[i], arg_r.value());
                            }

                            return eval_direct<MaxNodes, MaxList, 16>(
                                arena.get(lam.body), arena, new_env);
                        },
                        [&](closure::foreign_function<Core> const &ff) -> Res {
                            foundation::static_vector<Val, MaxNodes>
                                evaluated_args;
                            for (auto const &arg_id : app.args) {
                                auto arg_r =
                                    eval_direct<MaxNodes, MaxList, MaxBindings>(
                                        arena.get(arg_id), arena, environment);
                                if (!arg_r.has_value()) {
                                    return arg_r.error();
                                }
                                evaluated_args.push_back(arg_r.value());
                            }
                            return ff.fn(std::span<Val const>(
                                evaluated_args.begin(), evaluated_args.end()));
                        },
                        [](int const &) -> Res {
                            return foundation::parse_error{
                                {}, "attempted to call non-function"};
                        },
                        [](bool const &) -> Res {
                            return foundation::parse_error{
                                {}, "attempted to call non-function"};
                        },
                        [](closure::symbol const &) -> Res {
                            return foundation::parse_error{
                                {}, "attempted to call non-function"};
                        },
                        [](closure::unspecified const &) -> Res {
                            return foundation::parse_error{
                                {}, "attempted to call non-function"};
                        }},
                    func_r.value());
            },
            [&](elaborator::core_define<Core, MaxNodes> const &) -> Res {
                return foundation::parse_error{
                    {},
                    "eval_direct: define not supported in expression "
                    "context"};
            },
            [&](elaborator::core_set<Core, MaxNodes> const &cs) -> Res {
                auto val_r = eval_direct<MaxNodes, MaxList, MaxBindings>(
                    arena.get(cs.value), arena, environment);
                if (!val_r.has_value())
                    return val_r.error();
                return environment.assign(cs.name, val_r.value());
            },
            [&](elaborator::core_begin<Core, MaxNodes, MaxList> const &cb)
                -> Res {
                Res last{foundation::parse_error{{}, "begin: empty"}};
                for (int i = 0; i < cb.exprs.size(); ++i) {
                    last = eval_direct<MaxNodes, MaxList, MaxBindings>(
                        arena.get(cb.exprs[i]), arena, environment);
                    if (!last.has_value())
                        return last;
                }
                return last;
            }},
        node.inner);
}

} // namespace smd::smdscheme

#endif
