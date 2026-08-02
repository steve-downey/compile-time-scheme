// src/smd/cl/foundation/tagged_tree.hpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// New in this tree (step R3): the arena tree that the datum and core ASTs
// are instantiations of, so decision D15's Foldable and Traversable
// instances are written once, in <smd/cl/foundation/tagged_tree_instances.hpp>,
// and both trees inherit them.
#ifndef SRC_SMD_CL_FOUNDATION_TAGGED_TREE_HPP
#define SRC_SMD_CL_FOUNDATION_TAGGED_TREE_HPP

#include <smd/cl/foundation/node_f.hpp>
#include <smd/cl/foundation/static_vector.hpp>

#include <cassert>
#include <concepts>
#include <ranges>
#include <span>
#include <utility>
#include <variant>

namespace smd::cl::foundation {

/// An interior node of a @ref tagged_tree: one @ref branch_f layer with
/// the knot tied — the recursive position is `int`, an index into the
/// owning tree's own node column. The alias is the point: a stored
/// branch and a mid-fold branch (children replaced by an algebra's
/// results) are the same template at different `R`.
///
/// @tparam Tag         The branch description type.
/// @tparam MaxChildren Maximum number of children per branch.
template <class Tag, int MaxChildren>
using tree_branch = branch_f<Tag, int, MaxChildren>;

/// Where a node sits beneath its parent: the parent's index, and the node's
/// position in that parent's child list.
///
/// This is the transpose of a branch's child list — the same edge relation
/// read the other way — and a tree maintains both. Which direction a pass
/// has decides its shape: with children only, giving every node a property
/// derived from its parent is a scatter, because each node must write its
/// children's entries. With the parent link, the same pass is a fold, because
/// each node reads its parent's entry and writes only its own. Construction
/// is children-before-parent, so a parent's index always exceeds its
/// children's and descending index order visits every parent first.
///
/// Both fields are -1 for a node no branch names: the root, and any node
/// that was built but never referenced.
struct tree_link {
    int parent{-1};  ///< The parent node's index, or -1.
    int ordinal{-1}; ///< Position among the parent's children, or -1.

    // HIDDEN FRIEND
    friend constexpr auto operator==(tree_link, tree_link) -> bool = default;
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
    /// One node: a leaf or a branch — one @ref node_f layer at `R = int`.
    /// The static_assert below the class is the witness that the tree is
    /// the base functor's fixed point, materialized as a column.
    using node_type = node_f<Leaf, Tag, int, MaxChildren>;
    /// A branch's child-index list.
    using child_list = static_vector<int, MaxChildren>;

    constexpr tagged_tree() = default;

    /// Builds a tree from a node sequence and a root index, in that order.
    ///
    /// This is the assembly half of every shape-preserving algorithm over a
    /// tree: map @ref nodes to a new node sequence, hand it back here with
    /// the same root, and the result has the same shape by construction —
    /// same node count, same indices, so the copied child lists still name
    /// the nodes they named before. Nothing has to walk an index range to
    /// arrange that.
    ///
    /// @pre the range's length is <= @p MaxNodes
    /// @pre @p root is -1 (no root) or an index into the range
    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_reference_t<R>,
                                     node_type>
    [[nodiscard]] static constexpr auto from_nodes(R &&node_range, int root)
        -> tagged_tree;

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

    /// Returns the nodes in index order.
    ///
    /// This is the sequence an algorithm over a whole tree is written
    /// against, and the reason none of them needs an index range of its
    /// own: construction is children-before-parent, so index order is a
    /// topological order, and a walk in that order is an ascending walk.
    [[nodiscard]] constexpr auto nodes() const -> std::span<node_type const>;

    /// Returns the leaf payloads in index order, as a view over @ref nodes.
    ///
    /// This is the element sequence of the Foldable and Traversable
    /// instances in <smd/cl/foundation/tagged_tree_instances.hpp>: a fold
    /// sees leaves, because branches are structure and not elements. The
    /// documented traversal order is this view's order.
    [[nodiscard]] constexpr auto leaves() const;

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

