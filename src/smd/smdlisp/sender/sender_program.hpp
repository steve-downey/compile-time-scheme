// src/smd/smdlisp/sender/sender_program.hpp                      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_SENDER_SENDER_PROGRAM_HPP
#define SRC_SMD_SMDLISP_SENDER_SENDER_PROGRAM_HPP

#include <smd/smdlisp/closure/env.hpp>
#include <smd/smdlisp/closure/value.hpp>
#include <smd/smdlisp/elaborator/elaborate.hpp>
#include <smd/smdlisp/reader/read_datum.hpp>
#include <smd/smdlisp/sender/outcome.hpp>
#include <smd/smdlisp/sender/sender_eval.hpp>
#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/parser/cursor.hpp>

#include <string_view>

namespace smd::smdlisp::sender {

/// A compiled `smdlisp` program evaluated over Beman Execution senders.
///
/// The sender twin of @ref closure::closure_program, and it inherits that
/// type's caller-owns-the-datum-arena contract for the same reason (DIV-0007):
/// elaborated core nodes hold `std::string_view` fields viewing into the datum
/// atom data, so the datum arena has to outlive every call.
///
/// Unlike the closure programs, this one is **runtime-only**.  Beman's
/// `connect`/`start` machinery is not constant-evaluable, so the sender
/// backend cannot carry the `static_assert` twins that are the L11-L15 merge
/// criteria in `eval_direct.test.cpp` and `cps_code.test.cpp`; see DIV-0015.
///
/// @tparam MaxNodes    Arena capacity.
/// @tparam MaxList     Maximum argument/list length.
/// @tparam MaxBindings Environment capacity; must be 16.
/// @tparam MaxEnvs     Capacity of the closure-capture env_arena.
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs>
struct sender_program {
    using Core = elaborator::core_type<MaxNodes, MaxList>;

    Core root; ///< The elaborated root expression.
    smd::smdscheme::foundation::tree_arena<Core, MaxNodes>
        arena; ///< Owns all sub-nodes.

    /// Evaluates the program, returning the raw three-channel completion.
    ///
    /// This is the accessor to use when the *channel* is the thing under
    /// test -- when a caller wants to distinguish "unwound" from "was
    /// diagnosed" rather than merely "did not produce a value".
    [[nodiscard]] auto
    run(closure::env<Core, MaxBindings> &environment,
        closure::env_arena<Core, MaxBindings, MaxEnvs> &envs) const
        -> outcome<Core> {
        eval_context<MaxNodes, MaxList, MaxBindings, MaxEnvs> const ctx{
            &arena, &environment, &envs};
        return eval_node<MaxNodes, MaxList, MaxBindings, MaxEnvs>(root, ctx);
    }

    /// Evaluates the program and projects the completion onto the closure
    /// backends' @c result, so parity with them can be asserted directly.
    [[nodiscard]] auto
    operator()(closure::env<Core, MaxBindings> &environment,
               closure::env_arena<Core, MaxBindings, MaxEnvs> &envs) const
        -> smd::smdscheme::foundation::result<closure::value<Core>> {
        return to_result<Core>(run(environment, envs));
    }
};

/// Compiles an `smdlisp` source string to a callable @ref sender_program.
///
/// Chains read -> elaborate; evaluation happens at call time.  Mirrors
/// @ref closure::compile_to_closure's signature, including the caller-owned
/// @p datum_arena.
///
/// @tparam MaxNodes    Arena capacity (default 128).
/// @tparam MaxList     Maximum argument/list length (default 16).
/// @tparam MaxBindings Environment capacity; must be 16 (default 16).
/// @tparam MaxEnvs     Capacity of the closure-capture env_arena (default 32).
/// @param  src         The `smdlisp` source to compile.
/// @param  datum_arena Caller-owned storage for the read datum tree; must
///                      outlive every call to the returned program.
template <int MaxNodes = 128, int MaxList = 16, int MaxBindings = 16,
          int MaxEnvs = 32>
[[nodiscard]] auto compile_to_sender(
    std::string_view src,
    smd::smdscheme::foundation::tree_arena<
        reader::datum_type<MaxNodes, MaxList>, MaxNodes> &datum_arena) {
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    using ProgramT = sender_program<MaxNodes, MaxList, MaxBindings, MaxEnvs>;
    using Res = smd::smdscheme::foundation::result<ProgramT>;

    auto dr = reader::read_datum<MaxNodes, MaxList>(
        smd::smdscheme::parser::cursor{src}, datum_arena);
    if (!dr.has_value())
        return Res{dr.error()};
    smd::smdscheme::foundation::tree_arena<Core, MaxNodes> core_arena;
    auto er = elaborator::elaborate<MaxNodes, MaxList>(dr.value().value,
                                                       datum_arena, core_arena);
    if (!er.has_value())
        return Res{er.error()};
    return Res{ProgramT{er.value(), core_arena}};
}

} // namespace smd::smdlisp::sender

#endif
