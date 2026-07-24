// src/smd/smdlisp/closure/closure_program.hpp                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_CLOSURE_CLOSURE_PROGRAM_HPP
#define SRC_SMD_SMDLISP_CLOSURE_CLOSURE_PROGRAM_HPP

#include <smd/smdlisp/closure/cps_code.hpp>
#include <smd/smdlisp/closure/env.hpp>
#include <smd/smdlisp/closure/value.hpp>
#include <smd/smdlisp/elaborator/elaborate.hpp>
#include <smd/smdlisp/reader/read_datum.hpp>
#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/parser/cursor.hpp>

#include <string_view>
#include <utility>

namespace smd::smdlisp::closure {

/// A compiled `smdlisp` program that can be called with an environment.
///
/// Adapted by copy from `smd::smdscheme::closure::closure_program`, named to
/// mirror it directly (step L13). Wraps a @ref cps_code so it can be invoked
/// with an @ref env and an @ref env_arena, using the identity continuation
/// as the outermost @p k. Unlike the Scheme original -- whose `env` is
/// purely functional and passed by `const&` -- both @p environment and @p
/// envs are taken by mutable reference: `setq`/`defun`/`defvar` mutate
/// @p environment in place (step L12), and @p envs is the caller-owned
/// arena that owns every environment a closure captures while the program
/// runs (see `env.hpp`'s `env_arena` docs). Both must outlive the call.
///
/// @tparam MaxNodes    Arena capacity.
/// @tparam MaxList     Maximum argument/list length.
/// @tparam MaxBindings Environment capacity; must be 16 (see
///                      @ref cps_code's doc comment).
/// @tparam MaxEnvs     Capacity of the closure-capture env_arena.
/// @tparam CpsCode     The concrete CPS code type produced by
///                      @ref compile_cps.
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs, class CpsCode>
struct closure_program {
    using Core = elaborator::core_type<MaxNodes, MaxList>;

    CpsCode code; ///< The compiled CPS representation.

    /// Evaluates the program under @p environment, using @p envs to own any
    /// closures the program materializes.
    ///
    /// @param  environment The variable/function bindings and pair heap to
    ///                      evaluate under; mutated in place by `setq`/
    ///                      `defun`/`defvar`/`defparameter`.
    /// @param  envs        Caller-owned storage for captured environments;
    ///                      must outlive every closure this call (or
    ///                      anything it returns) might produce.
    /// @return The final value or a @ref smd::smdscheme::foundation::parse_error.
    constexpr auto operator()(env<Core, MaxBindings> &environment,
                              env_arena<Core, MaxBindings, MaxEnvs> &envs) const
        -> smd::smdscheme::foundation::result<value<Core>> {
        return code(environment, envs, detail::identity_k<Core>{});
    }
};

/// Compiles an `smdlisp` source string to a callable @ref closure_program.
///
/// Named to mirror `smd::smdscheme::closure::compile_to_closure` (step L13),
/// but **not** signature-compatible with it -- see DIV-0007
/// (`docs/divergences/`) for why. Chains the full pipeline: read -> elaborate
/// -> compile CPS. Any stage failure propagates as a
/// @ref smd::smdscheme::foundation::parse_error wrapped in the returned
/// @ref smd::smdscheme::foundation::result.
///
/// **@p datum_arena is caller-owned and must outlive the returned program.**
/// This is the one respect in which this function cannot mirror the Scheme
/// original's single-argument, fully self-contained signature: `smdlisp`'s
/// elaborated core nodes hold plain `std::string_view` fields (`core_lambda`
/// params, `core_function`'s bare-name alternative, `core_setq`/
/// `core_defun`/`core_defvar`'s names) that are views into the *datum* atom
/// data these names were read from -- see `elaborated_core.hpp`'s docs on
/// why those fields are plain `string_view` rather than an owned
/// `reader::folded_name` (`core_symbol`/`core_keyword`, in the ROOT-node
/// case, own their name for the identical reason in reverse; see that file).
/// A `smd::smdscheme::foundation::tree_arena` is index-addressed and
/// therefore safe to copy or return by value (`docs/cps-direction.md`), so
/// @p core_arena is safely function-local here and is copied wholesale into
/// the returned @ref cps_code -- but a `std::string_view` embedded inside a
/// copied core node is still a raw pointer into whatever memory backed the
/// *datum* arena at elaboration time, and copying/moving the object that
/// pointer targets does not update it. Making @p datum_arena a
/// function-local here (as an initial draft of this function did) therefore
/// left every non-trivial program with a dangling `string_view` the instant
/// this function returned -- caught by GCC's constexpr lifetime checker
/// (`accessing 'arena_dr' outside its lifetime`) on this file's own
/// `LambdaApplicationWithCarCdr` test (`'(x)`'s `x` is exactly such a
/// `core_lambda` param). Requiring the caller to own @p datum_arena, exactly
/// as `eval_direct.test.cpp`'s `run`/`run_mut` helpers already require for
/// *their* datum arena, sidesteps the problem instead of fixing its root
/// cause (the elaborator is out of scope for this step).
///
/// The returned program otherwise holds the compiled CPS tree (and its own
/// copy of the core arena) by value and can be called multiple times, each
/// time with a fresh @ref env / @ref env_arena pair supplied by the caller
/// -- unlike the Scheme original, an `smdlisp` @ref env cannot simply be
/// default-constructed and reused across calls if `setq`/`defun`/`defvar`
/// are in play, since those need a backing @ref store (see `env.hpp`'s
/// mutable-mode `default_env` overload) that the caller must own.
///
/// @tparam MaxNodes    Arena capacity (default 128, matching the capacity
///                      `eval_direct.test.cpp`'s end-to-end tests use).
/// @tparam MaxList     Maximum argument/list length (default 16).
/// @tparam MaxBindings Environment capacity; must be 16 (default 16).
/// @tparam MaxEnvs     Capacity of the closure-capture env_arena
///                      (default 32).
/// @param  src         The `smdlisp` source to compile.
/// @param  datum_arena Caller-owned storage for the read datum tree; must
///                      outlive every call to the returned program (see
///                      above).
/// @return A @c result<closure_program<...>> holding the callable on
///         success.
template <int MaxNodes = 128, int MaxList = 16, int MaxBindings = 16, int MaxEnvs = 32>
[[nodiscard]] constexpr auto compile_to_closure(
    std::string_view src,
    smd::smdscheme::foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>, MaxNodes> &datum_arena) {
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    using CpsCodeT = decltype(compile_cps<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
        std::declval<Core const &>(),
        std::declval<smd::smdscheme::foundation::tree_arena<Core, MaxNodes> const &>()));
    using ProgramT = closure_program<MaxNodes, MaxList, MaxBindings, MaxEnvs, CpsCodeT>;

    auto dr = reader::read_datum<MaxNodes, MaxList>(smd::smdscheme::parser::cursor{src}, datum_arena);
    if (!dr.has_value())
        return smd::smdscheme::foundation::result<ProgramT>{dr.error()};
    smd::smdscheme::foundation::tree_arena<Core, MaxNodes> core_arena;
    auto er = elaborator::elaborate<MaxNodes, MaxList>(dr.value().value, datum_arena, core_arena);
    if (!er.has_value())
        return smd::smdscheme::foundation::result<ProgramT>{er.error()};
    auto const &ct_root = er.value();
    return smd::smdscheme::foundation::result<ProgramT>{
        ProgramT{compile_cps<MaxNodes, MaxList, MaxBindings, MaxEnvs>(ct_root, core_arena)}};
}

} // namespace smd::smdlisp::closure

#endif
