// src/smd/cl/core/ast.hpp                                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_CORE_AST_HPP
#define SRC_SMD_CL_CORE_AST_HPP

#include <smd/cl/foundation/tagged_tree.hpp>
#include <smd/cl/foundation/tagged_tree_instances.hpp>
#include <smd/cl/symbol/symbol_id.hpp>

#include <variant>

namespace smd::cl::core {

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

/// A core-tree leaf: one expression that has no subexpressions.
using core_leaf =
    std::variant<core_fixnum, core_variable, core_keyword, core_nil, core_t>;
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

/// A core-tree branch tag: what a compound expression is. Step R4 (the
/// elaborator) grows this variant as it lowers more special operators;
/// decision D18 keeps that growth slow — prefer lowering onto existing
/// forms over new tags.
using core_tag = std::variant<core_call, core_if, core_progn>;
// 60da8def-db67-496e-9b36-6c544c60bc1e end

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
