// src/smd/smdscheme/sender/scheme_tree.hpp                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_SCHEME_TREE_HPP
#define SRC_SMD_SMDSCHEME_SENDER_SCHEME_TREE_HPP

#include <smd/smdscheme/foundation/static_vector.hpp>
#include <smd/smdscheme/sender/scheme_node_data.hpp>

namespace smd::smdscheme::sender {

/// A flat, index-addressed tree of @ref scheme_node_data nodes.
///
/// Nodes are allocated by @ref allocate in pre-order; child links use integer
/// indices rather than pointers so the tree is trivially copyable and can be
/// produced in a @c consteval context.
///
/// @tparam MaxNodes Maximum number of nodes (default 64).
template <int MaxNodes = 64>
struct scheme_tree {
    foundation::static_vector<scheme_node_data, MaxNodes>
        nodes{};     ///< All nodes, indexed by id.
    int root_id{-1}; ///< Index of the root node, or -1 before the root is set.

    /// Inserts @p node into the tree, assigning it the next available id.
    ///
    /// The @c node_id field of @p node is overwritten with the assigned index.
    ///
    /// @return The assigned integer index.
    constexpr auto allocate(scheme_node_data node) -> int {
        int id = static_cast<int>(nodes.size());
        node.node_id = id;
        nodes.push_back(node);
        return id;
    }

    /// Returns a const reference to the node at @p id.
    constexpr auto get(int id) const -> scheme_node_data const & {
        return nodes[id];
    }

    /// Returns a mutable reference to the node at @p id.
    constexpr auto get(int id) -> scheme_node_data & { return nodes[id]; }
};

} // namespace smd::smdscheme::sender

#endif
