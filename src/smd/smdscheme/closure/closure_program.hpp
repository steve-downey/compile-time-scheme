// src/smd/smdscheme/closure/closure_program.hpp -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_CLOSURE_CLOSURE_PROGRAM_HPP
#define SRC_SMD_SMDSCHEME_CLOSURE_CLOSURE_PROGRAM_HPP

#include <smd/smdscheme/closure/cps_code.hpp>
#include <smd/smdscheme/closure/value.hpp>
#include <smd/smdscheme/elaborator/elaborate.hpp>
#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/parser/cursor.hpp>
#include <smd/smdscheme/reader/read_datum.hpp>

#include <string_view>

namespace smd::smdscheme::closure {

/// A compiled Scheme program that can be called with an environment.
///
/// Wraps a @ref cps::cps_code so it can be invoked with just an environment,
/// using the identity continuation as the outermost @p k.  The type
/// @c CpsCode is deduced from the output of @ref cps::compile_cps.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum argument/list length.
/// @tparam CpsCode  The concrete CPS code type produced by @ref
/// cps::compile_cps.
template <int MaxNodes, int MaxList, class CpsCode>
struct closure_program {
    using Core = elaborator::core_type<MaxNodes, MaxList>;

    CpsCode code; ///< The compiled CPS representation.

    /// Evaluates the program in @p environment.
    ///
    /// @tparam MaxBindings Environment capacity.
    /// @param  environment The variable bindings to evaluate under.
    /// @return The final value or a @ref foundation::parse_error.
    template <int MaxBindings>
    constexpr auto operator()(env<Core, MaxBindings> const &environment) const
        -> foundation::result<value<Core>> {
        return code(environment, cps::detail::identity_k<Core>{});
    }
};

/// Compiles a Scheme source string to a callable @ref closure_program.
///
/// Chains the full pipeline: read → elaborate → compile CPS. Any stage
/// failure propagates as a @ref foundation::parse_error wrapped in the
/// returned @ref foundation::result.
///
/// The returned program is purely functional — it holds the compiled CPS
/// tree by value and can be called multiple times with different environments.
///
/// @tparam MaxNodes Arena capacity (default 32).
/// @tparam MaxList  Maximum argument/list length (default 16).
/// @param  src      The Scheme source to compile.
/// @return A @c result<closure_program<...>> holding the callable on success.
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
        ProgramT{cps::compile_cps<MaxNodes, MaxList>(ct_root, core_arena)}};
}

} // namespace smd::smdscheme::closure

#endif
