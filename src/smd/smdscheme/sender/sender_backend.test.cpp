// src/smd/schemepoc/sender_backend.test.cpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/elaborator.hpp>
#include <smd/schemepoc/sender_backend.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("SenderBackend - map literal integer") {
    using Core = smd::schemepoc::core_type<32, 16>;
    Core node;
    node.inner = smd::schemepoc::core_integer{42};
    smd::schemepoc::tree_arena<Core, 32> arena;
    smd::schemepoc::env<Core, 16> env;

    auto s = smd::schemepoc::sender_backend::compile_node<32, 16, 16>(
        node, arena, env);
    auto res = smd::schemepoc::sender_v::sync_wait(std::move(s));
    REQUIRE(std::get<0>(res.value()).has_value());
    REQUIRE(std::holds_alternative<int>(std::get<0>(res.value()).value()));
    REQUIRE(std::get<int>(std::get<0>(res.value()).value()) == 42);
}

TEST_CASE("SenderBackend - map literal boolean") {
    using Core = smd::schemepoc::core_type<32, 16>;
    Core node;
    node.inner = smd::schemepoc::core_boolean{true};
    smd::schemepoc::tree_arena<Core, 32> arena;
    smd::schemepoc::env<Core, 16> env;

    auto s = smd::schemepoc::sender_backend::compile_node<32, 16, 16>(
        node, arena, env);
    auto res = smd::schemepoc::sender_v::sync_wait(std::move(s));
    REQUIRE(std::get<0>(res.value()).has_value());
    REQUIRE(std::holds_alternative<bool>(std::get<0>(res.value()).value()));
    REQUIRE(std::get<bool>(std::get<0>(res.value()).value()) == true);
}

TEST_CASE("SenderBackend - map lookup") {
    using Core = smd::schemepoc::core_type<32, 16>;
    smd::schemepoc::tree_arena<Core, 32> arena;
    smd::schemepoc::env<Core, 16> env;
    env.define("x", smd::schemepoc::value<Core>{100});
    Core node;
    node.inner = smd::schemepoc::core_symbol{"x"};

    auto s = smd::schemepoc::sender_backend::compile_node<32, 16, 16>(
        node, arena, env);
    auto res = smd::schemepoc::sender_v::sync_wait(std::move(s));
    REQUIRE(std::get<0>(res.value()).has_value());
    REQUIRE(std::holds_alternative<int>(std::get<0>(res.value()).value()));
    REQUIRE(std::get<int>(std::get<0>(res.value()).value()) == 100);
}

TEST_CASE("SenderBackend - map arithmetic apply") {
    using Core = smd::schemepoc::core_type<32, 16>;
    smd::schemepoc::tree_arena<Core, 32> arena;
    smd::schemepoc::env<Core, 16> env;

    env.define("+", smd::schemepoc::value<Core>{smd::schemepoc::builtin{
                        smd::schemepoc::builtin_op::add}});

    Core func_node;
    func_node.inner = smd::schemepoc::core_symbol{"+"};
    Core a_node;
    a_node.inner = smd::schemepoc::core_integer{20};
    Core b_node;
    b_node.inner = smd::schemepoc::core_integer{22};

    Core app_node;
    smd::schemepoc::core_application<Core, 32, 16> app;
    app.func = smd::schemepoc::make_arena_box(arena, func_node);
    app.args.push_back(smd::schemepoc::make_arena_box(arena, a_node));
    app.args.push_back(smd::schemepoc::make_arena_box(arena, b_node));
    app_node.inner = app;

    auto s = smd::schemepoc::sender_backend::compile_node<32, 16, 16>(
        app_node, arena, env);
    auto res = smd::schemepoc::sender_v::sync_wait(std::move(s));
    REQUIRE(std::get<0>(res.value()).has_value());
    REQUIRE(std::holds_alternative<int>(std::get<0>(res.value()).value()));
    REQUIRE(std::get<int>(std::get<0>(res.value()).value()) == 42);
}

TEST_CASE("SenderBackend - compile to sender") {
    auto program_res =
        smd::schemepoc::compile_to_sender<32, 16>("(+ 1 (* 2 3))");
    REQUIRE(program_res.has_value());

    auto program = program_res.value();
    smd::schemepoc::env<smd::schemepoc::core_type<32, 16>, 16> env;
    env.define("+",
               smd::schemepoc::value<smd::schemepoc::core_type<32, 16>>{
                   smd::schemepoc::builtin{smd::schemepoc::builtin_op::add}});
    env.define("*", smd::schemepoc::value<smd::schemepoc::core_type<32, 16>>{
                        smd::schemepoc::builtin{
                            smd::schemepoc::builtin_op::multiply}});

    auto s = program(env);
    auto res = smd::schemepoc::sender_v::sync_wait(std::move(s));

    REQUIRE(std::get<0>(res.value()).has_value());
    REQUIRE(std::holds_alternative<int>(std::get<0>(res.value()).value()));
    REQUIRE(std::get<int>(std::get<0>(res.value()).value()) == 7);
}
