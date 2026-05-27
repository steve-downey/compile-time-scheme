// src/smd/smdscheme/closure/closure_backend.hpp                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_CLOSURE_CLOSURE_BACKEND_HPP
#define SRC_SMD_SMDSCHEME_CLOSURE_CLOSURE_BACKEND_HPP

// src/smd/smdscheme/closure/closure_backend.hpp                -*-C++-*-// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception#ifndef SRC_SMD_SMDSCHEME_CLOSURE_CLOSURE_BACKEND_HPP#define SRC_SMD_SMDSCHEME_CLOSURE_CLOSURE_BACKEND_HPP
#include <smd/smdscheme/closure/value.hpp>
#include <smd/smdscheme/closure/cps.hpp>
#include <smd/smdscheme/elaborator/elaborator.hpp>
#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/reader/reader.hpp>

#include <string_view>

namespace smd::smdscheme::closure {

// closure_program: a callable compiled object.
// Wraps a cps_code so it can be invoked with just an environment,
// using the identity as the outermost continuation.
template <int MaxNodes, int MaxList, class CpsCode>
struct closure_program {
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    CpsCode code;

    template <int MaxBindings>
    constexpr auto operator()(env<Core, MaxBindings> const &environment) const
        -> foundation::result<value<Core>> {
        return code(environment, cps::detail::identity_k<Core>{});
    }
};

// compile_to_closure: full pipeline from source string to callable closure.
// Chains read -> elaborator::elaborate -> compile_cps<MaxNodes, MaxList>.
// Returns foundation::result<closure_program<...>>; error propagates from any
// stage.
template <int MaxNodes = 32, int MaxList = 16>
[[nodiscard]] constexpr auto compile_to_closure(std::string_view src) {
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    using CpsCodeT = decltype(cps::compile_cps<MaxNodes, MaxList>(
        std::declval<Core const &>(),
        std::declval<foundation::tree_arena<Core, MaxNodes> const &>()));
    using ProgramT = closure_program<MaxNodes, MaxList, CpsCodeT>;

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
    return foundation::result<ProgramT>{
        ProgramT{compile_cps<MaxNodes, MaxList>(ct_root, core_arena)}};
}

} // namespace smd::smdscheme::closure

#endif
