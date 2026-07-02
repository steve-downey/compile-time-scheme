// src/smd/fixpoint/recursion_schemes.t.cpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/fixpoint/recursion_schemes.hpp>
#include <smd/fixpoint/recursion_schemes.hpp> // Re-inclusion check

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/overloaded.hpp>

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <variant>

using smd::fixpoint::Box;
using smd::fixpoint::Fix;
using smd::fixpoint::fold_fix;
using smd::fixpoint::make_box;
using smd::fixpoint::mendler_fold;
using smd::fixpoint::mendler_para;
using smd::fixpoint::overloaded;
using smd::fixpoint::refold;
using smd::fixpoint::unfold_fix;
using smd::fixpoint::unwrap_fix;
using smd::fixpoint::wrap_fix;

namespace {

struct Zero {};

template <typename A>
struct Succ {
    Box<A> pred;
};

template <typename A>
using NatF = std::variant<Zero, Succ<A>>;

using Nat = Fix<NatF>;

auto make_zero() -> Nat { return wrap_fix<NatF>(NatF<Nat>{Zero{}}); }

auto make_succ(Nat n) -> Nat {
    return wrap_fix<NatF>(NatF<Nat>{Succ<Nat>{make_box<Nat>(std::move(n))}});
}

template <typename A, typename F>
auto fmap_nat(F &&f, const NatF<A> &nat) {
    using B = std::invoke_result_t<F, const A &>;
    return std::visit(overloaded{
                          [](const Zero &) -> NatF<B> { return Zero{}; },
                          [&f](const Succ<A> &s) -> NatF<B> {
                              return Succ<B>{
                                  make_box<B>(std::invoke(f, *s.pred))};
                          },
                      },
                      nat);
}

auto fmap_nat_fn = [](auto &&f, const auto &nat) {
    return fmap_nat(std::forward<decltype(f)>(f), nat);
};

auto count_algebra = [](const NatF<int> &n) -> int {
    return std::visit(overloaded{
                          [](const Zero &) { return 0; },
                          [](const Succ<int> &s) { return *s.pred + 1; },
                      },
                      n);
};

auto nat_coalgebra = [](int n) -> NatF<int> {
    if (n <= 0)
        return Zero{};
    return Succ<int>{make_box<int>(n - 1)};
};

} // namespace

TEST_CASE("Cata - NatZero") {
    auto zero = make_zero();
    CHECK(fold_fix<int>(count_algebra, fmap_nat_fn, zero) == 0);
}

TEST_CASE("Cata - NatTwo") {
    auto two = make_succ(make_succ(make_zero()));
    CHECK(fold_fix<int>(count_algebra, fmap_nat_fn, two) == 2);
}

TEST_CASE("Cata - NatFive") {
    auto n = make_zero();
    for (int i = 0; i < 5; ++i) {
        n = make_succ(std::move(n));
    }
    CHECK(fold_fix<int>(count_algebra, fmap_nat_fn, n) == 5);
}

TEST_CASE("Cata - NatCustomAlgebra") {
    auto three = make_succ(make_succ(make_succ(make_zero())));

    auto bool_algebra = [](const NatF<bool> &n) -> bool {
        return std::visit(overloaded{
                              [](const Zero &) { return true; },
                              [](const Succ<bool> &s) { return !*s.pred; },
                          },
                          n);
    };

    CHECK(fold_fix<bool>(bool_algebra, fmap_nat_fn, three) == false);
}

TEST_CASE("UnfoldFix - NatFromZero") {
    auto nat = unfold_fix<NatF>(nat_coalgebra, fmap_nat_fn, 0);
    CHECK(fold_fix<int>(count_algebra, fmap_nat_fn, nat) == 0);
}

TEST_CASE("UnfoldFix - NatFromFive") {
    auto nat = unfold_fix<NatF>(nat_coalgebra, fmap_nat_fn, 5);
    CHECK(fold_fix<int>(count_algebra, fmap_nat_fn, nat) == 5);
}

TEST_CASE("Refold - NatZero") {
    CHECK(refold<int, NatF>(count_algebra, nat_coalgebra, fmap_nat_fn, 0) == 0);
}

TEST_CASE("Refold - NatFive") {
    CHECK(refold<int, NatF>(count_algebra, nat_coalgebra, fmap_nat_fn, 5) == 5);
}

TEST_CASE("Refold - EquivalentToFoldOfUnfold") {
    for (int n = 0; n < 10; ++n) {
        auto via_tree =
            fold_fix<int>(count_algebra, fmap_nat_fn,
                          unfold_fix<NatF>(nat_coalgebra, fmap_nat_fn, n));
        auto via_refold =
            refold<int, NatF>(count_algebra, nat_coalgebra, fmap_nat_fn, n);
        CHECK(via_tree == via_refold);
    }
}

