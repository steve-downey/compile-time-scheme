// src/smd/cl/foundation/tagged_tree_instances.hpp                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// New in this tree (step R3): the designated adapter location for
// tagged_tree's typeclass instances (per docs/CODING_RULES.md's Typeclass
// Design rules, datatype and adaptation are separate concerns). The datum
// and core trees are tagged_tree instantiations, so this one header is
// where decision D15's Foldable and Traversable instances live for both.
//
// Traversal order contract for every instance in this header: leaves are
// visited in ascending node-index order. Because tagged_tree construction
// is children-before-parent, a builder that adds children left to right
// (the reader, the elaborator) makes this exactly left-to-right source
// order. For traverse, effects are sequenced in that same order.
#ifndef SRC_SMD_CL_FOUNDATION_TAGGED_TREE_INSTANCES_HPP
#define SRC_SMD_CL_FOUNDATION_TAGGED_TREE_INSTANCES_HPP

#include <smd/cl/foundation/applicative.hpp>
#include <smd/cl/foundation/foldable.hpp>
#include <smd/cl/foundation/functor.hpp>
#include <smd/cl/foundation/static_vector.hpp>
#include <smd/cl/foundation/tagged_tree.hpp>
#include <smd/cl/foundation/traversable.hpp>

#include <algorithm>
#include <functional>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>

namespace smd::cl::foundation {

/// Functor @c Impl for @ref tagged_tree: maps @p f over the leaf payloads
/// in ascending node-index order, copying branch tags, child indices, and
/// the root unchanged, so the mapped tree has identical structure.
///
/// Written as what it is — a map over the node sequence — so shape
/// preservation is structural rather than something the code has to be
/// read for: @c from_nodes takes the mapped sequence and the same root,
/// and a @c transform cannot change a sequence's length.
struct tagged_tree_functor_impl {
    template <class F, class Leaf, class Tag, int MaxNodes, int MaxChildren>
    constexpr auto
    fmap(this auto &&, F &&f,
         tagged_tree<Leaf, Tag, MaxNodes, MaxChildren> const &tree) {
        using B = std::remove_cvref_t<std::invoke_result_t<F &, Leaf const &>>;
        using out_tree = tagged_tree<B, Tag, MaxNodes, MaxChildren>;
        using out_node = typename out_tree::node_type;
        using in_node =
            typename tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::node_type;
        // Dispatches one node's alternatives inside the algebra; it does
        // not drive the walk, which is the transform below.
        auto const map_node = [&f](in_node const &node) -> out_node {
            if (auto const *payload = std::get_if<Leaf>(&node)) {
                return std::invoke(f, *payload);
            }
            return std::get<tree_branch<Tag, MaxChildren>>(node);
        };
        return out_tree::from_nodes(
            tree.nodes() | std::views::transform(map_node), tree.root());
    }
};

/// Functor instance map for @ref tagged_tree.
struct tagged_tree_functor_map : functor<tagged_tree_functor_impl> {
    using tagged_tree_functor_impl::fmap;
};

/// Foldable @c Impl for @ref tagged_tree. Folds visit leaf payloads only;
/// branches contribute structure, not elements.
///
/// Traversal order: @c fold_map combines leaf images in ascending
/// node-index order (left-to-right source order for reader-built trees);
/// @c fold_right combines from the last leaf back to the first.
struct tagged_tree_foldable_impl {
    template <class F, class Leaf, class Tag, int MaxNodes, int MaxChildren,
              class M>
    constexpr auto
    fold_map(this auto &&, F &&f,
             tagged_tree<Leaf, Tag, MaxNodes, MaxChildren> const &tree,
             M const &m) {
        return std::ranges::fold_left(
            tree.leaves(), m.empty(), [&f, &m](auto acc, Leaf const &payload) {
                return m.combine(std::move(acc), std::invoke(f, payload));
            });
    }