    /// Returns where the node at @p index sits beneath its parent.
    /// @pre 0 <= index < size()
    [[nodiscard]] constexpr auto link(int index) const -> tree_link const &;

    /// Returns the parent links in index order, parallel to @ref nodes —
    /// the column to zip against when a pass reads both.
    [[nodiscard]] constexpr auto links() const -> std::span<tree_link const>;

    // HIDDEN FRIEND
    friend constexpr auto operator==(tagged_tree const &, tagged_tree const &)
        -> bool = default;

  private:
    /// Points @p children at @p index, their parent. The only scatter left
    /// in the design, and it is the container's own: it happens once, when
    /// the edge is created, rather than once per pass that needs it.
    constexpr auto link_children(int index, child_list const &children) -> void;

    /// Recomputes every link from the nodes. Used where nodes arrive
    /// already assembled (@ref from_nodes) rather than an edge at a time.
    constexpr auto relink() -> void;

    static_vector<node_type, MaxNodes> nodes_{};
    static_vector<tree_link, MaxNodes> links_{};
    int root_{-1};
};
// 673865b0-7d8f-4e49-a11e-1bc747348df5 end

// b3201a0d-8d11-4849-bb77-d10c75b2a0a4
/// The fixpoint witness: a tree's node is exactly one base-functor layer
/// with `int` in the recursive position. Nothing about this is a design
/// intention that code review has to protect — it is a type equality the
/// compiler checks.
static_assert(
    std::same_as<tagged_tree<int, char, 8, 4>::node_type,
                 node_f<int, char, int, 4>>,
    "a tagged_tree stores its own base functor, knot tied at R = int");
// b3201a0d-8d11-4849-bb77-d10c75b2a0a4 end

template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
template <std::ranges::input_range R>
    requires std::convertible_to<
        std::ranges::range_reference_t<R>,
        std::variant<Leaf, tree_branch<Tag, MaxChildren>>>
constexpr auto tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::from_nodes(
    R &&node_range, int root) -> tagged_tree {
    tagged_tree built;
    built.nodes_.append_range(std::forward<R>(node_range));
    built.relink();
    if (root >= 0) {
        built.set_root(root);
    }
    return built;
}

template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
constexpr auto tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::link_children(
    int index, child_list const &children) -> void {
    for (int ordinal = 0; ordinal < children.size();
         ++ordinal) { // substrate generic algorithm
        links_[children[ordinal]] = tree_link{index, ordinal};
    }
}

template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
constexpr auto tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::relink() -> void {
    links_ =
        static_vector<tree_link, MaxNodes>::filled(nodes_.size(), tree_link{});
    for (int index = 0; index < nodes_.size();
         ++index) { // substrate generic algorithm
        if (auto const *branch = std::get_if<branch_type>(&nodes_[index])) {
            link_children(index, branch->children);
        }
    }
}

template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
constexpr auto
tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::add_leaf(Leaf leaf) -> int {
    int const index = nodes_.size();
    nodes_.push_back(node_type{std::move(leaf)});
    links_.push_back(tree_link{});
    return index;
}

template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
constexpr auto tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::add_branch(
    Tag tag, child_list children) -> int {
    int const index = nodes_.size();
    links_.push_back(tree_link{});
    link_children(index, children);
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
constexpr auto tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::nodes() const
    -> std::span<node_type const> {
    return std::span<node_type const>{nodes_.begin(), nodes_.end()};
}

template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
constexpr auto tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::leaves() const {
    return nodes() | std::views::filter([](node_type const &n) {
               return std::holds_alternative<Leaf>(n);
           }) |
           std::views::transform([](node_type const &n) -> Leaf const & {
               return std::get<Leaf>(n);
           });
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

template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
constexpr auto
tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::link(int index) const
    -> tree_link const & {
    assert(index >= 0 && index < links_.size());
    return links_[index];
}

template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
constexpr auto tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::links() const
    -> std::span<tree_link const> {
    return std::span<tree_link const>{links_.begin(), links_.end()};
}

} // namespace smd::cl::foundation

#endif
