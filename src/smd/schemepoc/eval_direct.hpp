// src/smd/schemepoc/eval_direct.hpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SCHEMEPOC_EVAL_DIRECT_HPP
#define SRC_SMD_SCHEMEPOC_EVAL_DIRECT_HPP

#include <smd/schemepoc/elaborator.hpp>
#include <smd/schemepoc/result.hpp>
#include <smd/schemepoc/value.hpp>

#include <variant>

namespace smd::schemepoc {

template <int MaxNodes, int MaxList, int MaxBindings>
[[nodiscard]] constexpr auto
eval_direct(core_type<MaxNodes, MaxList> const &node,
            const tree_arena<core_type<MaxNodes, MaxList>, MaxNodes> &arena,
            env<core_type<MaxNodes, MaxList>, MaxBindings> const &environment)
    -> result<value<core_type<MaxNodes, MaxList>>> {

    using Core = core_type<MaxNodes, MaxList>;

    if (std::holds_alternative<core_integer>(node.inner))
        return value<Core>{std::get<core_integer>(node.inner).value};

    if (std::holds_alternative<core_boolean>(node.inner))
        return value<Core>{std::get<core_boolean>(node.inner).value};

    if (std::holds_alternative<core_symbol>(node.inner))
        return environment.lookup(std::get<core_symbol>(node.inner).name);

    if (std::holds_alternative<core_quote>(node.inner)) {
        auto const &cq = std::get<core_quote>(node.inner);
        if (std::holds_alternative<int>(cq.atom))
            return value<Core>{std::get<int>(cq.atom)};
        if (std::holds_alternative<bool>(cq.atom))
            return value<Core>{std::get<bool>(cq.atom)};
        return value<Core>{symbol{std::get<std::string_view>(cq.atom)}};
    }

    if (std::holds_alternative<core_if<Core, MaxNodes>>(node.inner)) {
        auto const &cif = std::get<core_if<Core, MaxNodes>>(node.inner);
        auto cond_r = eval_direct<MaxNodes, MaxList, MaxBindings>(
            arena.get(cif.condition), arena, environment);
        if (!cond_r.has_value())
            return cond_r.error();
        auto const &cond_val = cond_r.value();
        if (std::holds_alternative<bool>(cond_val) && !std::get<bool>(cond_val))
            return eval_direct<MaxNodes, MaxList, MaxBindings>(
                arena.get(cif.alternative), arena, environment);
        return eval_direct<MaxNodes, MaxList, MaxBindings>(
            arena.get(cif.consequent), arena, environment);
    }

    if (std::holds_alternative<core_lambda<Core, MaxNodes, MaxList>>(
            node.inner)) {
        return value<Core>{closure<Core>{
            &node,
            constexpr_box<env<Core, 16>>{new env<Core, 16>{environment}}}};
    }

    if (std::holds_alternative<core_application<Core, MaxNodes, MaxList>>(
            node.inner)) {
        auto const &app =
            std::get<core_application<Core, MaxNodes, MaxList>>(node.inner);
        auto func_r = eval_direct<MaxNodes, MaxList, MaxBindings>(
            arena.get(app.func), arena, environment);
        if (!func_r.has_value())
            return func_r.error();

        if (std::holds_alternative<builtin>(func_r.value())) {
            auto const &bi = std::get<builtin>(func_r.value());

            if (app.args.size() != 2)
                return parse_error{{}, "arity mismatch"};

            auto arg0_r = eval_direct<MaxNodes, MaxList, MaxBindings>(
                arena.get(app.args[0]), arena, environment);
            if (!arg0_r.has_value())
                return arg0_r.error();
            if (!std::holds_alternative<int>(arg0_r.value()))
                return parse_error{{}, "type error"};

            auto arg1_r = eval_direct<MaxNodes, MaxList, MaxBindings>(
                arena.get(app.args[1]), arena, environment);
            if (!arg1_r.has_value())
                return arg1_r.error();
            if (!std::holds_alternative<int>(arg1_r.value()))
                return parse_error{{}, "type error"};

            int a = std::get<int>(arg0_r.value());
            int b = std::get<int>(arg1_r.value());

            if (bi.op == builtin_op::add)
                return value<Core>{a + b};
            return value<Core>{a * b};
        }

        if (std::holds_alternative<closure<Core>>(func_r.value())) {
            auto const &clo = std::get<closure<Core>>(func_r.value());
            auto const &lam_node = *clo.node;
            // This cast should never fail if AST is valid, but we handle it
            // anyway
            if (!std::holds_alternative<core_lambda<Core, MaxNodes, MaxList>>(
                    lam_node.inner))
                return parse_error{{}, "type error"};

            auto const &lam =
                std::get<core_lambda<Core, MaxNodes, MaxList>>(lam_node.inner);
            if (app.args.size() != lam.params.size())
                return parse_error{{}, "arity mismatch"};

            auto new_env = clo.captured ? *clo.captured : environment;
            for (std::size_t i = 0; i < app.args.size(); ++i) {
                auto arg_r = eval_direct<MaxNodes, MaxList, MaxBindings>(
                    arena.get(app.args[i]), arena, environment);
                if (!arg_r.has_value())
                    return arg_r.error();
                new_env.define(lam.params[i], arg_r.value());
            }

            return eval_direct<MaxNodes, MaxList, MaxBindings>(
                arena.get(lam.body), arena, new_env);
        }

        if (std::holds_alternative<foreign_function<Core>>(func_r.value())) {
            auto const &ff = std::get<foreign_function<Core>>(func_r.value());
            static_vector<value<Core>, MaxNodes> evaluated_args;
            for (auto const &arg_id : app.args) {
                auto arg_r = eval_direct<MaxNodes, MaxList, MaxBindings>(
                    arena.get(arg_id), arena, environment);
                if (!arg_r.has_value())
                    return arg_r.error();
                evaluated_args.push_back(arg_r.value());
            }
            return ff.fn(std::span<value<Core> const>(evaluated_args.begin(),
                                                      evaluated_args.end()));
        }

        return parse_error{{}, "attempted to call non-function"};
    }

    return parse_error{{}, "eval_direct: unsupported form"};
}

} // namespace smd::schemepoc

#endif
