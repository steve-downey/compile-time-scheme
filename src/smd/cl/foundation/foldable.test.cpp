// src/smd/cl/foundation/foldable.test.cpp                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Shim smoke test (step R8, decision 2 corrected): the substantive
// Foldable law tests moved to src/smd/kit/foundation/foldable.test.cpp
// with the definition. This file only verifies that the forwarding shim
// compiles and that the forwarded names are usable from
// smd::cl::foundation exactly as before.

#include <smd/cl/foundation/foldable.hpp>
#include <smd/cl/foundation/foldable.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::cl::foundation::fold_left;
using smd::cl::foundation::foldable;
using smd::cl::foundation::foldable_typeclass;
using smd::cl::foundation::length;

TEST_CASE("FoldableShimTest - HeaderIsIdempotent") { REQUIRE(true); }

namespace {

template <class T>
struct pair_box {
    T first;
    T second;
};

struct pair_box_foldable_impl {
    template <class F, class T, class M>
    constexpr auto fold_map(this auto &&, F &&f, pair_box<T> const &pb,
                            M const &m) {
        return m.combine(f(pb.first), f(pb.second));
    }

    template <class F, class Acc, class T>
    constexpr auto fold_right(this auto &&, F &&f, Acc init,
                              pair_box<T> const &pb) -> Acc {
        return f(pb.first, f(pb.second, std::move(init)));
    }
};

struct pair_box_foldable_map : foldable<pair_box_foldable_impl> {
    using pair_box_foldable_impl::fold_map;
    using pair_box_foldable_impl::fold_right;
};

} // namespace

// foldable_typeclass now lives in smd::kit::foundation (step R8): a
// specialization must be declared where the primary template actually
// lives, not merely where a using-declaration makes its name callable.
namespace smd::kit::foundation {
template <class T>
inline constexpr auto foldable_typeclass<pair_box<T>> = pair_box_foldable_map{};
}

TEST_CASE("FoldableShimTest - ForwardedLengthAndFoldLeftCpoWork") {
    pair_box<int> const xs{1, 2};
    CHECK(length(xs) == 2);
    CHECK(fold_left([](int acc, int x) { return acc + x; }, 0, xs) == 3);
}
