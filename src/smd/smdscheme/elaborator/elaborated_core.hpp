// src/smd/smdscheme/elaborator/elaborated_core.hpp -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_ELABORATOR_ELABORATED_CORE_HPP
#define SRC_SMD_SMDSCHEME_ELABORATOR_ELABORATED_CORE_HPP

#include <smd/smdscheme/foundation/fix.hpp>
#include <smd/smdscheme/reader/datum_type.hpp> // needed for foundation::static_vector and basic types
#include <string_view>
#include <variant>

namespace smd::smdscheme::elaborator {

/// A Scheme integer literal in the elaborated core.
struct core_integer {
    int value;
};

/// A Scheme boolean literal in the elaborated core.
struct core_boolean {
    bool value;
};

/// A Scheme variable reference in the elaborated core.
struct core_symbol {
    std::string_view name; ///< Variable name as a view into the source.
};

/// A quoted atom — a literal integer, boolean, or symbol that is data
/// rather than code at the call site.
struct core_quote {
    std::variant<int, bool, std::string_view> atom;
};

/// A Scheme @c if form with condition, consequent, and alternative branches.
///
/// Each branch is stored as a handle into the core arena.
///
/// @tparam R        Recursive self-reference (the core fix-point type).
/// @tparam MaxNodes Arena capacity.
// bb010057-c0a1-468b-a21c-7ab18af8cd72
template <typename R, int MaxNodes>
struct core_if {
    foundation::arena_box<R, MaxNodes> condition; ///< The test expression.
    foundation::arena_box<R, MaxNodes>
        consequent; ///< Branch taken when truthy.
    foundation::arena_box<R, MaxNodes>
        alternative; ///< Branch taken when false.
};
// bb010057-c0a1-468b-a21c-7ab18af8cd72 end

/// A Scheme @c lambda form with a fixed parameter list and a single-expression
/// body.
///
/// @tparam R        Recursive self-reference.
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum number of formal parameters.
template <typename R, int MaxNodes, int MaxList>
struct core_lambda {
    foundation::static_vector<std::string_view, MaxList>
        params;                              ///< Formal parameter names.
    foundation::arena_box<R, MaxNodes> body; ///< Body expression.
};

/// A function application: a function expression applied to argument
/// expressions.
///
/// @tparam R        Recursive self-reference.
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum number of arguments.
template <typename R, int MaxNodes, int MaxList>
struct core_application {
    foundation::arena_box<R, MaxNodes> func; ///< The function expression.
    foundation::static_vector<foundation::arena_box<R, MaxNodes>, MaxList>
        args; ///< Argument expressions.
};

/// A top-level @c define binding: binds @c name to @c value.
///
/// @tparam R        Recursive self-reference.
/// @tparam MaxNodes Arena capacity.
template <typename R, int MaxNodes>
struct core_define {
    std::string_view name;                    ///< The name being defined.
    foundation::arena_box<R, MaxNodes> value; ///< The defining expression.
};

/// Factory template that produces the open-recursive variant layer for the core
/// AST.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list/argument length.
template <int MaxNodes, int MaxList>
struct core_f_factory {
    /// One variant layer; @p R is the recursive self-reference.
    template <typename R>
    using type =
        std::variant<core_integer, core_boolean, core_symbol, core_quote,
                     core_if<R, MaxNodes>, core_lambda<R, MaxNodes, MaxList>,
                     core_application<R, MaxNodes, MaxList>,
                     core_define<R, MaxNodes>>;
};

/// The concrete recursive core AST type, formed as the fixed point of
/// @ref core_f_factory.
///
/// @tparam MaxNodes Maximum tree nodes (arena size).
/// @tparam MaxList  Maximum list/argument length.
template <int MaxNodes, int MaxList>
using core_type =
    foundation::fix<core_f_factory<MaxNodes, MaxList>::template type>;

/// The result of the elaboration phase: a core expression and its arena.
///
/// @c root holds the root node directly (not as a handle); child nodes
/// reference each other through @c arena.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list/argument length.
template <int MaxNodes, int MaxList>
struct elaborated_core {
    using core = core_type<MaxNodes, MaxList>;
    foundation::tree_arena<core, MaxNodes>
        arena; ///< Storage for all sub-nodes.
    core root; ///< The root expression.
};

} // namespace smd::smdscheme::elaborator
#endif
