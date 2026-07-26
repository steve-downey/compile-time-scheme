// src/smd/smdlisp/smdlisp.hpp                                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_SMDLISP_HPP
#define SRC_SMD_SMDLISP_SMDLISP_HPP

#include <smd/smdlisp/closure/closure_program.hpp>
#include <smd/smdlisp/closure/env.hpp>
#include <smd/smdlisp/closure/value.hpp>
#include <smd/smdlisp/elaborator/elaborated_core.hpp>
#include <smd/smdlisp/reader/datum_type.hpp>
#include <smd/smdscheme/foundation/arena_box.hpp>
#include <smd/smdscheme/foundation/result.hpp>

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>

namespace smd::smdlisp {

// 54b5ce61-30e2-448d-b830-933d229b999e
/// A compile-time string literal that can be used as a non-type template
/// parameter.
///
/// Stores a null-terminated copy of the source string in a @c char array so
/// that the string content participates in template argument deduction and
/// becomes part of the type identity.
///
/// Adapted by copy from `smd::smdscheme::source_literal`, which it mirrors
/// exactly. The copy is deliberate and is recorded as DIV-0012
/// (`docs/divergences/`): the Scheme original lives in the *package* header
/// `smd/smdscheme/smdscheme.hpp`, not in the language-agnostic `foundation`
/// target, so aliasing it would make every `smdlisp` consumer depend on the
/// whole Scheme compiler (and inherit `smdscheme.closure`'s INTERFACE
/// `-freflection`); and `smdscheme` is frozen for edits (decision D1), so
/// the type cannot be moved down into `foundation` where reuse would be
/// free.
///
/// @tparam N Length of the string literal, including the null terminator.
template <std::size_t N>
struct source_literal {
    char text[N]{}; ///< Null-terminated copy of the source string.

    /// Constructs from a string literal; copies all @p N characters.
    constexpr source_literal(char const (&input)[N]) {
        std::copy_n(input, N, text);
    }

    /// Returns a @c string_view over the stored text, excluding the null
    /// terminator.
    [[nodiscard]] constexpr auto view() const -> std::string_view {
        return {text, N - 1};
    }
};
// 54b5ce61-30e2-448d-b830-933d229b999e end

// fe65137c-94d0-4c11-bc1f-b0dae61e2190
/// An `smdlisp` source literal compiled to a callable program, together with
/// the datum arena that program's `string_view` fields point into.
///
/// This is the piece `smd::smdscheme` does not need. There,
/// `compile_to_closure` is self-contained and `compiled_closure` can be a
/// one-liner. Here, `closure::compile_to_closure` takes a *caller-owned*
/// datum arena that must outlive the program, because elaborated core nodes
/// hold plain `std::string_view` fields viewing the datum atoms their names
/// were read from (DIV-0007). A variable template alone has nowhere to put
/// that arena, so this class supplies the home: @ref datum_arena_ is a data
/// member declared before @ref program_, hence initialized first, and the
/// constructor compiles into it. Every `string_view` inside @ref program_
/// therefore points into a subobject of *this* object and stays valid for
/// exactly as long as the program does.
///
/// The same aliasing is why this type is neither copyable nor movable:
/// copying it would copy @ref datum_arena_ to a new address while leaving
/// @ref program_'s views pointing at the old one. Guaranteed copy elision
/// makes `inline constexpr auto p = lisp_program<"...">{};` work regardless
/// (the prvalue initializes the variable directly), which is all
/// @ref compiled_lisp needs.
///
/// A source string that does not read or elaborate is a compile-time error:
/// `result::value()` is `std::get` on the error alternative, which throws,
/// and a throw is not a constant expression.
///
/// @tparam Source      The `smdlisp` source literal, deduced from the NTTP.
/// @tparam MaxNodes    Arena capacity.
/// @tparam MaxList     Maximum argument/list length.
/// @tparam MaxBindings Environment capacity; must be 16 (see
///                      `closure/cps_code.hpp`).
/// @tparam MaxEnvs     Capacity of the closure-capture `env_arena`.
template <source_literal Source, int MaxNodes = 128, int MaxList = 16,
          int MaxBindings = 16, int MaxEnvs = 32>
class lisp_program {
  public:
    /// Storage for the read datum tree, owned by composition here rather
    /// than by the caller (DIV-0007).
    using datum_arena_type = smd::smdscheme::foundation::tree_arena<
        reader::datum_type<MaxNodes, MaxList>, MaxNodes>;

