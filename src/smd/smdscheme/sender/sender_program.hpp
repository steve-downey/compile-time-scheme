// src/smd/smdscheme/sender/sender_program.hpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_SENDER_PROGRAM_HPP
#define SRC_SMD_SMDSCHEME_SENDER_SENDER_PROGRAM_HPP

#include <smd/smdscheme/closure/value.hpp>
#include <smd/smdscheme/elaborator/elaborate.hpp>
#include <smd/smdscheme/elaborator/elaborated_core.hpp>
#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/reader/read_datum.hpp>
#include <smd/smdscheme/parser/cursor.hpp>
#include <smd/smdscheme/sender/sender_v.hpp>

namespace smd::smdscheme::sender {
namespace sender_backend {
// 87e88913-dd13-4994-b2fe-04cb6ffa34ee

using elaborator::core_application;
using elaborator::core_boolean;
using elaborator::core_if;
using elaborator::core_integer;
using elaborator::core_lambda;
using elaborator::core_quote;
using elaborator::core_symbol;

// NOTE: Creating a fully dynamic sender graph from a runtime AST without
// type erasure (std::any/any_sender) or coroutines (Beman Task) hits
// C++ structural typing limits, as `when_all` and `then` have distinct types.
// We use `beman::task::task` as an asynchronous sender factory which
// type-erases the graph structures into a common coroutine state machine type.

template <int MaxNodes, int MaxList, int MaxBindings>
sender_v::task<foundation::result<
    closure::value<elaborator::core_type<MaxNodes, MaxList>>>>
compile_node(elaborator::core_type<MaxNodes, MaxList> const &node,
             foundation::tree_arena<elaborator::core_type<MaxNodes, MaxList>,
                                    MaxNodes> const &arena,
             closure::env<elaborator::core_type<MaxNodes, MaxList>, MaxBindings>
                 environment);

template <int MaxNodes, int MaxList, int MaxBindings>
sender_v::task<foundation::result<
    closure::value<elaborator::core_type<MaxNodes, MaxList>>>>
compile_node(elaborator::core_type<MaxNodes, MaxList> const &node,
             foundation::tree_arena<elaborator::core_type<MaxNodes, MaxList>,
                                    MaxNodes> const &arena,
             closure::env<elaborator::core_type<MaxNodes, MaxList>, MaxBindings>
                 environment) {

    using Core = elaborator::core_type<MaxNodes, MaxList>;

    // Literal evaluations
    if (std::holds_alternative<core_integer>(node.inner))
        co_return closure::value<Core>{
            std::get<core_integer>(node.inner).value};

    if (std::holds_alternative<core_boolean>(node.inner))
        co_return closure::value<Core>{
            std::get<core_boolean>(node.inner).value};

    if (std::holds_alternative<core_symbol>(node.inner))
        co_return environment.lookup(std::get<core_symbol>(node.inner).name);

    if (std::holds_alternative<core_quote>(node.inner)) {
        auto const &cq = std::get<core_quote>(node.inner);
        if (std::holds_alternative<int>(cq.atom))
            co_return closure::value<Core>{std::get<int>(cq.atom)};
        if (std::holds_alternative<bool>(cq.atom))
            co_return closure::value<Core>{std::get<bool>(cq.atom)};
        co_return closure::value<Core>{
            closure::symbol{std::get<std::string_view>(cq.atom)}};
    }

    if (std::holds_alternative<core_if<Core, MaxNodes>>(node.inner)) {
        auto const &cif = std::get<core_if<Core, MaxNodes>>(node.inner);
        auto cond_r = co_await compile_node<MaxNodes, MaxList, MaxBindings>(
            arena.get(cif.condition), arena, environment);
        if (!cond_r.has_value())
            co_return cond_r.error();

        auto const &cond_val = cond_r.value();
        if (std::holds_alternative<bool>(cond_val) &&
            !std::get<bool>(cond_val)) {
            co_return co_await compile_node<MaxNodes, MaxList, MaxBindings>(
                arena.get(cif.alternative), arena, environment);
        }
        co_return co_await compile_node<MaxNodes, MaxList, MaxBindings>(
            arena.get(cif.consequent), arena, environment);
    }

    if (std::holds_alternative<core_lambda<Core, MaxNodes, MaxList>>(
            node.inner)) {
        co_return closure::value<Core>{closure::closure<Core>{
            &node, closure::constexpr_box<closure::env<Core, 16>>{
                       new closure::env<Core, 16>{environment}}}};
    }

    if (std::holds_alternative<core_application<Core, MaxNodes, MaxList>>(
            node.inner)) {
        auto const &app =
            std::get<core_application<Core, MaxNodes, MaxList>>(node.inner);
        auto func_r = co_await compile_node<MaxNodes, MaxList, MaxBindings>(
            arena.get(app.func), arena, environment);
        if (!func_r.has_value())
            co_return func_r.error();

        if (std::holds_alternative<closure::builtin>(func_r.value())) {
            auto const &bi = std::get<closure::builtin>(func_r.value());

            if (app.args.size() != 2)
                co_return foundation::parse_error{{}, "arity mismatch"};

            auto arg0_r = co_await compile_node<MaxNodes, MaxList, MaxBindings>(
                arena.get(app.args[0]), arena, environment);
            if (!arg0_r.has_value())
                co_return arg0_r.error();
            if (!std::holds_alternative<int>(arg0_r.value()))
                co_return foundation::parse_error{{}, "type error"};

            auto arg1_r = co_await compile_node<MaxNodes, MaxList, MaxBindings>(
                arena.get(app.args[1]), arena, environment);
            if (!arg1_r.has_value())
                co_return arg1_r.error();
            if (!std::holds_alternative<int>(arg1_r.value()))
                co_return foundation::parse_error{{}, "type error"};

            int a = std::get<int>(arg0_r.value());
            int b = std::get<int>(arg1_r.value());

            if (bi.op == closure::builtin_op::add)
                co_return closure::value<Core>{a + b};
            co_return closure::value<Core>{a * b};
        }

        if (std::holds_alternative<closure::closure<Core>>(func_r.value())) {
            auto const &clo = std::get<closure::closure<Core>>(func_r.value());
            auto const &lam_node = *clo.node;
            if (!std::holds_alternative<core_lambda<Core, MaxNodes, MaxList>>(
                    lam_node.inner))
                co_return foundation::parse_error{{}, "type error"};

            auto const &lam =
                std::get<core_lambda<Core, MaxNodes, MaxList>>(lam_node.inner);

            if (app.args.size() != lam.params.size())
                co_return foundation::parse_error{{}, "arity mismatch"};

            auto new_env = clo.captured ? *clo.captured : environment;
            for (int i = 0; i < app.args.size(); ++i) {
                auto arg_r =
                    co_await compile_node<MaxNodes, MaxList, MaxBindings>(
                        arena.get(app.args[i]), arena, environment);
                if (!arg_r.has_value())
                    co_return arg_r.error();
                new_env.define(lam.params[i], arg_r.value());
            }

            co_return co_await compile_node<MaxNodes, MaxList, MaxBindings>(
                arena.get(lam.body), arena, new_env);
        }

        if (std::holds_alternative<closure::foreign_function<Core>>(
                func_r.value())) {
            auto const &ff =
                std::get<closure::foreign_function<Core>>(func_r.value());
            foundation::static_vector<closure::value<Core>, MaxNodes>
                evaluated_args;
            for (auto const &arg_id : app.args) {
                auto arg_r =
                    co_await compile_node<MaxNodes, MaxList, MaxBindings>(
                        arena.get(arg_id), arena, environment);
                if (!arg_r.has_value())
                    co_return arg_r.error();
                evaluated_args.push_back(arg_r.value());
            }
            co_return ff.fn(std::span<closure::value<Core> const>(
                evaluated_args.begin(), evaluated_args.end()));
        }

        co_return foundation::parse_error{{}, "attempted to call non-function"};
    }

    co_return foundation::parse_error{{}, "compile_node: unsupported form"};
}

} // namespace sender_backend

template <int MaxNodes, int MaxList>
struct sender_program {
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    Core root;
    foundation::tree_arena<Core, MaxNodes> arena;

    template <int MaxBindings>
    auto operator()(closure::env<Core, MaxBindings> environment) const
        -> sender_v::task<foundation::result<closure::value<Core>>> {
        return sender_backend::compile_node<MaxNodes, MaxList, MaxBindings>(
            root, arena, std::move(environment));
    }
};

template <int MaxNodes = 32, int MaxList = 16>
[[nodiscard]] constexpr auto compile_to_sender(std::string_view src) {
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    using ProgramT = sender_program<MaxNodes, MaxList>;

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
// 87e88913-dd13-4994-b2fe-04cb6ffa34ee end

} // namespace smd::smdscheme::sender
#endif
