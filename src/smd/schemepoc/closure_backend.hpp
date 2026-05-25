// src/smd/schemepoc/closure_backend.hpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SCHEMEPOC_CLOSURE_BACKEND_HPP
#define SRC_SMD_SCHEMEPOC_CLOSURE_BACKEND_HPP

#include <smd/schemepoc/cps.hpp>
#include <smd/schemepoc/elaborator.hpp>
#include <smd/schemepoc/reader.hpp>
#include <smd/schemepoc/result.hpp>
#include <smd/schemepoc/value.hpp>

#include <string_view>

namespace smd::schemepoc {

// closure_program: a callable compiled object.
// Wraps a cps_code so it can be invoked with just an environment,
// using the identity as the outermost continuation.
template <class CpsCode>
struct closure_program {
    CpsCode code;

    template <int MaxBindings>
    constexpr auto operator()(env<MaxBindings> const &environment) const
        -> result<value> {
        return code(environment, detail::identity_k{});
    }
};

template <class CpsCode>
closure_program(CpsCode) -> closure_program<CpsCode>;

// compile_to_closure: full pipeline from source string to callable closure.
// Chains read -> elaborate -> compile_cps.
// Returns result<closure_program<...>>; error propagates from any stage.
template <int MaxNodes = 32, int MaxList = 16>
[[nodiscard]] constexpr auto compile_to_closure(std::string_view src) {
    using CT       = core_tree<MaxNodes, MaxList>;
    using CpsCodeT = decltype(compile_cps(std::declval<CT const &>(),
                                          std::declval<node_id>()));
    using ProgramT = closure_program<CpsCodeT>;

    auto dr = read_datum<MaxNodes, MaxList>(cursor{src});
    if (!dr.has_value())
        return result<ProgramT>{dr.error()};
    auto er = elaborate(dr.value().value);
    if (!er.has_value())
        return result<ProgramT>{er.error()};
    auto const &ct = er.value();
    return result<ProgramT>{ProgramT{compile_cps(ct, ct.size() - 1)}};
}

} // namespace smd::schemepoc

#endif
