// src/smd/schemepoc/fix.test.cpp                                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/fix.hpp>
#include <smd/schemepoc/fix.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <type_traits>
#include <variant>

// Minimal expression functor family for testing fix<F> and fold_fix.
// These types live here, not in fix.hpp; fix.hpp uses ADL to find fmap.

template <class A>
struct int_f {
    int value{};
};

template <class A>
struct add_f {
    A left;
    A right;
};

template <class A>
using expr_f = std::variant<int_f<A>, add_f<A>>;

template <class A, class Fn>
constexpr auto fmap(expr_f<A> const& layer, Fn fn) {
    using B = std::invoke_result_t<Fn, A const&>;
    return std::visit(
        [&](auto const& node) -> expr_f<B> {
            using T = std::remove_cvref_t<decltype(node)>;
            if constexpr (std::is_same_v<T, int_f<A>>) {
                return int_f<B>{node.value};
            } else {
                return add_f<B>{fn(node.left), fn(node.right)};
            }
        },
        layer);
}

using expr = smd::schemepoc::fix<expr_f>;

constexpr auto lit(int v) -> expr {
    return expr{expr_f<expr>{int_f<expr>{v}}};
}

constexpr auto add(expr l, expr r) -> expr {
    return expr{expr_f<expr>{add_f<expr>{std::move(l), std::move(r)}}};
}

constexpr auto eval_algebra = [](expr_f<int> layer) -> int {
    return std::visit(
        [](auto const& node) -> int {
            using T = std::remove_cvref_t<decltype(node)>;
            if constexpr (std::is_same_v<T, int_f<int>>) {
                return node.value;
            } else {
                return node.left + node.right;
            }
        },
        layer);
};

// (1 + 2) + 3 == 6
static_assert([] {
    using namespace smd::schemepoc;
    auto tree = add(add(lit(1), lit(2)), lit(3));
    return fold_fix<int>(tree, eval_algebra) == 6;
}());

TEST_CASE("FixTest - HeaderIsIdempotent") { REQUIRE(true); }
