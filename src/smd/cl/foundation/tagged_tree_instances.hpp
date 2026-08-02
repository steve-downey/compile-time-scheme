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
#include <smd/cl/foundation/tagged_tree.hpp>
#include <smd/cl/foundation/traversable.hpp>

#include <functional>
#include <ranges>
#include <type_traits>
#include <utility>

namespace smd::cl::foundation {

/// Functor @c Impl for @ref tagged_tree: maps @p f over the leaf payloads
/// in ascending node-index order, copying branch tags, child indices, and
/// the root unchanged, so the mapped tree has identical structure.
struct tagged_tree_functor_impl {
    template <class F, class Leaf, class Tag, int MaxNodes, int MaxChildren>
    constexpr auto
    fmap(this auto &&, F &&f,
         tagged_tree<Leaf, Tag, MaxNodes, MaxChildren> const &tree) {
        using B = std::remove_cvref_t<std::invoke_result_t<F &, Leaf const &>>;
        tagged_tree<B, Tag, MaxNodes, MaxChildren> mapped;
        std::ranges::for_each(std::views::iota(0, tree.size()), [&](int i) {
            if (tree.is_leaf(i)) {
                mapped.add_leaf(std::invoke(f, tree.leaf(i)));
            } else {
                mapped.add_branch(tree.branch(i).tag, tree.branch(i).children);
            }
        });
        if (tree.root() >= 0) {
            mapped.set_root(tree.root());
        }
        return mapped;
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
        auto acc = m.empty();
        std::ranges::for_each(std::views::iota(0, tree.size()), [&](int i) {
            if (tree.is_leaf(i)) {
                acc = m.combine(std::move(acc), std::invoke(f, tree.leaf(i)));
            }
        });
        return acc;
    }

    template <class F, class Acc, class Leaf, class Tag, int MaxNodes,
              int MaxChildren>
    constexpr auto
    fold_right(this auto &&, F &&f, Acc init,
               tagged_tree<Leaf, Tag, MaxNodes, MaxChildren> const &tree)
        -> Acc {
        std::ranges::for_each(
            std::views::iota(0, tree.size()) | std::views::reverse, [&](int i) {
                if (tree.is_leaf(i)) {
                    init = f(tree.leaf(i), std::move(init));
                }
            });
        return init;
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
        auto const &tc = applicative_typeclass<effect_type>;
        auto accumulated = tc.pure(out_tree{});
        auto const append_leaf = [](out_tree out, B mapped) {
            out.add_leaf(std::move(mapped));
            return out;
        };
        std::ranges::for_each(std::views::iota(0, tree.size()), [&](int i) {
            if (tree.is_leaf(i)) {
                accumulated = tc.invoke(append_leaf, std::move(accumulated),
                                        std::invoke(f, tree.leaf(i)));
            } else {
                accumulated = tc.invoke(
                    [&branch = tree.branch(i)](out_tree out) {
                        out.add_branch(branch.tag, branch.children);
                        return out;
                    },
                    std::move(accumulated));
            }
        });
        return tc.invoke(
            [root = tree.root()](out_tree out) {
                if (root >= 0) {
                    out.set_root(root);
                }
                return out;
            },
            std::move(accumulated));
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
