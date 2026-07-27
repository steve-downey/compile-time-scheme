// src/smd/smdlisp/sender/lisp_plan_tree.hpp                      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_SENDER_LISP_PLAN_TREE_HPP
#define SRC_SMD_SMDLISP_SENDER_LISP_PLAN_TREE_HPP

#include <smd/smdscheme/foundation/static_vector.hpp>

#include <string_view>

namespace smd::smdlisp::sender {

/// Data describing a single node in a reflected sender execution graph.
///
/// The `smdlisp` counterpart of
/// @ref smd::smdscheme::sender::scheme_node_data, populated at compile time
/// by @ref build_lisp_tree via P2996 reflection.  Kept as a separate type
/// rather than reusing the Scheme one because the `smdlisp` graph has node
/// kinds the Scheme graph has no name for -- `unwind_protect` and `defer`
/// above all -- and `smd::smdscheme` is frozen (decision D1), so the Scheme
/// classifier cannot learn them.
struct lisp_node_data {
    int node_id{}; ///< Index of this node in its @ref lisp_plan_tree.
    std::string_view sender_algo{};  ///< Sender kind, e.g. "just", "then",
                                     ///< "unwind_protect".
    std::string_view lisp_context{}; ///< Optional annotation, e.g. the CL
                                     ///< form the node came from.
    smd::smdscheme::foundation::static_vector<int, 8>
        child_ids{}; ///< Indices of child sender nodes.

    friend constexpr auto operator==(lisp_node_data const &,
                                     lisp_node_data const &) -> bool = default;
};

/// A flat, index-addressed tree of @ref lisp_node_data nodes.
///
/// Child links are integer indices rather than pointers, so the tree is
/// trivially copyable and can be produced in a @c consteval context.
///
/// @tparam MaxNodes Maximum number of nodes (default 64).
template <int MaxNodes = 64>
struct lisp_plan_tree {
    smd::smdscheme::foundation::static_vector<lisp_node_data, MaxNodes>
        nodes{};     ///< All nodes, indexed by id.
    int root_id{-1}; ///< Index of the root, or -1 before it is set.

    /// Inserts @p node, assigning it the next available id (which overwrites
    /// @p node's own @c node_id), and returns that id.
    constexpr auto allocate(lisp_node_data node) -> int {
        int id = static_cast<int>(nodes.size());
        node.node_id = id;
        nodes.push_back(node);
        return id;
    }

    /// Returns a const reference to the node at @p id.
    constexpr auto get(int id) const -> lisp_node_data const & {
        return nodes[id];
    }

    /// Returns a mutable reference to the node at @p id.
    constexpr auto get(int id) -> lisp_node_data & { return nodes[id]; }
};

} // namespace smd::smdlisp::sender

#endif
