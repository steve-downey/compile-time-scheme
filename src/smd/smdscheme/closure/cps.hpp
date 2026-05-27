// src/smd/smdscheme/closure/cps.hpp                                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_CLOSURE_CPS_HPP
#define SRC_SMD_SMDSCHEME_CLOSURE_CPS_HPP

#include <smd/smdscheme/closure/value.hpp>
#include <smd/smdscheme/elaborator/elaborator.hpp>
#include <smd/smdscheme/foundation/result.hpp>

#include <variant>

namespace smd::smdscheme::cps {

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
    constexpr auto operator()(closure::value<Core> v) const
        -> foundation::result<closure::value<Core>> {
        return v;
    }
};

template <int MaxNodes, int MaxList, class Cont, class Env, class K>
constexpr auto cps_dispatch(
    elaborator::core_type<MaxNodes, MaxList> const &node,
    const foundation::tree_arena<elaborator::core_type<MaxNodes, MaxList>,
                                 MaxNodes> &arena,
    Cont const &cont, Env const &env, K const &k)
    -> foundation::result<
        closure::value<elaborator::core_type<MaxNodes, MaxList>>> {
    using Core = elaborator::core_type<MaxNodes, MaxList>;

    if (std::holds_alternative<elaborator::core_integer>(node.inner)) {
        auto r = cont(closure::value<Core>{
            std::get<elaborator::core_integer>(node.inner).value});
        if (!r.has_value())
            return r;
        return k(r.value());
    }

    if (std::holds_alternative<elaborator::core_boolean>(node.inner)) {
        auto r = cont(closure::value<Core>{
            std::get<elaborator::core_boolean>(node.inner).value});
        if (!r.has_value())
            return r;
        return k(r.value());
    }

    if (std::holds_alternative<elaborator::core_symbol>(node.inner)) {
        auto lr =
            env.lookup(std::get<elaborator::core_symbol>(node.inner).name);
        if (!lr.has_value())
            return foundation::result<closure::value<Core>>{lr.error()};
        auto r = cont(lr.value());
        if (!r.has_value())
            return r;
        return k(r.value());
    }

    if (std::holds_alternative<elaborator::core_if<Core, MaxNodes>>(
            node.inner)) {
        auto const &cif =
            std::get<elaborator::core_if<Core, MaxNodes>>(node.inner);
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

    if (std::holds_alternative<elaborator::core_quote>(node.inner)) {
        auto const &cq = std::get<elaborator::core_quote>(node.inner);
        closure::value<Core> v;
        if (std::holds_alternative<int>(cq.atom))
            v = closure::value<Core>{std::get<int>(cq.atom)};
        else if (std::holds_alternative<bool>(cq.atom))
            v = closure::value<Core>{std::get<bool>(cq.atom)};
        else
            v = closure::value<Core>{
                closure::symbol{std::get<std::string_view>(cq.atom)}};
        auto r = cont(v);
        if (!r.has_value())
            return r;
        return k(r.value());
    }

    if (std::holds_alternative<
            elaborator::core_lambda<Core, MaxNodes, MaxList>>(node.inner)) {
        closure::value<Core> v{closure::closure<Core>{
            &node, closure::constexpr_box<closure::env<Core, 16>>{
                       new closure::env<Core, 16>{env}}}};
        auto r = cont(v);
        if (!r.has_value())
            return r;
        return k(r.value());
    }

    if (std::holds_alternative<
            elaborator::core_application<Core, MaxNodes, MaxList>>(
            node.inner)) {
        auto const &app =
            std::get<elaborator::core_application<Core, MaxNodes, MaxList>>(
                node.inner);

        auto func_r = cps_dispatch<MaxNodes, MaxList>(arena.get(app.func),
                                                      arena, identity_k<Core>{},
                                                      env, identity_k<Core>{});
        if (!func_r.has_value())
            return func_r;

        if (std::holds_alternative<closure::builtin>(func_r.value())) {
            auto const &bi = std::get<closure::builtin>(func_r.value());
            if (app.args.size() != 2)
                return foundation::result<closure::value<Core>>{
                    foundation::parse_error{{}, "arity mismatch"}};

            auto arg0_r = cps_dispatch<MaxNodes, MaxList>(
                arena.get(app.args[0]), arena, identity_k<Core>{}, env,
                identity_k<Core>{});
            if (!arg0_r.has_value())
                return arg0_r;
            if (!std::holds_alternative<int>(arg0_r.value()))
                return foundation::result<closure::value<Core>>{
                    foundation::parse_error{{}, "type error"}};
            int a = std::get<int>(arg0_r.value());

            auto arg1_r = cps_dispatch<MaxNodes, MaxList>(
                arena.get(app.args[1]), arena, identity_k<Core>{}, env,
                identity_k<Core>{});
            if (!arg1_r.has_value())
                return arg1_r;
            if (!std::holds_alternative<int>(arg1_r.value()))
                return foundation::result<closure::value<Core>>{
                    foundation::parse_error{{}, "type error"}};
            int b = std::get<int>(arg1_r.value());

            closure::value<Core> app_val;
            if (bi.op == closure::builtin_op::add)
                app_val = closure::value<Core>{a + b};
            else
                app_val = closure::value<Core>{a * b};

            auto r = cont(app_val);
            if (!r.has_value())
                return r;
            return k(r.value());
        }

        if (std::holds_alternative<closure::closure<Core>>(func_r.value())) {
            auto const &clo = std::get<closure::closure<Core>>(func_r.value());
            auto const &lam_node = *clo.node;

            if (!std::holds_alternative<
                    elaborator::core_lambda<Core, MaxNodes, MaxList>>(
                    lam_node.inner))
                return foundation::result<closure::value<Core>>{
                    foundation::parse_error{{}, "type error"}};
            auto const &lam =
                std::get<elaborator::core_lambda<Core, MaxNodes, MaxList>>(
                    lam_node.inner);
            if (app.args.size() != lam.params.size())
                return foundation::result<closure::value<Core>>{
                    foundation::parse_error{{}, "arity mismatch"}};

            auto new_env = clo.captured ? *clo.captured : env;
            for (int i = 0; i < app.args.size(); ++i) {
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

        if (std::holds_alternative<closure::foreign_function<Core>>(
                func_r.value())) {
            auto const &ff =
                std::get<closure::foreign_function<Core>>(func_r.value());

            foundation::static_vector<closure::value<Core>, MaxNodes>
                evaluated_args;
            for (auto const &arg_id : app.args) {
                auto arg_r = cps_dispatch<MaxNodes, MaxList>(
                    arena.get(arg_id), arena, identity_k<Core>{}, env,
                    identity_k<Core>{});
                if (!arg_r.has_value())
                    return arg_r;
                evaluated_args.push_back(arg_r.value());
            }

            auto ff_r = ff.fn(std::span<closure::value<Core> const>(
                evaluated_args.begin(), evaluated_args.end()));
            if (!ff_r.has_value())
                return foundation::result<closure::value<Core>>{ff_r.error()};

            auto r = cont(ff_r.value());
            if (!r.has_value())
                return r;
            return k(r.value());
        }

        return foundation::result<closure::value<Core>>{
            foundation::parse_error{{}, "attempted to call non-function"}};
    }
    return foundation::result<closure::value<Core>>{
        foundation::parse_error{{}, "cps_dispatch: unsupported form"}};
}

} // namespace detail

template <int MaxNodes, int MaxList, class Cont>
[[nodiscard]] constexpr auto cps_of(
    elaborator::core_type<MaxNodes, MaxList> const &node,
    foundation::tree_arena<elaborator::core_type<MaxNodes, MaxList>, MaxNodes>
        arena,
    Cont cont) {
    return cps_code{[node, arena, cont](auto const &env, auto k) constexpr {
        return detail::cps_dispatch<MaxNodes, MaxList>(node, arena, cont, env,
                                                       k);
    }};
}

template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto compile_cps(
    elaborator::core_type<MaxNodes, MaxList> const &node,
    foundation::tree_arena<elaborator::core_type<MaxNodes, MaxList>, MaxNodes>
        arena) {
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    return cps_of<MaxNodes, MaxList>(node, arena, detail::identity_k<Core>{});
}

} // namespace smd::smdscheme::cps

#endif
