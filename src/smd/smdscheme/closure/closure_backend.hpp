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
template <int MaxNodes, int MaxList, class CpsCode>
struct closure_program {
    using Core = core_type<MaxNodes, MaxList>;
    CpsCode code;

    template <int MaxBindings>
    constexpr auto operator()(env<Core, MaxBindings> const &environment) const
        -> result<value<Core>> {
        return code(environment, detail::identity_k<Core>{});
    }
};

// compile_to_closure: full pipeline from source string to callable closure.
// Chains read -> elaborate -> compile_cps<MaxNodes, MaxList>.
// Returns result<closure_program<...>>; error propagates from any stage.
template <int MaxNodes = 32, int MaxList = 16>
[[nodiscard]] constexpr auto compile_to_closure(std::string_view src) {
    using Core = core_type<MaxNodes, MaxList>;
    using CpsCodeT = decltype(compile_cps<MaxNodes, MaxList>(
        std::declval<Core const &>(),
        std::declval<tree_arena<Core, MaxNodes> const &>()));
    using ProgramT = closure_program<MaxNodes, MaxList, CpsCodeT>;

    tree_arena<datum_type<MaxNodes, MaxList>, MaxNodes> arena_dr;
    auto dr = read_datum<MaxNodes, MaxList>(cursor{src}, arena_dr);
    if (!dr.has_value())
        return result<ProgramT>{dr.error()};
    tree_arena<Core, MaxNodes> core_arena;
    auto er =
        elaborate<MaxNodes, MaxList>(dr.value().value, arena_dr, core_arena);
    if (!er.has_value())
        return result<ProgramT>{er.error()};
    auto const &ct_root = er.value();
    return result<ProgramT>{
        ProgramT{compile_cps<MaxNodes, MaxList>(ct_root, core_arena)}};
}

} // namespace smd::schemepoc

#endif
