// src/smd/smdscheme/sender/sender_mendler_program.hpp            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_SENDER_MENDLER_PROGRAM_HPP
#define SRC_SMD_SMDSCHEME_SENDER_SENDER_MENDLER_PROGRAM_HPP

#include <smd/smdscheme/sender/comp_tree.hpp>
#include <smd/smdscheme/sender/sender_mendler_eval.hpp>

#include <smd/smdscheme/closure/value.hpp>
#include <smd/smdscheme/elaborator/elaborate.hpp>
#include <smd/smdscheme/foundation/arena_box.hpp>
#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/parser/cursor.hpp>
#include <smd/smdscheme/reader/read_datum.hpp>

#include <string_view>

namespace smd::smdscheme::sender_mendler {

/// A compiled Scheme program that evaluates via sender-structured
/// Mendler-style interpretation over a Fix<CompF> computation tree.
///
/// Structurally identical to fixpoint_eval::fixpoint_program, but evaluation
/// uses sender_mendler_run which expresses independent argument evaluations
/// as deferred senders combined with when_all.
template <int MaxList>
struct sender_mendler_program {
    using CompT = fixpoint_eval::Comp<MaxList>;

    CompT root;

    template <int MaxBindings>
    auto operator()(closure::env<CompT, MaxBindings> const &environment) const
        -> foundation::result<closure::value<CompT>> {
        return sender_mendler_run<MaxList, MaxBindings>(root, environment);
    }
};

/// Compiles a Scheme source string to a sender_mendler_program.
///
/// Chains the full pipeline: read -> elaborate -> core_to_comp. Evaluation
/// happens at call time via sender_mendler_run.
template <int MaxNodes = 32, int MaxList = 16>
[[nodiscard]] auto compile_sender_mendler(std::string_view src)
    -> foundation::result<sender_mendler_program<MaxList>> {

    using Core     = elaborator::core_type<MaxNodes, MaxList>;
    using ProgramT = sender_mendler_program<MaxList>;

    foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>, MaxNodes>
        arena_dr;
    auto dr =
        reader::read_datum<MaxNodes, MaxList>(parser::cursor{src}, arena_dr);
    if (!dr.has_value())
        return foundation::result<ProgramT>{dr.error()};

    foundation::tree_arena<Core, MaxNodes> core_arena;
    auto er = elaborator::elaborate<MaxNodes, MaxList>(
        dr.value().value, arena_dr, core_arena);
    if (!er.has_value())
        return foundation::result<ProgramT>{er.error()};

    auto comp_r =
        fixpoint_eval::core_to_comp<MaxNodes, MaxList>(er.value(), core_arena);
    if (!comp_r.has_value())
        return foundation::result<ProgramT>{comp_r.error()};

    return foundation::result<ProgramT>{ProgramT{std::move(comp_r.value())}};
}

} // namespace smd::smdscheme::sender_mendler

#endif
