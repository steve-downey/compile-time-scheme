// src/smd/cl/core/ast.hpp                                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_CORE_AST_HPP
#define SRC_SMD_CL_CORE_AST_HPP

#include <smd/cl/foundation/static_vector.hpp>
#include <smd/cl/foundation/tagged_tree.hpp>
#include <smd/cl/foundation/tagged_tree_instances.hpp>
#include <smd/cl/symbol/symbol_id.hpp>

#include <array>
#include <cstddef>
#include <string_view>
#include <variant>

namespace smd::cl::core {

/// Maximum length, in characters, of a string literal the core can carry.
///
/// Deliberately not shared with the reader's @c reader::max_string_chars:
/// the core does not depend on the reader, and the elaborator diagnoses a
/// literal that does not fit rather than truncating it, so the two may
/// differ without anything being silently lost.
inline constexpr int max_string_chars = 128;

/// Maximum number of parameters a @ref core_lambda can name.
///
/// A constant rather than a template parameter, for the same reason
/// @ref max_string_chars is one: a lambda's parameter list is storage
/// inside a node, and decision D14 keeps capacities out of type identity.
inline constexpr int max_lambda_params = 16;

/// A fixnum literal in the elaborated core.
struct core_fixnum {
    int value = 0; ///< The literal value.

    // HIDDEN FRIEND
    friend constexpr auto operator==(core_fixnum, core_fixnum)
        -> bool = default;
};

// 01c40466-6b6c-4651-b7b6-98c7a289ef5d
/// A VARIABLE-namespace reference: a symbol used as an expression,
/// resolved among variable bindings, never among function bindings
/// (Lisp-2). Carries the interned @ref symbol::symbol_id (decision D12),
/// so a compiled program never holds a view into reader storage — the
/// dangling-name failure the old tree's @c core_symbol documents cannot
/// arise.
struct core_variable {
    symbol::symbol_id id{}; ///< The referenced symbol.

    // HIDDEN FRIEND
    friend constexpr auto operator==(core_variable, core_variable)
        -> bool = default;
};

/// A self-evaluating keyword literal.
struct core_keyword {
    symbol::symbol_id id{}; ///< The interned keyword.

    // HIDDEN FRIEND
    friend constexpr auto operator==(core_keyword, core_keyword)
        -> bool = default;
};

/// The self-evaluating `nil` constant: the sole false value and the empty
/// list. Distinct from @ref core_variable so a bare `NIL` elaborates to a
/// constant, not a variable lookup.
struct core_nil {
    // HIDDEN FRIEND
    friend constexpr auto operator==(core_nil, core_nil) -> bool = default;
};

/// The canonical `t` (true) constant, distinct from @ref core_variable
/// for the same reason as @ref core_nil.
struct core_t {
    // HIDDEN FRIEND
    friend constexpr auto operator==(core_t, core_t) -> bool = default;
};

/// A self-evaluating character literal.
struct core_character {
    char value = 0; ///< The character.

    // HIDDEN FRIEND
    friend constexpr auto operator==(core_character, core_character)
        -> bool = default;
};

/// A self-evaluating string literal, carrying its own characters.
///
/// The storage is the core's, not the reader's: a compiled program holds no
/// view into the datum arena, for the same reason @ref core_variable holds a
/// @ref symbol::symbol_id rather than a name (decision D12). The fixed
/// capacity is storage, not type identity, so decision D14 is untouched —
/// @ref max_string_chars is a constant, never a template parameter.
struct core_string {
    std::array<char, max_string_chars> storage{}; ///< The characters.
    int length = 0; ///< Number of valid characters in storage.

    /// Returns the contents as a view into this object's own storage,
    /// valid while this @c core_string (or the copy it was taken from) is
    /// alive.
    [[nodiscard]] constexpr auto view() const -> std::string_view {
        return std::string_view(storage.data(),
                                static_cast<std::size_t>(length));
    }

    // HIDDEN FRIEND
    friend constexpr auto operator==(core_string const &, core_string const &)
        -> bool = default;
};

/// A symbol used as *data* rather than as a variable reference: what
/// `'foo` denotes. Distinct from @ref core_variable because the position
/// decides the namespace, not the atom — the same interned
/// @ref symbol::symbol_id reaches the evaluator as a value here and as a
/// lookup there.
struct core_quoted_symbol {
    symbol::symbol_id id{}; ///< The symbol, as a value.

    // HIDDEN FRIEND
    friend constexpr auto operator==(core_quoted_symbol, core_quoted_symbol)
        -> bool = default;
};

/// A core-tree leaf: one expression that has no subexpressions.
using core_leaf =
    std::variant<core_fixnum, core_variable, core_keyword, core_nil, core_t,
                 core_character, core_string, core_quoted_symbol>;
// 01c40466-6b6c-4651-b7b6-98c7a289ef5d end

// 60da8def-db67-496e-9b36-6c544c60bc1e
/// A FUNCTION-namespace call (Lisp-2): the callee is a symbol resolved in
/// the function namespace, not a subexpression, so it lives in the tag;
/// the children are the argument expressions, left to right.
struct core_call {
    symbol::symbol_id callee{}; ///< The called function's symbol.

