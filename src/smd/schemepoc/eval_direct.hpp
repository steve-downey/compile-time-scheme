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
[[nodiscard]] constexpr auto eval_direct(core_tree<MaxNodes, MaxList> const &ct,
                                         node_id root,
                                         env<MaxBindings> const &environment)
    -> result<value> {

    auto const &node = ct.get(root);

    if (std::holds_alternative<core_integer>(node))
        return value{std::get<core_integer>(node).value};

    if (std::holds_alternative<core_boolean>(node))
        return value{std::get<core_boolean>(node).value};

    if (std::holds_alternative<core_symbol>(node))
        return environment.lookup(std::get<core_symbol>(node).name);

    if (std::holds_alternative<core_if>(node)) {
        auto const &cif = std::get<core_if>(node);
        auto cond_r = eval_direct(ct, cif.condition, environment);
        if (!cond_r.has_value())
            return cond_r.error();
        auto const &cond_val = cond_r.value();
        if (std::holds_alternative<bool>(cond_val) && !std::get<bool>(cond_val))
            return eval_direct(ct, cif.alternative, environment);
        return eval_direct(ct, cif.consequent, environment);
    }

    if (std::holds_alternative<core_lambda<MaxList>>(node)) {
        return value{closure{root}};
    }

    if (std::holds_alternative<core_application<MaxList>>(node)) {
        auto const &app = std::get<core_application<MaxList>>(node);
        auto func_r = eval_direct(ct, app.func, environment);
        if (!func_r.has_value())
            return func_r.error();

        if (std::holds_alternative<builtin>(func_r.value())) {
            auto const &bi = std::get<builtin>(func_r.value());

            if (app.args.size() != 2)
                return parse_error{{}, "type error"};

            auto arg0_r = eval_direct(ct, app.args[0], environment);
            if (!arg0_r.has_value())
                return arg0_r.error();
            if (!std::holds_alternative<int>(arg0_r.value()))
                return parse_error{{}, "type error"};

            auto arg1_r = eval_direct(ct, app.args[1], environment);
            if (!arg1_r.has_value())
                return arg1_r.error();
            if (!std::holds_alternative<int>(arg1_r.value()))
                return parse_error{{}, "type error"};

            int a = std::get<int>(arg0_r.value());
            int b = std::get<int>(arg1_r.value());

            if (bi.op == builtin_op::add)
                return value{a + b};
            return value{a * b};
        }

        if (std::holds_alternative<closure>(func_r.value())) {
            auto const &clo = std::get<closure>(func_r.value());
            auto const &lam_node = ct.get(clo.node);
            // This cast should never fail if AST is valid, but we handle it
            // anyway
            if (!std::holds_alternative<core_lambda<MaxList>>(lam_node))
                return parse_error{{}, "type error"};

            auto const &lam = std::get<core_lambda<MaxList>>(lam_node);
            if (app.args.size() != lam.params.size())
                return parse_error{{}, "arity error"};

            auto new_env = environment;
            for (std::size_t i = 0; i < app.args.size(); ++i) {
                auto arg_r = eval_direct(ct, app.args[i], environment);
                if (!arg_r.has_value())
                    return arg_r.error();
                new_env.define(lam.params[i], arg_r.value());
            }

            return eval_direct(ct, lam.body, new_env);
        }

        return parse_error{{}, "type error"};
    }

    return parse_error{{}, "eval_direct: unsupported form"};
}

} // namespace smd::schemepoc

#endif
