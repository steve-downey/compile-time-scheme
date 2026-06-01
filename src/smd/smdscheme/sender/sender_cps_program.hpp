// src/smd/smdscheme/sender/sender_cps_program.hpp -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_SENDER_CPS_PROGRAM_HPP
#define SRC_SMD_SMDSCHEME_SENDER_SENDER_CPS_PROGRAM_HPP

#include <smd/smdscheme/closure/value.hpp>
#include <smd/smdscheme/elaborator/elaborate.hpp>
#include <smd/smdscheme/elaborator/elaborated_core.hpp>
#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/parser/cursor.hpp>
#include <smd/smdscheme/reader/read_datum.hpp>
#include <smd/smdscheme/sender/sender_v.hpp>

#include <span>
#include <variant>

namespace smd::smdscheme::sender_cps {

using elaborator::core_application;
using elaborator::core_boolean;
using elaborator::core_if;
using elaborator::core_integer;
using elaborator::core_lambda;
using elaborator::core_quote;
using elaborator::core_symbol;

namespace detail {

/// Synchronously drives a sender to completion via @c sync_wait and extracts
/// the first tuple element.
///
/// @tparam T   The value type to extract.
/// @param  sender  A Beman Execution26 sender producing @c std::tuple<T>.
template <typename T>
T unwrap_sender(auto sender) {
    return std::get<0>(sender_v::sync_wait(std::move(sender)).value());
}

} // namespace detail

/// Evaluates a core AST node using Beman Execution26 senders as the
/// computation primitives.
///
/// This backend expresses the interpreter in terms of @c just, @c then,
/// and @c when_all rather than coroutines or direct recursion.  The sender
/// graph is constructed eagerly for each sub-expression, then driven to
/// completion via @ref detail::unwrap_sender.  This makes the evaluation
/// strategy visible as a first-class sender graph, which is the purpose of
/// this backend.
///
/// @tparam MaxNodes    Arena capacity.
/// @tparam MaxList     Maximum argument/list length.
/// @tparam MaxBindings Environment capacity.
/// @param  node        Core node to evaluate.
/// @param  arena       Core arena; must outlive the call.
/// @param  environment Current variable bindings.
/// @return The evaluated value or a @ref foundation::parse_error.
template <int MaxNodes, int MaxList, int MaxBindings>
auto eval_node(elaborator::core_type<MaxNodes, MaxList> const &node,
               foundation::tree_arena<elaborator::core_type<MaxNodes, MaxList>,
                                      MaxNodes> const &arena,
               closure::env<elaborator::core_type<MaxNodes, MaxList>,
                            MaxBindings> const &environment)
    -> foundation::result<
        closure::value<elaborator::core_type<MaxNodes, MaxList>>> {

    using Core = elaborator::core_type<MaxNodes, MaxList>;
    using Val = closure::value<Core>;
    using Res = foundation::result<Val>;

    // Integer literal: just produces the value
    if (std::holds_alternative<core_integer>(node.inner)) {
        return detail::unwrap_sender<Res>(
            sender_v::just(Res{Val{std::get<core_integer>(node.inner).value}}));
    }

    // Boolean literal: just produces the value
    if (std::holds_alternative<core_boolean>(node.inner)) {
        return detail::unwrap_sender<Res>(
            sender_v::just(Res{Val{std::get<core_boolean>(node.inner).value}}));
    }

    // Symbol: just the environment, then lookup
    if (std::holds_alternative<core_symbol>(node.inner)) {
        auto name = std::get<core_symbol>(node.inner).name;
        return detail::unwrap_sender<Res>(sender_v::then(
            sender_v::just(name), [&environment](std::string_view n) -> Res {
                return environment.lookup(n);
            }));
    }

    // Quote: just produces the quoted atom
    if (std::holds_alternative<core_quote>(node.inner)) {
        auto const &cq = std::get<core_quote>(node.inner);
        Val v;
        if (std::holds_alternative<int>(cq.atom))
            v = Val{std::get<int>(cq.atom)};
        else if (std::holds_alternative<bool>(cq.atom))
            v = Val{std::get<bool>(cq.atom)};
        else
            v = Val{closure::symbol{std::get<std::string_view>(cq.atom)}};
        return detail::unwrap_sender<Res>(sender_v::just(Res{v}));
    }

    // If: evaluate condition, then select and evaluate branch
    if (std::holds_alternative<core_if<Core, MaxNodes>>(node.inner)) {
        auto const &cif = std::get<core_if<Core, MaxNodes>>(node.inner);
        auto cond_r = eval_node<MaxNodes, MaxList, MaxBindings>(
            arena.get(cif.condition), arena, environment);

        return detail::unwrap_sender<Res>(sender_v::then(
            sender_v::just(std::move(cond_r)), [&](Res cv) -> Res {
                if (!cv.has_value())
                    return cv;
                bool taken = !std::holds_alternative<bool>(cv.value()) ||
                             std::get<bool>(cv.value());
                auto const &branch = taken ? cif.consequent : cif.alternative;
                return eval_node<MaxNodes, MaxList, MaxBindings>(
                    arena.get(branch), arena, environment);
            }));
    }

    // Lambda: just wraps the closure value
    if (std::holds_alternative<core_lambda<Core, MaxNodes, MaxList>>(
            node.inner)) {
        return detail::unwrap_sender<Res>(
            sender_v::just(Res{Val{closure::closure<Core>{
                &node, closure::constexpr_box<closure::env<Core, 16>>{
                           new closure::env<Core, 16>{environment}}}}}));
    }

    // Application: evaluate func and args, then apply
    if (std::holds_alternative<core_application<Core, MaxNodes, MaxList>>(
            node.inner)) {
        auto const &app =
            std::get<core_application<Core, MaxNodes, MaxList>>(node.inner);

        auto func_r = eval_node<MaxNodes, MaxList, MaxBindings>(
            arena.get(app.func), arena, environment);
        if (!func_r.has_value())
            return func_r;

        // e619ae0a-47be-48b4-86f5-1dfac90299e6
        // Builtin application: when_all args, then apply operator
        if (std::holds_alternative<closure::builtin>(func_r.value())) {
            auto const &bi = std::get<closure::builtin>(func_r.value());
            if (app.args.size() != 2)
                return Res{foundation::parse_error{{}, "arity mismatch"}};

            auto arg0_r = eval_node<MaxNodes, MaxList, MaxBindings>(
                arena.get(app.args[0]), arena, environment);
            auto arg1_r = eval_node<MaxNodes, MaxList, MaxBindings>(
                arena.get(app.args[1]), arena, environment);

            return detail::unwrap_sender<Res>(sender_v::then(
                sender_v::when_all(sender_v::just(std::move(arg0_r)),
                                   sender_v::just(std::move(arg1_r))),
                [bi](Res a0r, Res a1r) -> Res {
                    if (!a0r.has_value())
                        return a0r;
                    if (!a1r.has_value())
                        return a1r;
                    if (!std::holds_alternative<int>(a0r.value()) ||
                        !std::holds_alternative<int>(a1r.value()))
                        return Res{foundation::parse_error{{}, "type error"}};
                    int a = std::get<int>(a0r.value());
                    int b = std::get<int>(a1r.value());
                    if (bi.op == closure::builtin_op::add)
                        return Res{Val{a + b}};
                    return Res{Val{a * b}};
                }));
        }
        // e619ae0a-47be-48b4-86f5-1dfac90299e6 end

        // Closure application: evaluate args, bind params, evaluate body
        if (std::holds_alternative<closure::closure<Core>>(func_r.value())) {
            auto const &clo = std::get<closure::closure<Core>>(func_r.value());
            auto const &lam_node = *clo.node;

            if (!std::holds_alternative<core_lambda<Core, MaxNodes, MaxList>>(
                    lam_node.inner))
                return Res{foundation::parse_error{{}, "type error"}};

            auto const &lam =
                std::get<core_lambda<Core, MaxNodes, MaxList>>(lam_node.inner);

            if (app.args.size() != lam.params.size())
                return Res{foundation::parse_error{{}, "arity mismatch"}};

            auto new_env = clo.captured ? *clo.captured : environment;
            for (int i = 0; i < app.args.size(); ++i) {
                auto arg_r = eval_node<MaxNodes, MaxList, MaxBindings>(
                    arena.get(app.args[i]), arena, environment);
                if (!arg_r.has_value())
                    return arg_r;
                new_env.define(lam.params[i], arg_r.value());
            }

            return eval_node<MaxNodes, MaxList, MaxBindings>(
                arena.get(lam.body), arena, new_env);
        }

        // Foreign function: evaluate args, then apply
        if (std::holds_alternative<closure::foreign_function<Core>>(
                func_r.value())) {
            auto const &ff =
                std::get<closure::foreign_function<Core>>(func_r.value());

            foundation::static_vector<Val, MaxNodes> evaluated_args;
            for (auto const &arg_id : app.args) {
                auto arg_r = eval_node<MaxNodes, MaxList, MaxBindings>(
                    arena.get(arg_id), arena, environment);
                if (!arg_r.has_value())
                    return arg_r;
                evaluated_args.push_back(arg_r.value());
            }

            return detail::unwrap_sender<Res>(sender_v::then(
                sender_v::just(std::span<Val const>(evaluated_args.begin(),
                                                    evaluated_args.end())),
                [&ff](std::span<Val const> args) -> Res {
                    return ff.fn(args);
                }));
        }

        return Res{
            foundation::parse_error{{}, "attempted to call non-function"}};
    }

    return Res{foundation::parse_error{{}, "sender_cps: unsupported form"}};
}

/// A compiled Scheme program that evaluates via Execution26 sender primitives.
///
/// Holds the elaborated root and arena by value.  @c operator() calls
/// @ref eval_node directly; the evaluation is synchronous from the caller's
/// perspective even though sender graphs are constructed internally.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum argument/list length.
template <int MaxNodes, int MaxList>
struct sender_cps_program {
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    Core root; ///< Elaborated root expression.
    foundation::tree_arena<Core, MaxNodes>
        arena; ///< Arena owning all sub-nodes.

