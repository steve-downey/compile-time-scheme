// src/smd/schemepoc/cps.hpp                                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SCHEMEPOC_CPS_HPP
#define SRC_SMD_SCHEMEPOC_CPS_HPP

#include <smd/schemepoc/elaborator.hpp>
#include <smd/schemepoc/eval_direct.hpp>
#include <smd/schemepoc/result.hpp>
#include <smd/schemepoc/value.hpp>

namespace smd::schemepoc {

template <class F>
struct cps_code {
    F f;

    template <class Env, class K>
    constexpr auto operator()(Env const& env, K k) const {
        return f(env, k);
    }
};

template <class F>
cps_code(F) -> cps_code<F>;

template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto compile_cps(
    core_tree<MaxNodes, MaxList> const& core,
    node_id                             root) {
    return cps_code{[core, root](auto const& env, auto k) constexpr {
        auto v = eval_direct(core, root, env);
        if (!v.has_value())
            return v;
        return k(v.value());
    }};
}

} // namespace smd::schemepoc

#endif