    // HIDDEN FRIEND
    friend constexpr auto operator==(core_call, core_call) -> bool = default;
};

/// A two- or three-armed conditional; children are test, then, and
/// (optionally) else.
struct core_if {
    // HIDDEN FRIEND
    friend constexpr auto operator==(core_if, core_if) -> bool = default;
};

/// A sequence evaluated left to right for the last child's value;
/// children are the body forms.
struct core_progn {
    // HIDDEN FRIEND
    friend constexpr auto operator==(core_progn, core_progn) -> bool = default;
};

/// Hermetic pair construction, used only to build quoted list data;
/// children are the car and the cdr.
///
/// Decision D18 says lower onto existing forms before adding a tag, and the
/// existing form here would be @ref core_call to `cons` — which is wrong,
/// not merely inelegant. ANSI quoted data is a literal, not a call, so
/// `'(1 2)` must not depend on what the function slot of `CONS` holds; a
/// program that redefines `cons` still gets pairs from its quoted lists.
/// That is a semantics no existing tag expresses, which is exactly D18's
/// stated exception.
struct core_cons {
    // HIDDEN FRIEND
    friend constexpr auto operator==(core_cons, core_cons) -> bool = default;
};

// 60da8def-db67-496e-9b36-6c544c60bc1e end

// 7d0ac0a1-9c4e-4f2b-8a2e-3f4b6d1e5c07
/// A function definition: the parameters it binds, and one child, the body
/// it evaluates with them bound.
///
/// Not an expression yet. Nothing in the R5 object language evaluates a
/// @c core_lambda — @ref core_defun reads this node without evaluating it,
/// which is why there is no function object among the evaluator's values.
/// The step that makes `lambda` an expression and `#'f` executable gives
/// this node an evaluation rule and adds that value alternative; it does
/// not change this node.
struct core_lambda {
    foundation::static_vector<symbol::symbol_id, max_lambda_params>
        params{}; ///< The parameter names, left to right.

    // HIDDEN FRIEND
    friend constexpr auto operator==(core_lambda const &, core_lambda const &)
        -> bool = default;
};

/// Installs its one child — a @ref core_lambda — in a symbol's function
/// slot, and evaluates to that symbol.
///
/// This is the node that closes DIV-0009. The recursion works because the
/// name is resolved in the function slot at *call* time rather than
/// captured at definition time, so a body that calls its own name finds the
/// definition that is being installed. Decision D18 asks for a lowering
/// before a tag: there is none to be had, because "put this function in
/// that symbol's function slot" is not something any other core form says.
struct core_defun {
    symbol::symbol_id name{}; ///< The symbol whose function slot to set.

    // HIDDEN FRIEND
    friend constexpr auto operator==(core_defun, core_defun) -> bool = default;
};

/// A named exit point; children are the body forms, evaluated for the last
/// one's value.
///
/// A block is where the question "is this unwind aimed at me?" is asked,
/// and it is asked against this node's own name (decision D13).
struct core_block {
    symbol::symbol_id name{}; ///< The block's name.

    // HIDDEN FRIEND
    friend constexpr auto operator==(core_block, core_block) -> bool = default;
};

/// Puts an unwind in flight toward the named block, carrying its one
/// child's value, or `nil` if it has no child.
struct core_return_from {
    symbol::symbol_id name{}; ///< The block to return from.

    // HIDDEN FRIEND
    friend constexpr auto operator==(core_return_from, core_return_from)
        -> bool = default;
};

/// Evaluates its first child, then its remaining children as cleanup —
/// whichever way the first child completed.
///
/// The one form whose meaning is stated in terms of all three channels at
/// once, which is why it arrives in the phase that split them.
struct core_unwind_protect {
    // HIDDEN FRIEND
    friend constexpr auto operator==(core_unwind_protect, core_unwind_protect)
        -> bool = default;
};

/// A core-tree branch tag: what a compound expression is. The elaborator
/// grows this variant as it lowers more special operators; decision D18
/// keeps that growth slow — prefer lowering onto existing forms over new
/// tags.
using core_tag =
    std::variant<core_call, core_if, core_progn, core_cons, core_lambda,
                 core_defun, core_block, core_return_from, core_unwind_protect>;
// 7d0ac0a1-9c4e-4f2b-8a2e-3f4b6d1e5c07 end

/// The elaborated core tree: a self-contained @ref foundation::tagged_tree
/// value whose leaves are @ref core_leaf and whose branches are
/// @ref core_tag, carrying the Foldable and Traversable instances of
/// <smd/cl/foundation/tagged_tree_instances.hpp> from the start
/// (decision D15) rather than retrofitted. Builders add children before
/// parents, left to right, so those instances' documented leaf order is
/// left-to-right source order.
///
/// @tparam MaxNodes    Maximum tree nodes.
/// @tparam MaxChildren Maximum subexpressions per node.
template <int MaxNodes, int MaxChildren>
using core_tree =
    foundation::tagged_tree<core_leaf, core_tag, MaxNodes, MaxChildren>;

} // namespace smd::cl::core

#endif