    /// Evaluates the program in @p environment.
    ///
    /// @tparam MaxBindings Environment capacity.
    template <int MaxBindings>
    auto operator()(closure::env<Core, MaxBindings> const &environment) const
        -> foundation::result<closure::value<Core>> {
        return eval_node<MaxNodes, MaxList, MaxBindings>(root, arena,
                                                         environment);
    }
};

/// Compiles a Scheme source string to a @ref sender_cps_program.
///
/// Chains read → elaborate; evaluation happens at call time via @ref eval_node
/// using Execution26 sender primitives.
///
/// @tparam MaxNodes Arena capacity (default 32).
/// @tparam MaxList  Maximum argument/list length (default 16).
/// @param  src      Scheme source to compile.
/// @return A @c result<sender_cps_program<...>> on success.
template <int MaxNodes = 32, int MaxList = 16>
[[nodiscard]] constexpr auto compile_sender_cps(std::string_view src) {
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    using ProgramT = sender_cps_program<MaxNodes, MaxList>;

    foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>, MaxNodes>
        arena_dr;
    auto dr =
        reader::read_datum<MaxNodes, MaxList>(parser::cursor{src}, arena_dr);
    if (!dr.has_value())
        return foundation::result<ProgramT>{dr.error()};
    foundation::tree_arena<Core, MaxNodes> core_arena;
    auto er = elaborator::elaborate<MaxNodes, MaxList>(dr.value().value,
                                                       arena_dr, core_arena);
    if (!er.has_value())
        return foundation::result<ProgramT>{er.error()};
    auto const &ct_root = er.value();
    return foundation::result<ProgramT>{ProgramT{ct_root, core_arena}};
}

} // namespace smd::smdscheme::sender_cps

#endif
