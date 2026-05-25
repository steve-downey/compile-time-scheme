// src/smd/schemepoc/cps.hpp                                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SCHEMEPOC_CPS_HPP
#define SRC_SMD_SCHEMEPOC_CPS_HPP

#include <smd/schemepoc/elaborator.hpp>
#include <smd/schemepoc/result.hpp>
#include <smd/schemepoc/value.hpp>

#include <variant>

namespace smd::schemepoc {

template <class F>
struct cps_code {
    F f;

    template <class Env, class K>
    constexpr auto operator()(Env const &env, K k) const {
        return f(env, k);
    }
};

template <class F>
cps_code(F) -> cps_code<F>;

namespace detail {

template <class Core>
struct identity_k {
    constexpr auto operator()(value<Core> v) const -> result<value<Core>> {
        return v;
    }
};

template <int MaxNodes, int MaxList, class Cont, class Env, class K>
constexpr auto
cps_dispatch(core_type<MaxNodes, MaxList> const &node,
             const tree_arena<core_type<MaxNodes, MaxList>, MaxNodes> &arena,
             Cont const &cont, Env const &env, K const &k)
    -> result<value<core_type<MaxNodes, MaxList>>> {
    using Core = core_type<MaxNodes, MaxList>;

    if (std::holds_alternative<core_integer>(node.inner)) {
        auto r = cont(value<Core>{std::get<core_integer>(node.inner).value});
        if (!r.has_value())
            return r;
        return k(r.value());
    }

    if (std::holds_alternative<core_boolean>(node.inner)) {
        auto r = cont(value<Core>{std::get<core_boolean>(node.inner).value});
        if (!r.has_value())
            return r;
        return k(r.value());
    }

    if (std::holds_alternative<core_symbol>(node.inner)) {
        auto lr = env.lookup(std::get<core_symbol>(node.inner).name);
        if (!lr.has_value())
            return result<value<Core>>{lr.error()};
        auto r = cont(lr.value());
        if (!r.has_value())
            return r;
        return k(r.value());
    }

    if (std::holds_alternative<core_if<Core, MaxNodes>>(node.inner)) {
        auto const &cif = std::get<core_if<Core, MaxNodes>>(node.inner);
        auto cond_r = cps_dispatch<MaxNodes, MaxList>(arena.get(cif.condition),
                                                      arena, identity_k<Core>{},
                                                      env, identity_k<Core>{});
        if (!cond_r.has_value())
            return cond_r;
        bool taken = !std::holds_alternative<bool>(cond_r.value()) ||
                     std::get<bool>(cond_r.value());
        auto const &branch = taken ? cif.consequent : cif.alternative;
        return cps_dispatch<MaxNodes, MaxList>(arena.get(branch), arena, cont,
                                               env, k);
    }

    if (std::holds_alternative<core_quote>(node.inner)) {
        auto const &cq = std::get<core_quote>(node.inner);
        value<Core> v;
        if (std::holds_alternative<int>(cq.atom))
            v = value<Core>{std::get<int>(cq.atom)};
        else if (std::holds_alternative<bool>(cq.atom))
            v = value<Core>{std::get<bool>(cq.atom)};
        else
            v = value<Core>{symbol{std::get<std::string_view>(cq.atom)}};
        auto r = cont(v);
        if (!r.has_value())
            return r;
        return k(r.value());
    }

    if (std::holds_alternative<core_lambda<Core, MaxNodes, MaxList>>(
            node.inner)) {
        value<Core> v{
            closure<Core>{&node, constexpr_box<schemepoc::env<Core, 16>>{
                                     new schemepoc::env<Core, 16>{env}}}};
        auto r = cont(v);
        if (!r.has_value())
            return r;
        return k(r.value());
    }

    if (std::holds_alternative<core_application<Core, MaxNodes, MaxList>>(
            node.inner)) {
        auto const &app =
            std::get<core_application<Core, MaxNodes, MaxList>>(node.inner);

        auto func_r = cps_dispatch<MaxNodes, MaxList>(arena.get(app.func),
                                                      arena, identity_k<Core>{},
                                                      env, identity_k<Core>{});
        if (!func_r.has_value())
            return func_r;

        if (std::holds_alternative<builtin>(func_r.value())) {
            auto const &bi = std::get<builtin>(func_r.value());
            if (app.args.size() != 2)
                return result<value<Core>>{parse_error{{}, "arity mismatch"}};

            auto arg0_r = cps_dispatch<MaxNodes, MaxList>(
                arena.get(app.args[0]), arena, identity_k<Core>{}, env,
                identity_k<Core>{});
            if (!arg0_r.has_value())
                return arg0_r;
            if (!std::holds_alternative<int>(arg0_r.value()))
                return result<value<Core>>{parse_error{{}, "type error"}};
            int a = std::get<int>(arg0_r.value());

            auto arg1_r = cps_dispatch<MaxNodes, MaxList>(
                arena.get(app.args[1]), arena, identity_k<Core>{}, env,
                identity_k<Core>{});
            if (!arg1_r.has_value())
                return arg1_r;
            if (!std::holds_alternative<int>(arg1_r.value()))
                return result<value<Core>>{parse_error{{}, "type error"}};
            int b = std::get<int>(arg1_r.value());

            value<Core> app_val;
            if (bi.op == builtin_op::add)
                app_val = value<Core>{a + b};
            else
                app_val = value<Core>{a * b};

            auto r = cont(app_val);
            if (!r.has_value())
                return r;
            return k(r.value());
        }

        if (std::holds_alternative<closure<Core>>(func_r.value())) {
            auto const &clo = std::get<closure<Core>>(func_r.value());
            auto const &lam_node = *clo.node;

            if (!std::holds_alternative<core_lambda<Core, MaxNodes, MaxList>>(
                    lam_node.inner))
                return result<value<Core>>{parse_error{{}, "type error"}};
            auto const &lam =
                std::get<core_lambda<Core, MaxNodes, MaxList>>(lam_node.inner);
            if (app.args.size() != lam.params.size())
                return result<value<Core>>{parse_error{{}, "arity mismatch"}};

            auto new_env = clo.captured ? *clo.captured : env;
            for (std::size_t i = 0; i < app.args.size(); ++i) {
                auto arg_r = cps_dispatch<MaxNodes, MaxList>(
                    arena.get(app.args[i]), arena, identity_k<Core>{}, env,
                    identity_k<Core>{});
                if (!arg_r.has_value())
                    return arg_r;
                new_env.define(lam.params[i], arg_r.value());
            }
            return cps_dispatch<MaxNodes, MaxList>(arena.get(lam.body), arena,
                                                   cont, new_env, k);
        }

        return result<value<Core>>{
            parse_error{{}, "attempted to call non-function"}};
    }
    return result<value<Core>>{
        parse_error{{}, "cps_dispatch: unsupported form"}};
}

} // namespace detail

template <int MaxNodes, int MaxList, class Cont>
[[nodiscard]] constexpr auto
cps_of(core_type<MaxNodes, MaxList> const &node,
       tree_arena<core_type<MaxNodes, MaxList>, MaxNodes> arena, Cont cont) {
    using Core = core_type<MaxNodes, MaxList>;
    return cps_code{[node, arena, cont](auto const &env, auto k) constexpr {
        return detail::cps_dispatch<MaxNodes, MaxList>(node, arena, cont, env,
                                                       k);
    }};
}

template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
compile_cps(core_type<MaxNodes, MaxList> const &node,
            tree_arena<core_type<MaxNodes, MaxList>, MaxNodes> arena) {
    using Core = core_type<MaxNodes, MaxList>;
    return cps_of<MaxNodes, MaxList>(node, arena, detail::identity_k<Core>{});
}

} // namespace smd::schemepoc

#endif
