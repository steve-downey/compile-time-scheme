// src/smd/smdscheme/closure/cps_code.hpp                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_CLOSURE_CPS_CODE_HPP
#define SRC_SMD_SMDSCHEME_CLOSURE_CPS_CODE_HPP

#include <smd/smdscheme/closure/value.hpp>
#include <smd/smdscheme/elaborator/elaborate.hpp>
#include <smd/smdscheme/foundation/result.hpp>

#include <variant>

namespace smd::smdscheme::cps {

/// A callable CPS representation of a compiled Scheme expression.
///
/// @c cps_code<F> wraps a callable @p F with signature
/// @code
///   (Env const&, K) -> result<value<Core>>
/// @endcode
/// where @p K is the outermost continuation.  Callers pass an environment
/// and a continuation; the code evaluates the expression and invokes the
/// continuation with the resulting value.
///
/// @tparam F The underlying callable type (deduced via the deduction guide).
// 715f2954-9e34-4da8-a339-c7d810a82f70
template <class F>
struct cps_code {
    F f;

    /// Evaluates the CPS expression in @p env, invoking continuation @p k.
    template <class Env, class K>
    constexpr auto operator()(Env const &env, K k) const {
        return f(env, k);
    }
};
// 715f2954-9e34-4da8-a339-c7d810a82f70 end

/// Deduction guide: @c cps_code(f) deduces @c cps_code<F>.
template <class F>
cps_code(F) -> cps_code<F>;

namespace detail {

/// The identity continuation: returns its argument unchanged.
///
/// Used as the terminal continuation in @ref cps_of and @ref compile_cps
/// to extract the final value.
///
/// @tparam Core Core AST type.
template <class Core>
struct identity_k {
    constexpr auto operator()(closure::value<Core> v) const
        -> foundation::result<closure::value<Core>> {
        return v;
    }
};

/// Internal CPS dispatch over a core AST node.
///
/// Evaluates @p node with outer continuation @p cont and inner continuation
/// @p k.  On success the value is passed to @p cont; if @p cont succeeds
/// its result is passed to @p k.  This two-continuation style allows the
/// caller to intercept intermediate values without restructuring the
/// recursion.
///
/// Handles: integer, boolean, symbol, @c if, quote, lambda, application
/// (builtin, closure, and foreign-function dispatch).
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum argument/list length.
/// @tparam Cont     Intermediate continuation type.
/// @tparam Env      Environment type.
/// @tparam K        Outer continuation type.
/// @param  node  Node to evaluate.
/// @param  arena Core arena.
/// @param  cont  Intermediate continuation applied to the node's value.
/// @param  env   Current variable bindings.
/// @param  k     Outer continuation applied to @c cont's result.
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
        // 9a78bc9b-37eb-4b80-bd27-6146c24f6244
        closure::value<Core> v{closure::closure<Core>{
            &node, closure::constexpr_box<closure::env<Core, 16>>{
                       new closure::env<Core, 16>{env}}}};
        auto r = cont(v);
        if (!r.has_value())
            return r;
        return k(r.value());
        // 9a78bc9b-37eb-4b80-bd27-6146c24f6244 end
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
            // 7d7514b6-5dcb-41d9-8938-5f539e66688f
            return cps_dispatch<MaxNodes, MaxList>(arena.get(lam.body), arena,
                                                   cont, new_env, k);
            // 7d7514b6-5dcb-41d9-8938-5f539e66688f end
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

/// Wraps a core node and its arena in a @ref cps_code with an explicit outer
/// continuation @p cont.
///
/// The returned code, when called with an environment and a final continuation
/// @p k, evaluates @p node, passes the result to @p cont, then passes @p cont's
/// result to @p k.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum argument/list length.
/// @tparam Cont     Intermediate continuation type.
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

/// Compiles a core node to a @ref cps_code using the identity as the outermost
/// intermediate continuation.
///
/// The resulting code evaluates the expression and returns the value directly
/// to the final continuation.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum argument/list length.
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
