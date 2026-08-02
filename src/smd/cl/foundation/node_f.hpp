// src/smd/cl/foundation/node_f.hpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// New in this tree: the base functor a tagged tree is the fixed point
// of. One layer of tree, generic in what a child is: R = int ties the
// knot through an index column, and every recursion scheme over the
// tree is a fold that maps R before the algebra looks.
#ifndef SRC_SMD_CL_FOUNDATION_NODE_F_HPP
#define SRC_SMD_CL_FOUNDATION_NODE_F_HPP

#include <smd/cl/foundation/static_vector.hpp>

#include <functional>
#include <ranges>
#include <type_traits>
#include <variant>

namespace smd::cl::foundation {

// 230b6f16-acb7-488a-800b-29c0a84bd2f8
/// One interior layer of a tagged tree: a tag value describing what the
/// branch is, and its children — whatever a child currently is.
///
/// @tparam Tag         The branch description type.
/// @tparam R           The recursive position: what occupies a child
///                     slot. `int` for a stored tree (a child is an
///                     index), an algebra's carrier mid-fold (a child is
///                     its already-computed result).
/// @tparam MaxChildren Maximum number of children per branch.
template <class Tag, class R, int MaxChildren>
struct branch_f {
    Tag tag{};                                ///< What this branch is.
    static_vector<R, MaxChildren> children{}; ///< The child slots.

    // HIDDEN FRIEND
    friend constexpr auto operator==(branch_f const &, branch_f const &)
        -> bool = default;
};

/// One layer of a tagged tree: a leaf, or a branch over child slots of
/// type @p R. This is the tree's base functor; the tree itself is its
/// fixed point at `R = int`, materialized as a column those ints index.
template <class Leaf, class Tag, class R, int MaxChildren>
using node_f = std::variant<Leaf, branch_f<Tag, R, MaxChildren>>;

/// Maps @p f over a layer's recursive positions — each child slot —
/// leaving the leaf alternative and a branch's tag untouched. This is
/// the base functor's fmap, and it is the whole interface a recursion
/// scheme needs: fold results into the child slots, then hand the layer
/// to an algebra that never chases an index.
template <class F, class Leaf, class Tag, class R, int MaxChildren>
[[nodiscard]] constexpr auto
fmap_children(F &&f, node_f<Leaf, Tag, R, MaxChildren> const &node) {
    using S = std::remove_cvref_t<std::invoke_result_t<F &, R const &>>;
    using out_node = node_f<Leaf, Tag, S, MaxChildren>;
    if (auto const *leaf = std::get_if<Leaf>(&node)) {
        return out_node{*leaf};
    }
    auto const &branch = std::get<branch_f<Tag, R, MaxChildren>>(node);
    branch_f<Tag, S, MaxChildren> mapped{branch.tag, {}};
    mapped.children.append_range(
        branch.children | std::views::transform([&f](R const &child) -> S {
            return std::invoke(f, child);
        }));
    return out_node{std::move(mapped)};
}
// 230b6f16-acb7-488a-800b-29c0a84bd2f8 end

} // namespace smd::cl::foundation

#endif