  private:
    /// Must be declared before @ref program_ so it is initialized first.
    datum_arena_type datum_arena_{};

  public:
    /// The core AST type this program was elaborated into.
    using core_type = elaborator::core_type<MaxNodes, MaxList>;
    /// The runtime value type the program evaluates to.
    using value_type = closure::value<core_type>;
    /// The environment type @ref operator() must be called with.
    using env_type = closure::env<core_type, MaxBindings>;
    /// The closure-capture arena type @ref operator() must be called with.
    using env_arena_type = closure::env_arena<core_type, MaxBindings, MaxEnvs>;
    /// The pair heap type a `cons`-using program's environment needs.
    using pair_heap_type =
        closure::pair_heap<core_type, closure::default_max_pairs>;
    /// The variable store type a `setq`-using program's environment needs.
    using store_type = closure::store<core_type, closure::default_max_store>;
    /// The type @ref operator() returns.
    using result_type = smd::smdscheme::foundation::result<value_type>;

    /// Reads, elaborates, and CPS-compiles @c Source into this object.
    constexpr lisp_program();

    lisp_program(lisp_program const &) = delete;
    auto operator=(lisp_program const &) -> lisp_program & = delete;

    /// Evaluates the program under @p environment, using @p envs to own any
    /// closures it materializes.
    ///
    /// @param environment Bindings to evaluate under; mutated in place by
    ///                     `setq`/`defun`/`defvar`/`defparameter`.
    /// @param envs        Caller-owned storage for captured environments.
    [[nodiscard]] constexpr auto operator()(env_type &environment,
                                            env_arena_type &envs) const
        -> result_type;

  private:
    using program_type = std::remove_cvref_t<
        decltype(closure::compile_to_closure<MaxNodes, MaxList, MaxBindings,
                                             MaxEnvs>(
                     std::declval<std::string_view>(),
                     std::declval<datum_arena_type &>())
                     .value())>;

    program_type program_;
};

template <source_literal Source, int MaxNodes, int MaxList, int MaxBindings,
          int MaxEnvs>
constexpr lisp_program<Source, MaxNodes, MaxList, MaxBindings,
                       MaxEnvs>::lisp_program()
    : program_{
          closure::compile_to_closure<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
              Source.view(), datum_arena_)
              .value()} {}

template <source_literal Source, int MaxNodes, int MaxList, int MaxBindings,
          int MaxEnvs>
constexpr auto
lisp_program<Source, MaxNodes, MaxList, MaxBindings, MaxEnvs>::operator()(
    env_type &environment, env_arena_type &envs) const -> result_type {
    return program_(environment, envs);
}

/// A `constexpr` variable template that compiles an `smdlisp` source literal
/// to a callable @ref lisp_program at translation time.
///
/// Mirrors `smd::smdscheme::compiled_closure`. The one visible difference is
/// the call signature: an `smdlisp` program is called with a *mutable*
/// environment and a closure-capture arena, both caller-owned, because
/// `setq`/`defun`/`defvar` mutate the environment in place and a captured
/// environment needs a stable home (`closure/env.hpp`).
///
/// Usage:
/// @code
///   using prog = decltype(compiled_lisp<"(+ 1 2)">);
///   prog::env_arena_type envs;
///   auto env = closure::default_env<prog::core_type, 16>();
///   auto result = compiled_lisp<"(+ 1 2)">(env, envs);
/// @endcode
///
/// @tparam Source The `smdlisp` source literal, deduced from the NTTP.
template <source_literal Source>
inline constexpr auto compiled_lisp = lisp_program<Source>{};
// fe65137c-94d0-4c11-bc1f-b0dae61e2190 end

} // namespace smd::smdlisp

#endif // SRC_SMD_SMDLISP_SMDLISP_HPP
