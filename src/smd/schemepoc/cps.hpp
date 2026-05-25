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

// Fixed-type identity continuation: avoids spawning new lambda types on each
// recursive cps_dispatch instantiation and keeps template depth bounded.
struct identity_k {
    constexpr auto operator()(value v) const -> result<value> { return v; }
};

// cps_dispatch(core, id, cont, env, k)
//   Evaluates node `id` structurally, threads cont and k through.
//   cont: value -> result<value> — applied to the node's final value.
//   k:    value -> result<value> — the outermost continuation.
//
// Two template instantiations exist in practice:
//   1. <Cont, K> for the outermost call
//   2. <identity_k, identity_k> for all intermediate sub-expression evaluations
// Neither causes circular template instantiation.
template <int MaxNodes, int MaxList, class Cont, class Env, class K>
constexpr auto cps_dispatch(core_tree<MaxNodes, MaxList> const &core,
                            node_id id, Cont const &cont, Env const &env,
                            K const &k) -> result<value> {
    auto const &node = core.get(id);

    if (std::holds_alternative<core_integer>(node)) {
        auto r = cont(value{std::get<core_integer>(node).value});
        if (!r.has_value())
            return r;
        return k(r.value());
    }

    if (std::holds_alternative<core_boolean>(node)) {
        auto r = cont(value{std::get<core_boolean>(node).value});
        if (!r.has_value())
            return r;
        return k(r.value());
    }

    if (std::holds_alternative<core_symbol>(node)) {
        auto lr = env.lookup(std::get<core_symbol>(node).name);
        if (!lr.has_value())
            return result<value>{lr.error()};
        auto r = cont(lr.value());
        if (!r.has_value())
            return r;
        return k(r.value());
    }

    if (std::holds_alternative<core_if>(node)) {
        auto const &cif = std::get<core_if>(node);
        auto cond_r =
            cps_dispatch(core, cif.condition, identity_k{}, env, identity_k{});
        if (!cond_r.has_value())
            return cond_r;
        bool taken = !std::holds_alternative<bool>(cond_r.value()) ||
                     std::get<bool>(cond_r.value());
        node_id branch = taken ? cif.consequent : cif.alternative;
        return cps_dispatch(core, branch, cont, env, k);
    }

    if (std::holds_alternative<core_quote>(node)) {
        auto const &cq = std::get<core_quote>(node);
        value v;
        if (std::holds_alternative<int>(cq.atom))
            v = value{std::get<int>(cq.atom)};
        else if (std::holds_alternative<bool>(cq.atom))
            v = value{std::get<bool>(cq.atom)};
        else
            v = value{symbol{std::get<std::string_view>(cq.atom)}};

        auto r = cont(v);
        if (!r.has_value())
            return r;
        return k(r.value());
    }

    if (std::holds_alternative<core_lambda<MaxList>>(node)) {
        value v{closure{id, constexpr_box<schemepoc::env<16>>{
                                new schemepoc::env<16>{env}}}};
        auto r = cont(v);
        if (!r.has_value())
            return r;
        return k(r.value());
    }

    if (std::holds_alternative<core_application<MaxList>>(node)) {
        auto const &app = std::get<core_application<MaxList>>(node);

        auto func_r =
            cps_dispatch(core, app.func, identity_k{}, env, identity_k{});
        if (!func_r.has_value())
            return func_r;

        if (std::holds_alternative<builtin>(func_r.value())) {
            auto const &bi = std::get<builtin>(func_r.value());

            if (app.args.size() != 2)
                return result<value>{parse_error{{}, "arity mismatch"}};

            auto arg0_r = cps_dispatch(core, app.args[0], identity_k{}, env,
                                       identity_k{});
            if (!arg0_r.has_value())
                return arg0_r;
            if (!std::holds_alternative<int>(arg0_r.value()))
                return result<value>{parse_error{{}, "type error"}};
            int a = std::get<int>(arg0_r.value());

            auto arg1_r = cps_dispatch(core, app.args[1], identity_k{}, env,
                                       identity_k{});
            if (!arg1_r.has_value())
                return arg1_r;
            if (!std::holds_alternative<int>(arg1_r.value()))
                return result<value>{parse_error{{}, "type error"}};
            int b = std::get<int>(arg1_r.value());

            value app_val;
            if (bi.op == builtin_op::add)
                app_val = value{a + b};
            else
                app_val = value{a * b};

            auto r = cont(app_val);
            if (!r.has_value())
                return r;
            return k(r.value());
        }

        if (std::holds_alternative<closure>(func_r.value())) {
            auto const &clo = std::get<closure>(func_r.value());
            auto const &lam_node = core.get(clo.node);

            if (!std::holds_alternative<core_lambda<MaxList>>(lam_node))
                return result<value>{parse_error{{}, "type error"}};

            auto const &lam = std::get<core_lambda<MaxList>>(lam_node);
            if (app.args.size() != lam.params.size())
                return result<value>{parse_error{{}, "arity mismatch"}};

            auto new_env = clo.captured ? *clo.captured : env;
            for (std::size_t i = 0; i < app.args.size(); ++i) {
                auto arg_r = cps_dispatch(core, app.args[i], identity_k{}, env,
                                          identity_k{});
                if (!arg_r.has_value())
                    return arg_r;
                new_env.define(lam.params[i], arg_r.value());
            }

            return cps_dispatch(core, lam.body, cont, new_env, k);
        }

        return result<value>{parse_error{{}, "attempted to call non-function"}};
    }

    return result<value>{parse_error{{}, "cps_dispatch: unsupported form"}};
}

} // namespace detail

// cps_of(core, id, cont)
//   Returns a cps_code whose operator()(env, k) evaluates node `id` in CPS,
//   passes the result to cont, then passes cont's result to k.
//   cont is the current (inner) continuation; k is the outer continuation
//   supplied at call-time.  For the outermost call, cont is the identity.
template <int MaxNodes, int MaxList, class Cont>
[[nodiscard]] constexpr auto cps_of(core_tree<MaxNodes, MaxList> const &core,
                                    node_id id, Cont cont) {
    return cps_code{[core, id, cont](auto const &env, auto k) constexpr {
        return detail::cps_dispatch(core, id, cont, env, k);
    }};
}

template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
compile_cps(core_tree<MaxNodes, MaxList> const &core, node_id root) {
    return cps_of(core, root, detail::identity_k{});
}

} // namespace smd::schemepoc

#endif