// --- Mendler-style fold tests ---

namespace {

auto mendler_count_algebra = [](auto const &recurse, int ctx,
                                NatF<Nat> const &layer) -> int {
    return std::visit(overloaded{
                          [&](Zero const &) -> int { return ctx; },
                          [&](Succ<Nat> const &s) -> int {
                              return recurse(*s.pred, ctx + 1);
                          },
                      },
                      layer);
};

constexpr auto ce_make_zero() -> Nat {
    return wrap_fix<NatF>(NatF<Nat>{Zero{}});
}

constexpr auto ce_make_succ(Nat n) -> Nat {
    return wrap_fix<NatF>(NatF<Nat>{Succ<Nat>{make_box<Nat>(std::move(n))}});
}

constexpr auto constexpr_mendler_count(Nat const &n) -> int {
    auto alg = [](auto const &recurse, int ctx,
                  NatF<Nat> const &layer) -> int {
        return std::visit(overloaded{
                              [&](Zero const &) -> int { return ctx; },
                              [&](Succ<Nat> const &s) -> int {
                                  return recurse(*s.pred, ctx + 1);
                              },
                          },
                          layer);
    };
    return mendler_fold<int, NatF>(alg, 0, n);
}

constexpr auto constexpr_mendler_para_depth(Nat const &n) -> int {
    auto alg = [](auto const &recurse, int ctx, Nat const &,
                  NatF<Nat> const &layer) -> int {
        return std::visit(overloaded{
                              [&](Zero const &) -> int { return ctx; },
                              [&](Succ<Nat> const &s) -> int {
                                  return recurse(*s.pred, ctx + 1);
                              },
                          },
                          layer);
    };
    return mendler_para<int, NatF>(alg, 0, n);
}

} // namespace

TEST_CASE("MendlerFold - NatZero") {
    auto zero = make_zero();
    CHECK(mendler_fold<int, NatF>(mendler_count_algebra, 0, zero) == 0);
}

TEST_CASE("MendlerFold - NatThree") {
    auto three = make_succ(make_succ(make_succ(make_zero())));
    CHECK(mendler_fold<int, NatF>(mendler_count_algebra, 0, three) == 3);
}

TEST_CASE("MendlerFold - WithContext") {
    auto two = make_succ(make_succ(make_zero()));
    CHECK(mendler_fold<int, NatF>(mendler_count_algebra, 10, two) == 12);
}

TEST_CASE("MendlerFold - Constexpr") {
    constexpr auto result = [] consteval {
        auto three = ce_make_succ(ce_make_succ(ce_make_succ(ce_make_zero())));
        return constexpr_mendler_count(three);
    }();
    static_assert(result == 3);
    CHECK(result == 3);
}

TEST_CASE("MendlerPara - NatZero") {
    auto alg = [](auto const &recurse, int ctx, Nat const &,
                  NatF<Nat> const &layer) -> int {
        return std::visit(overloaded{
                              [&](Zero const &) -> int { return ctx; },
                              [&](Succ<Nat> const &s) -> int {
                                  return recurse(*s.pred, ctx + 1);
                              },
                          },
                          layer);
    };
    auto zero = make_zero();
    CHECK(mendler_para<int, NatF>(alg, 0, zero) == 0);
}

TEST_CASE("MendlerPara - AccessesOriginalNode") {
    auto alg = [](auto const &recurse, int ctx, Nat const &node,
                  NatF<Nat> const &layer) -> int {
        return std::visit(
            overloaded{
                [&](Zero const &) -> int {
                    (void)node;
                    return ctx;
                },
                [&](Succ<Nat> const &s) -> int {
                    auto const &inner = unwrap_fix(node);
                    (void)std::get<Succ<Nat>>(inner);
                    return recurse(*s.pred, ctx + 1);
                },
            },
            layer);
    };
    auto three = make_succ(make_succ(make_succ(make_zero())));
    CHECK(mendler_para<int, NatF>(alg, 0, three) == 3);
}

TEST_CASE("MendlerPara - Constexpr") {
    constexpr auto result = [] consteval {
        auto two = ce_make_succ(ce_make_succ(ce_make_zero()));
        return constexpr_mendler_para_depth(two);
    }();
    static_assert(result == 2);
    CHECK(result == 2);
}

TEST_CASE("MendlerFold - EquivalentToFoldFix") {
    for (int n = 0; n < 10; ++n) {
        auto nat = unfold_fix<NatF>(nat_coalgebra, fmap_nat_fn, n);
        auto via_fold = fold_fix<int>(count_algebra, fmap_nat_fn, nat);
        auto via_mendler =
            mendler_fold<int, NatF>(mendler_count_algebra, 0, nat);
        CHECK(via_fold == via_mendler);
    }
}
