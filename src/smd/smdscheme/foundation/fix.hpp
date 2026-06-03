// src/smd/smdscheme/foundation/fix.hpp                         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_FOUNDATION_FIX_HPP
#define SRC_SMD_SMDSCHEME_FOUNDATION_FIX_HPP

#include <smd/smdscheme/foundation/functor.hpp>
#include <utility>

namespace smd::smdscheme::foundation {

/// The fixed-point of a functor @p F, enabling recursive types without
/// explicit self-referential template parameters.
///
/// @c fix<F> wraps one layer @c F<fix<F>>, so a recursive tree type
/// @c Tree = F<Tree> is represented as @c fix<F>.  The @c inner member
/// gives access to the outermost @c F layer.
///
/// @tparam F A unary type template (e.g., @c std::variant) that forms one
///           recursion layer.
// 7a8b9c0d-1e2f-3a4b-5c6d-7e8f9a0b1c2d
template <template <class> class F>
struct fix {
    F<foundation::fix<F>> inner; ///< The outermost functor layer.

    constexpr fix() = default;

    /// Constructs from a fully-formed outer layer @p layer.
    constexpr explicit fix(F<foundation::fix<F>> layer)
        : inner(std::move(layer)) {}
};
// 7a8b9c0d-1e2f-3a4b-5c6d-7e8f9a0b1c2d end

/// Recursively folds a @c fix<F> tree using a carrier algebra.
///
/// Each node is first mapped to its carrier type @p R by applying
/// @p algebra to the recursively folded children, then the results are
/// combined bottom-up.  @c fmap is dispatched through the
/// @c functor_typeclass CPO so the recursion is driven by the registered
/// Functor instance for @c F.
///
/// @tparam R       Carrier type — the return type of @p algebra.
/// @tparam F       The recursive functor template.
/// @tparam Algebra A callable with signature @c R(F<R>).
/// @param  tree    Root of the tree to fold.
/// @param  algebra The carrier algebra applied at each node.
/// @return The fold result of type @p R.
// 8b9c0d1e-2f3a-4b5c-6d7e-8f9a0b1c2d3e
template <class R, template <class> class F, class Algebra>
constexpr auto fold_fix(foundation::fix<F> const &tree, Algebra algebra) -> R {
    auto mapped = fmap(tree.inner, [&](auto const &child) -> R {
        return fold_fix<R>(child, algebra);
    });
    return algebra(mapped);
}
// 8b9c0d1e-2f3a-4b5c-6d7e-8f9a0b1c2d3e end

} // namespace smd::smdscheme::foundation

#endif
