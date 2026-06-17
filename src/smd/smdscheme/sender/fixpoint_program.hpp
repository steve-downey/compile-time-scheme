// src/smd/smdscheme/sender/fixpoint_program.hpp                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_FIXPOINT_PROGRAM_HPP
#define SRC_SMD_SMDSCHEME_SENDER_FIXPOINT_PROGRAM_HPP

#include <smd/smdscheme/sender/comp_tree.hpp>
#include <smd/smdscheme/sender/fixpoint_eval.hpp>

#include <smd/smdscheme/closure/value.hpp>
#include <smd/smdscheme/elaborator/elaborate.hpp>
#include <smd/smdscheme/foundation/arena_box.hpp>
#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/parser/cursor.hpp>
#include <smd/smdscheme/reader/read_datum.hpp>

#include <string_view>

namespace smd::smdscheme::fixpoint_eval {

/// A compiled Scheme program that evaluates via the Mendler-style interpreter
/// over a Fix<CompF> computation tree.
///
/// The program owns the entire Comp tree by value (children are indirected via
/// Box = std::indirect). When invoked with an environment, it uses mendler_run
/// to evaluate the root computation node.
///
/// Lifetime note: closures created during evaluation store pointers into this
/// tree's root. Do not copy the program after creating closures from it.
///
/// @tparam MaxList  Maximum argument/list length.
template <int MaxList>
struct fixpoint_program {
    using CompT = Comp<MaxList>;

    CompT root; ///< The computation tree root, owned by value.

    /// Evaluates the program in @p environment.
    ///
    /// @tparam MaxBindings Environment capacity.
    /// @param  environment The variable bindings to evaluate under.
    /// @return The final value or a @ref foundation::parse_error.
    template <int MaxBindings>
    constexpr auto operator()(closure::env<CompT, MaxBindings> const &environment) const
        -> foundation::result<closure::value<CompT>> {
        return mendler_run<MaxList, MaxBindings>(root, environment);
    }
};

/// Compiles a Scheme source string to a @ref fixpoint_program.
///
/// Chains the full pipeline: read → elaborate → core_to_comp. Any stage
/// failure propagates as a @ref foundation::parse_error wrapped in the
/// returned @ref foundation::result.
///
/// The returned program is functional — it holds the computation tree by value
/// and can be called multiple times with different environments.
///
/// @tparam MaxNodes Arena capacity (default 32).
/// @tparam MaxList  Maximum argument/list length (default 16).
/// @param  src      The Scheme source to compile.
/// @return A @c result<fixpoint_program<MaxList>> holding the callable on success.
template <int MaxNodes = 32, int MaxList = 16>
[[nodiscard]] constexpr auto compile_fixpoint(std::string_view src)
    -> foundation::result<fixpoint_program<MaxList>> {

    using Core = elaborator::core_type<MaxNodes, MaxList>;
    using ProgramT = fixpoint_program<MaxList>;

    // Read datum from source
    foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>, MaxNodes> arena_dr;
    auto dr = reader::read_datum<MaxNodes, MaxList>(parser::cursor{src}, arena_dr);
    if (!dr.has_value())
        return foundation::result<ProgramT>{dr.error()};

    // Elaborate datum to core AST
    foundation::tree_arena<Core, MaxNodes> core_arena;
    auto er = elaborator::elaborate<MaxNodes, MaxList>(dr.value().value, arena_dr, core_arena);
    if (!er.has_value())
        return foundation::result<ProgramT>{er.error()};

    // Convert core AST to computation tree
    auto comp_r = core_to_comp<MaxNodes, MaxList>(er.value(), core_arena);
    if (!comp_r.has_value())
        return foundation::result<ProgramT>{comp_r.error()};

    return foundation::result<ProgramT>{ProgramT{std::move(comp_r.value())}};
}

} // namespace smd::smdscheme::fixpoint_eval

#endif
