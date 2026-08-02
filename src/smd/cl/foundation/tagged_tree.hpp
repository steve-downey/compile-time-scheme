// src/smd/cl/foundation/tagged_tree.hpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// New in this tree (step R3): the arena tree that the datum and core ASTs
// are instantiations of, so decision D15's Foldable and Traversable
// instances are written once, in <smd/cl/foundation/tagged_tree_instances.hpp>,
// and both trees inherit them.
#ifndef SRC_SMD_CL_FOUNDATION_TAGGED_TREE_HPP
#define SRC_SMD_CL_FOUNDATION_TAGGED_TREE_HPP

#include <smd/cl/foundation/static_vector.hpp>

#include <cassert>
#include <utility>
#include <variant>

namespace smd::cl::foundation {

/// An interior node of a @ref tagged_tree: a tag value describing what the
/// branch is (a list, a quote, an application, ...) plus the indices of its
/// children within the owning tree.
///
/// @tparam Tag         The branch description type.
/// @tparam MaxChildren Maximum number of children per branch.
template <class Tag, int MaxChildren>
struct tree_branch {
    Tag tag{};                                  ///< What this branch is.
    static_vector<int, MaxChildren> children{}; ///< Child node indices.

    // HIDDEN FRIEND
    friend constexpr auto operator==(tree_branch const &, tree_branch const &)
        -> bool = default;
};

/// A self-contained arena tree whose nodes are either leaves of type
/// @p Leaf or branches carrying a @p Tag and child indices.
///
/// The tree owns its nodes: children are plain @c int indices into the
/// tree's own storage, so a tree is an ordinary value — copyable,
/// comparable, constexpr-usable — with no external arena to keep alive
/// (the dangling-view failure mode recorded on the old tree's
/// @c core_symbol does not exist here). Indices carry no capacity, so
/// nothing capacity-shaped leaks into node identity (decision D14);
/// the capacities parameterise the tree because the tree is storage.
///
/// Construction is children-before-parent: a branch may only refer to
/// nodes that already exist, so node index order is a topological order
/// with every child preceding its parent. A builder that adds children
/// left to right (as the reader does) therefore leaves the leaves in
/// left-to-right source order — the order the typeclass instances in
/// <smd/cl/foundation/tagged_tree_instances.hpp> document.
///
/// @tparam Leaf        Leaf payload type; must be default-constructible
///                     (node storage is a @ref static_vector).
/// @tparam Tag         Branch description type.
/// @tparam MaxNodes    Maximum number of nodes.
/// @tparam MaxChildren Maximum number of children per branch.
// 673865b0-7d8f-4e49-a11e-1bc747348df5
template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
class tagged_tree {
  public:
    /// The leaf payload type, named for leaf-generic code.
    using leaf_type = Leaf;
    /// The branch description type.
    using tag_type = Tag;
    /// The interior-node type.
    using branch_type = tree_branch<Tag, MaxChildren>;
    /// One node: a leaf or a branch.
    using node_type = std::variant<Leaf, branch_type>;
    /// A branch's child-index list.
    using child_list = static_vector<int, MaxChildren>;

    constexpr tagged_tree() = default;

    /// Appends a leaf node and returns its index.
    /// @pre size() < capacity()
    constexpr auto add_leaf(Leaf leaf) -> int;

    /// Appends a branch node and returns its index.
    /// @pre size() < capacity()
    /// @pre every index in @p children names an existing node
    constexpr auto add_branch(Tag tag, child_list children) -> int;

    /// Marks the node at @p index as the root of the tree.
    /// @pre 0 <= index < size()
    constexpr auto set_root(int index) -> void;

    /// Returns the root node's index, or -1 if no root has been set.
    [[nodiscard]] constexpr auto root() const -> int;

    /// Returns the number of nodes.
    [[nodiscard]] constexpr auto size() const -> int;

    /// Returns @p MaxNodes, so builders can check for room before an
    /// @ref add_leaf or @ref add_branch that must not overflow.
    [[nodiscard]] constexpr auto capacity() const -> int;

    /// Returns @p MaxChildren, the per-branch child capacity.
    [[nodiscard]] constexpr auto max_children() const -> int;

    /// Returns the node at @p index.
    /// @pre 0 <= index < size()
    [[nodiscard]] constexpr auto node(int index) const -> node_type const &;

    /// Returns true if the node at @p index is a leaf.
    /// @pre 0 <= index < size()
    [[nodiscard]] constexpr auto is_leaf(int index) const -> bool;

    /// Returns the leaf payload at @p index.
    /// @pre is_leaf(index)
    [[nodiscard]] constexpr auto leaf(int index) const -> Leaf const &;

    /// Returns the branch at @p index.
    /// @pre !is_leaf(index)
    [[nodiscard]] constexpr auto branch(int index) const -> branch_type const &;

    // HIDDEN FRIEND
    friend constexpr auto operator==(tagged_tree const &, tagged_tree const &)
        -> bool = default;

  private:
    static_vector<node_type, MaxNodes> nodes_{};
    int root_{-1};
};
// 673865b0-7d8f-4e49-a11e-1bc747348df5 end

template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
constexpr auto
tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::add_leaf(Leaf leaf) -> int {
    int const index = nodes_.size();
    nodes_.push_back(node_type{std::move(leaf)});
    return index;
}

template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
constexpr auto tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::add_branch(
    Tag tag, child_list children) -> int {
    int const index = nodes_.size();
    nodes_.push_back(
        node_type{branch_type{std::move(tag), std::move(children)}});
    return index;
}

template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
constexpr auto
tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::set_root(int index) -> void {
    assert(index >= 0 && index < nodes_.size());
    root_ = index;
}

template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
constexpr auto tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::root() const
    -> int {
    return root_;
}

template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
constexpr auto tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::size() const
    -> int {
    return nodes_.size();
}

template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
constexpr auto tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::capacity() const
    -> int {
    return MaxNodes;
}

template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
constexpr auto
tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::max_children() const -> int {
    return MaxChildren;
}

template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
constexpr auto
tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::node(int index) const
    -> node_type const & {
    assert(index >= 0 && index < nodes_.size());
    return nodes_[index];
}

template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
constexpr auto
tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::is_leaf(int index) const
    -> bool {
    return std::holds_alternative<Leaf>(node(index));
}

template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
constexpr auto
tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::leaf(int index) const
    -> Leaf const & {
    return std::get<Leaf>(node(index));
}

template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
constexpr auto
tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::branch(int index) const
    -> branch_type const & {
    return std::get<branch_type>(node(index));
}

} // namespace smd::cl::foundation

#endif
