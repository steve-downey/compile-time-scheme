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

/// A compiled `smdlisp` program that can be called with an environment and
/// a closure-capture arena.
///
/// Wraps a @ref cps_code so it can be invoked with just those two, using
/// the identity continuation as the outermost @p k. The type @c CpsCode is
/// deduced from the output of @ref compile_cps.
///
/// **`operator()` is templated on @p MaxBindings / @p MaxEnvs, not the
/// struct itself** (mirroring `smd::smdscheme::closure::closure_program`'s
/// own `operator()`-level `MaxBindings` template parameter): the
/// underlying `CpsCode` lambda (see @ref cps_of) is already fully generic
/// over the environment/arena types passed at call time, so nothing about
/// compiling a program to CPS needs to fix those capacities in advance --
/// they are chosen by whichever caller constructs the runtime environment
/// and closure-capture arena.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum argument/list/body length.
/// @tparam CpsCode  The concrete CPS code type produced by @ref
///                   compile_cps.
template <int MaxNodes, int MaxList, class CpsCode>
struct closure_program {
    using Core = elaborator::core_type<MaxNodes, MaxList>;

    CpsCode code; ///< The compiled CPS representation.

    /// Evaluates the program under @p environment (mutable -- see @ref
    /// cps_code's docs for why) and @p envs, the shared closure-capture
    /// arena that owns every environment a `lambda`/`function` form
    /// captures while evaluating.
    ///
    /// @tparam MaxBindings Environment capacity (must be 16 -- see
    ///                      `eval_direct.hpp`'s doc comment).
    /// @tparam MaxEnvs     Capacity of @p envs.
    /// @param  environment The variable/function bindings and pair heap
    ///                      to evaluate under.
    /// @param  envs        The shared closure-capture arena.
    /// @return The final value or a @ref
    ///         smd::smdscheme::foundation::parse_error.
    template <int MaxBindings, int MaxEnvs>
    constexpr auto operator()(env<Core, MaxBindings> &environment,
                              env_arena<Core, MaxBindings, MaxEnvs> &envs) const
        -> smd::smdscheme::foundation::result<value<Core>> {
        return code(environment, envs, detail::identity_k<Core>{});
    }
};

/// Compiles an `smdlisp` source string to a callable @ref closure_program.
///
/// Chains the full pipeline: read -> elaborate -> compile CPS. Any stage
/// failure propagates as a @ref smd::smdscheme::foundation::parse_error
/// wrapped in the returned @ref smd::smdscheme::foundation::result.
///
/// **@p datum_arena is a caller-owned, out parameter -- unlike the Scheme
/// original's `compile_to_closure(src)`, which needs no such parameter.**
/// This is a forced divergence, not a style choice (see
/// `docs/divergences/DIV-0007-compile-to-closure-needs-caller-owned-datum-arena.md`):
/// per decision D2, `smdlisp` folds symbol/keyword spellings to uppercase
/// at read time, so `reader::folded_name` OWNS its (folded) storage,
/// unlike the Scheme reader's `datum_symbol.name`, which is a bare view
/// into the original, never-rewritten source text. `core_lambda::params`
/// and `core_function::target`'s `std::string_view` alternative (see
/// `elaborated_core.hpp`'s docs, and L10's handoff note on exactly this
/// reasoning) are views into a datum node's `folded_name` storage --
/// i.e. into @p datum_arena's memory -- and those views are read every
/// time the *compiled* program is later called (looking up a parameter
/// name, resolving an application head), not merely during elaboration.
/// A locally-scoped `datum_arena` (as the Scheme original's equivalent
/// function body uses, since Scheme's views trace back to the
/// caller-owned source string instead) is therefore unsound here: it goes
/// out of scope when this function returns, while the returned program's
/// core nodes still view into it. GCC's constexpr evaluator caught this
/// directly (an "accessing ... outside its lifetime" diagnostic) the
/// first time this function was written with a function-local arena.
/// @p datum_arena must outlive every call to the returned program, not
/// just this function's own body -- exactly the same caller-owned-arena
/// discipline `eval_direct.test.cpp`'s `run()`/`run_mut()` helpers already
/// follow for the very same reason.  @p core_arena, in contrast, does NOT
/// need to outlive this call: @ref compile_cps's @ref cps_of captures its
/// arena argument BY VALUE (a full deep copy) into the returned CPS code,
/// so this function keeps @p core_arena local.
///
/// The returned program is purely functional -- it holds the compiled CPS
/// tree by value and can be called multiple times with different
/// environments (subject to the environment/arena capacities chosen at
/// each call, per @ref closure_program::operator()'s own template
/// parameters).
///
/// **Merge criterion (docs/cl-pivot-plan.md, step L13):** every L11/L12
/// end-to-end `eval_direct`/`elaborate` scenario also passes through this
/// entry point -- see `closure_program.test.cpp`.
///
/// @tparam MaxNodes Arena capacity (default 128, matching the capacity
///                   `eval_direct.test.cpp` and `elaborate.test.cpp` use).
/// @tparam MaxList  Maximum argument/list/body length (default 16).
/// @param  src         The `smdlisp` source to compile.
/// @param  datum_arena Caller-owned; must outlive every call to the
///                      returned program (see above).
/// @return A @c result<closure_program<...>> holding the callable on
///         success.
template <int MaxNodes = 128, int MaxList = 16>
[[nodiscard]] constexpr auto compile_to_closure(
    std::string_view src,
    smd::smdscheme::foundation::tree_arena<
        reader::datum_type<MaxNodes, MaxList>, MaxNodes> &datum_arena) {
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    using CpsCodeT = decltype(compile_cps<MaxNodes, MaxList>(
        std::declval<Core const &>(),
        std::declval<
            smd::smdscheme::foundation::tree_arena<Core, MaxNodes> const &>()));
    using ProgramT = closure_program<MaxNodes, MaxList, CpsCodeT>;

    auto dr = reader::read_datum<MaxNodes, MaxList>(
        smd::smdscheme::parser::cursor{src}, datum_arena);
    if (!dr.has_value())
        return smd::smdscheme::foundation::result<ProgramT>{dr.error()};
    smd::smdscheme::foundation::tree_arena<Core, MaxNodes> core_arena;
    auto er = elaborator::elaborate<MaxNodes, MaxList>(dr.value().value,
                                                       datum_arena, core_arena);
    if (!er.has_value())
        return smd::smdscheme::foundation::result<ProgramT>{er.error()};
    auto const &ct_root = er.value();
    return smd::smdscheme::foundation::result<ProgramT>{
        ProgramT{compile_cps<MaxNodes, MaxList>(ct_root, core_arena)}};
}

} // namespace smd::smdlisp::closure

#endif