    template <class F, class Acc, class Leaf, class Tag, int MaxNodes,
              int MaxChildren>
    constexpr auto
    fold_right(this auto &&, F &&f, Acc init,
               tagged_tree<Leaf, Tag, MaxNodes, MaxChildren> const &tree)
        -> Acc {
        // f is already (element, accumulator) -> accumulator, which is
        // std::ranges::fold_right's own step signature, so the right fold
        // is the standard algorithm and not a reversed walk.
        return std::ranges::fold_right(tree.leaves(), std::move(init),
                                       std::forward<F>(f));
    }
};

/// Foldable instance map for @ref tagged_tree.
struct tagged_tree_foldable_map : foldable<tagged_tree_foldable_impl> {
    using tagged_tree_foldable_impl::fold_map;
    using tagged_tree_foldable_impl::fold_right;
};

/// Traversable @c Impl for @ref tagged_tree.
///
/// @c traverse applies the effectful @p f to every leaf payload in
/// ascending node-index order (the documented Foldable order), sequencing
/// effects left to right through the effect's registered Applicative
/// instance, and rebuilds a tree of identical structure — same node
/// indices, same branch tags and children, same root — inside the effect.
/// Every leaf is visited; the effect's @c apply decides what surviving
/// values look like (for @c result, the leftmost error wins). For error
/// propagation that must stop visiting early, use @c fold_left_short.
///
/// The effect type must name its carried type as @c value_type and have a
/// registered @c applicative_typeclass instance; the carried type must be
/// default-constructible (node storage is a @ref static_vector).
// 8fabe263-2634-4144-abed-f69013131e52
struct tagged_tree_traversable_impl {
    template <class F, class Leaf, class Tag, int MaxNodes, int MaxChildren>
    constexpr auto
    traverse(this auto &&, F &&f,
             tagged_tree<Leaf, Tag, MaxNodes, MaxChildren> const &tree) {
        using effect_type =
            std::remove_cvref_t<std::invoke_result_t<F &, Leaf const &>>;
        using B = typename effect_type::value_type;
        using out_tree = tagged_tree<B, Tag, MaxNodes, MaxChildren>;
        using out_node = typename out_tree::node_type;
        using in_node =
            typename tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::node_type;
        using out_nodes = static_vector<out_node, MaxNodes>;
        auto const &tc = applicative_typeclass<effect_type>;

        // Every node maps to an effect carrying its image: a leaf runs f,
        // a branch is carried by pure, which has no effect of its own. The
        // two arms are uniform, so the walk below is one fold and not a
        // conditional.
        auto const wrap = [](B mapped) { return out_node{std::move(mapped)}; };
        auto const node_effect = [&f, &tc, &wrap](in_node const &node) {
            if (auto const *payload = std::get_if<Leaf>(&node)) {
                return tc.invoke(wrap, std::invoke(f, *payload));
            }
            return tc.pure(
                out_node{std::get<tree_branch<Tag, MaxChildren>>(node)});
        };
        auto const append = [](out_nodes acc, out_node node) {
            acc.push_back(std::move(node));
            return acc;
        };
        // Sequencing the node effects left to right is a left fold in the
        // applicative — the accumulator is an effect, so effect order is
        // fold order and nothing else has to state it.
        auto sequenced = std::ranges::fold_left(
            tree.nodes(), tc.pure(out_nodes{}),
            [&](auto accumulated, in_node const &node) {
                return tc.invoke(append, std::move(accumulated),
                                 node_effect(node));
            });
        return tc.invoke(
            [root = tree.root()](out_nodes sequenced_nodes) {
                return out_tree::from_nodes(sequenced_nodes, root);
            },
            std::move(sequenced));
    }
};
// 8fabe263-2634-4144-abed-f69013131e52 end

/// Traversable instance map for @ref tagged_tree.
struct tagged_tree_traversable_map : traversable<tagged_tree_traversable_impl> {
    using tagged_tree_traversable_impl::traverse;
};

/// Registers the Functor instance for @ref tagged_tree.
template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
inline constexpr auto
    functor_typeclass<tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>> =
        tagged_tree_functor_map{};

/// Registers the Foldable instance for @ref tagged_tree.
template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
inline constexpr auto
    foldable_typeclass<tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>> =
        tagged_tree_foldable_map{};

/// Registers the Traversable instance for @ref tagged_tree.
template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
inline constexpr auto
    traversable_typeclass<tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>> =
        tagged_tree_traversable_map{};

} // namespace smd::cl::foundation

#endif
