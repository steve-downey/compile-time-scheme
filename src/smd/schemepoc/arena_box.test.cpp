// src/smd/schemepoc/arena_box.test.cpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/arena_box.hpp>
#include <smd/schemepoc/static_vector.hpp>
#include <catch2/catch_test_macros.hpp>
#include <variant>

namespace smd::schemepoc {

template <template <typename> class F>
struct Fix {
    F<Fix<F>> inner;
};

template <typename T>
using test_box = arena_box<T, 1024>;

template <typename R>
struct ExprF {
    struct Lit { int value; };
    struct Add {
        test_box<R> left;
        test_box<R> right;
    };
    
    std::variant<Lit, Add> value;
};

using Expr = Fix<ExprF>;

constexpr auto evaluate(const Expr& expr) -> int {
    if (std::holds_alternative<ExprF<Expr>::Lit>(expr.inner.value)) {
        return std::get<ExprF<Expr>::Lit>(expr.inner.value).value;
    } else {
        auto add = std::get<ExprF<Expr>::Add>(expr.inner.value);
        return evaluate(*add.left) + evaluate(*add.right);
    }
}

constexpr auto make_lit(int v) -> Expr {
    return Expr{ExprF<Expr>{ExprF<Expr>::Lit{v}}};
}

constexpr auto make_add(tree_arena<Expr, 1024>& arena, Expr left, Expr right) -> Expr {
    return Expr{ExprF<Expr>{ExprF<Expr>::Add{
        make_arena_box(arena, left),
        make_arena_box(arena, right)
    }}};
}

constexpr auto test_eval() -> int {
    tree_arena<Expr, 1024> arena;
    Expr left = make_lit(10);
    Expr right = make_lit(32);
    Expr add = make_add(arena, left, right);
    return evaluate(add);
}

static_assert(test_eval() == 42);

TEST_CASE("ArenaBoxTest - SimpleEval") {
    REQUIRE(test_eval() == 42);
}

} // namespace smd::schemepoc
