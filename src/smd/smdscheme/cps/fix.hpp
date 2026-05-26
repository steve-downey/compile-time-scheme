// src/smd/schemepoc/fix.hpp                                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_CPS_FIX_HPP
#define SRC_SMD_SMDSCHEME_CPS_FIX_HPP

#include <smd/smdscheme/parser/functor.hpp>
#include <utility>

namespace smd::smdscheme::cps {

template <template <class> class F>
struct fix {
    F<cps::fix<F>> inner;

    constexpr fix() = default;
    constexpr explicit fix(F<cps::fix<F>> layer) : inner(std::move(layer)) {}
};

// fold_fix recursively folds a cps::fix<F> tree using a carrier-algebra.
// R is the carrier type (return type of algebra).
// fmap is invoked via the CPO smd::smdscheme::fmap, which delegates to
// functor_typeclass.
template <class R, template <class> class F, class Algebra>
constexpr auto fold_fix(cps::fix<F> const &tree, Algebra algebra) -> R {
    auto mapped = fmap(tree.inner, [&](auto const &child) -> R {
        return fold_fix<R>(child, algebra);
    });
    return algebra(mapped);
}

} // namespace smd::smdscheme::cps

#endif
