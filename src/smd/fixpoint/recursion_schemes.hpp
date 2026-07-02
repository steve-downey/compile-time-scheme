// src/smd/fixpoint/recursion_schemes.hpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_RECURSION_SCHEMES
#define INCLUDED_SMD_FIXPOINT_RECURSION_SCHEMES

#include <smd/fixpoint/fix.hpp>

namespace smd::fixpoint {

template <typename Result, template <typename> class F, typename Algebra,
          typename FMap>
auto fold_fix(const Algebra &algebra, const FMap &fmap_fn, const Fix<F> &tree)
    -> Result {
    const auto &layer = unwrap_fix(tree);
    auto evaluated = fmap_fn(
        [&](const Fix<F> &child) -> Result {
            return fold_fix<Result>(algebra, fmap_fn, child);
        },
        layer);
    return algebra(evaluated);
}

template <template <typename> class F, typename Coalgebra, typename FMap,
          typename Seed>
auto unfold_fix(const Coalgebra &coalgebra, const FMap &fmap_fn,
                const Seed &seed) -> Fix<F> {
    auto layer = coalgebra(seed);
    auto expanded = fmap_fn(
        [&](const Seed &child) -> Fix<F> {
            return unfold_fix<F>(coalgebra, fmap_fn, child);
        },
        layer);
    return wrap_fix<F>(std::move(expanded));
}

template <typename Result, template <typename> class F, typename Algebra,
          typename Coalgebra, typename FMap, typename Seed>
auto refold(const Algebra &algebra, const Coalgebra &coalgebra,
            const FMap &fmap_fn, const Seed &seed) -> Result {
    auto layer = coalgebra(seed);
    auto evaluated = fmap_fn(
        [&](const Seed &child) -> Result {
            return refold<Result, F>(algebra, coalgebra, fmap_fn, child);
        },
        layer);
    return algebra(evaluated);
}

template <typename Result, template <typename> class F, typename Ctx,
          typename Algebra>
constexpr auto mendler_fold(Algebra const &alg, Ctx const &ctx,
                            Fix<F> const &tree) -> Result {
    auto recurse = [&alg](Fix<F> const &child, Ctx const &c) -> Result {
        return mendler_fold<Result, F>(alg, c, child);
    };
    return alg(recurse, ctx, unwrap_fix(tree));
}

template <typename Result, template <typename> class F, typename Ctx,
          typename Algebra>
constexpr auto mendler_para(Algebra const &alg, Ctx const &ctx,
                            Fix<F> const &tree) -> Result {
    auto recurse = [&alg](Fix<F> const &child, Ctx const &c) -> Result {
        return mendler_para<Result, F>(alg, c, child);
    };
    return alg(recurse, ctx, tree, unwrap_fix(tree));
}

template <typename Result, template <typename> class F, typename Algebra,
          typename FMap>
[[deprecated("use fold_fix")]]
auto cata(const Algebra &algebra, const FMap &fmap_fn, const Fix<F> &tree)
    -> Result {
    return fold_fix<Result>(algebra, fmap_fn, tree);
}

} // namespace smd::fixpoint

#endif
