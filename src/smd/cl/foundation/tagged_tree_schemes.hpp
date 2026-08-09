// src/smd/cl/foundation/tagged_tree_schemes.hpp                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// New in this tree: the recursion schemes over tagged_tree, written as
// the topological folds they are. A tagged_tree stores its own base
// functor (node_f at R = int), so a catamorphism is fold_up over the
// node column with fmap_children swapping indices for results, and an
// inherited attribute is fold_down over the link column. No scheme here
// recurses, and none is Mendler-style: over a materialized column the
// children's results are computed by position before the parent is
// visited, so the recurse-knob has nothing left to turn — the one
// client that needs selective descent (evaluation) is a machine, not a
// fold.
#ifndef SRC_SMD_CL_FOUNDATION_TAGGED_TREE_SCHEMES_HPP
#define SRC_SMD_CL_FOUNDATION_TAGGED_TREE_SCHEMES_HPP

#include <smd/cl/foundation/node_f.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/foundation/static_vector.hpp>
#include <smd/cl/foundation/tagged_tree.hpp>
#include <smd/cl/foundation/topo_fold.hpp>

#include <cassert>
#include <functional>
#include <ranges>
#include <type_traits>
#include <utility>

namespace smd::cl::foundation {

namespace detail {

/// Materializes one stored layer for an algebra: child indices become
/// the children's already-computed results, looked up in the fold's own
/// accumulator.
template <class A, class Leaf, class Tag, int MaxNodes, int MaxChildren>
[[nodiscard]] constexpr auto
materialize(static_vector<A, MaxNodes> const &done,
            node_f<Leaf, Tag, int, MaxChildren> const &node) {
    return fmap_children([&done](int child) -> A { return done[child]; }, node);
}

} // namespace detail

// 11011910-7c9b-42e5-bc50-53bbfd3242d9
/// Catamorphism over a @ref tagged_tree: folds the tree bottom-up with a
/// plain F-algebra, and returns the root's value.
///
/// The algebra receives one @ref node_f layer whose child slots hold the
/// children's already-computed results — it never sees an index, never
/// looks anything up, and cannot ask how deep it is. Construction is
/// children-before-parent, so ascending index order visits every child
/// first and the whole catamorphism is one linear pass: no stack, no
/// recursion, no bound on nesting depth.
///
/// This needs I1 (@ref tagged_tree) and nothing else, and in particular
/// needs no law of the algebra — not commutativity, not associativity.
/// The children reach the algebra through the branch's own child list, in
/// that list's order, so two layouts of one shape give one answer, and
/// which topological order the builder chose is invisible here. That is
/// not true of the Foldable and Traversable instances, whose element
/// order *is* index order; see I2.
///
/// @tparam A   The algebra's carrier (explicit).
/// @tparam Alg Callable: `A(node_f<Leaf, Tag, A, MaxChildren> const&)`.
/// @pre the tree has a root
template <class A, class Alg, class Leaf, class Tag, int MaxNodes,
          int MaxChildren>
    requires std::convertible_to<
        std::invoke_result_t<Alg &, node_f<Leaf, Tag, A, MaxChildren> const &>,
        A>
[[nodiscard]] constexpr auto
cata(tagged_tree<Leaf, Tag, MaxNodes, MaxChildren> const &tree, Alg alg) -> A {
    assert(tree.root() >= 0);
    auto const done = fold_up<A, MaxNodes>(
        tree.nodes(),
        [&alg](static_vector<A, MaxNodes> const &results,
               auto const &node) -> A {
            return std::invoke(alg, detail::materialize(results, node));
        });
    return done[tree.root()];
}
// 11011910-7c9b-42e5-bc50-53bbfd3242d9 end

/// Short-circuiting catamorphism: the algebra returns `result<A>`, the
/// leftmost failure wins, and nothing past it is visited.
///
/// @tparam A   The algebra's carrier (explicit).
/// @tparam Alg Callable: `result<A>(node_f<Leaf, Tag, A, MaxChildren>
///             const&)`.
/// @pre the tree has a root
template <class A, class Alg, class Leaf, class Tag, int MaxNodes,
          int MaxChildren>
    requires std::same_as<
        std::remove_cvref_t<std::invoke_result_t<
            Alg &, node_f<Leaf, Tag, A, MaxChildren> const &>>,
        result<A>>
[[nodiscard]] constexpr auto
cata_short(tagged_tree<Leaf, Tag, MaxNodes, MaxChildren> const &tree, Alg alg)
    -> result<A> {
    assert(tree.root() >= 0);
    auto done = fold_up_short<A, MaxNodes>(
        tree.nodes(),
        [&alg](static_vector<A, MaxNodes> const &results,
               auto const &node) -> result<A> {
            return std::invoke(alg, detail::materialize(results, node));
        });
    if (!done.has_value()) {
        return done.error();
    }
    return done.value()[tree.root()];
}

/// Paramorphism over a @ref tagged_tree: @ref cata, except the algebra
/// also receives the tree and the node's own index, so it can inspect
/// the original node — its tag as stored, its position, its link —
/// alongside the children's results.
///
/// This is the scheme for the algebra that needs the node itself
/// (DIV-0016's driver), and it subsumes the Mendler motivation on this
/// representation: everything a Mendler algebra's recurse-knob could
/// reach is already in the accumulator, by position.
///
/// @tparam A   The algebra's carrier (explicit).
/// @tparam Alg Callable: `A(tree, int index, node_f<Leaf, Tag, A,
///             MaxChildren> const&)`.
/// @pre the tree has a root
template <class A, class Alg, class Leaf, class Tag, int MaxNodes,
          int MaxChildren>
    requires std::convertible_to<
        std::invoke_result_t<
            Alg &, tagged_tree<Leaf, Tag, MaxNodes, MaxChildren> const &, int,
            node_f<Leaf, Tag, A, MaxChildren> const &>,
        A>
[[nodiscard]] constexpr auto
para(tagged_tree<Leaf, Tag, MaxNodes, MaxChildren> const &tree, Alg alg) -> A {
    assert(tree.root() >= 0);
    auto const done = fold_up<A, MaxNodes>(
        tree.nodes(),
        [&](static_vector<A, MaxNodes> const &results, auto const &node) -> A {
            return std::invoke(alg, tree, results.size(),
                               detail::materialize(results, node));
        });
    return done[tree.root()];
}

/// Short-circuiting paramorphism; the sibling of @ref cata_short as
/// @ref para is of @ref cata.
///
/// @tparam A   The algebra's carrier (explicit).
/// @tparam Alg Callable: `result<A>(tree, int index, node_f<Leaf, Tag,
///             A, MaxChildren> const&)`.
/// @pre the tree has a root
template <class A, class Alg, class Leaf, class Tag, int MaxNodes,
          int MaxChildren>
    requires std::same_as<
        std::remove_cvref_t<std::invoke_result_t<
            Alg &, tagged_tree<Leaf, Tag, MaxNodes, MaxChildren> const &, int,
            node_f<Leaf, Tag, A, MaxChildren> const &>>,
        result<A>>
[[nodiscard]] constexpr auto
para_short(tagged_tree<Leaf, Tag, MaxNodes, MaxChildren> const &tree, Alg alg)
    -> result<A> {
    assert(tree.root() >= 0);
    auto done = fold_up_short<A, MaxNodes>(
        tree.nodes(),
        [&](static_vector<A, MaxNodes> const &results,
            auto const &node) -> result<A> {
            return std::invoke(alg, tree, results.size(),
                               detail::materialize(results, node));
        });
    if (!done.has_value()) {
        return done.error();
    }
    return std::move(done).value()[tree.root()];
}

// d284e784-4e9f-4a7a-83db-8bfd2c8d82c0
/// Inherited-attribute scan over a @ref tagged_tree: computes one value
/// per node, top down, each from its own row and its parent's
/// already-computed value. Returns the whole column, which is what an
/// inherited attribute is.
///
/// This is @ref fold_down aimed through the tree's link column — the
/// transpose of the child lists — and it is the reason that column
/// exists: without a way to find its parent, a node can only be told
/// what it is by a parent writing into it, which is a scatter. The step
/// receives a null parent pointer at the root and at any node no branch
/// names; distinguishing those is the step's business, via the index.
///
/// @tparam A    The attribute type (explicit; default-constructible).
/// @tparam Step Callable: `A(tree, int index, A const* parent)`.
template <class A, class Step, class Leaf, class Tag, int MaxNodes,
          int MaxChildren>
    requires std::convertible_to<
        std::invoke_result_t<
            Step &, tagged_tree<Leaf, Tag, MaxNodes, MaxChildren> const &, int,
            A const *>,
        A>
[[nodiscard]] constexpr auto
scan_down(tagged_tree<Leaf, Tag, MaxNodes, MaxChildren> const &tree, Step step)
    -> static_vector<A, MaxNodes> {
    return fold_down<A, MaxNodes>(
        tree.links() | std::views::transform([](tree_link const &link) -> int {
            return link.parent;
        }),
        [&](A const *parent, int index) -> A {
            return std::invoke(step, tree, index, parent);
        });
}
// d284e784-4e9f-4a7a-83db-8bfd2c8d82c0 end

} // namespace smd::cl::foundation

#endif
