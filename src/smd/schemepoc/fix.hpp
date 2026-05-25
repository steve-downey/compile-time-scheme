// src/smd/schemepoc/fix.hpp                                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SCHEMEPOC_FIX_HPP
#define SRC_SMD_SCHEMEPOC_FIX_HPP

#include <utility>

namespace smd::schemepoc {

template <template <class> class F>
struct fix {
    F<fix<F>> inner;

    constexpr fix() = default;
    constexpr explicit fix(F<fix<F>> layer) : inner(std::move(layer)) {}
};

// fold_fix recursively folds a fix<F> tree using a carrier-algebra.
// R is the carrier type (return type of algebra).
// fmap must be findable via ADL for the specific F<...> layer type.
template <class R, template <class> class F, class Algebra>
constexpr auto fold_fix(fix<F> const &tree, Algebra algebra) -> R {
    auto mapped = fmap(tree.inner, [&](auto const &child) -> R {
        return fold_fix<R>(child, algebra);
    });
    return algebra(mapped);
}

} // namespace smd::schemepoc

#endif
