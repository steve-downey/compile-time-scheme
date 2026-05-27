// src/smd/smdscheme/sender/sender_backend.test.cpp             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/sender/sender_backend.hpp>
#include <smd/smdscheme/sender/sender_backend.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

TEST_CASE("SenderBackendTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("SenderBackendTest - CompileIntegerNode") {
    using Core = smd::smdscheme::elaborator::core_type<32, 16>;

    Core node;
    node.inner = smd::smdscheme::elaborator::core_integer{42};

    smd::smdscheme::foundation::tree_arena<Core, 32> arena;
    auto e = smd::smdscheme::closure::default_env<Core, 16>();

    auto s = smd::smdscheme::sender::sender_backend::compile_node<32, 16, 16>(
        node, arena, e);
    auto res = smd::smdscheme::sender_v::sync_wait(std::move(s));

    REQUIRE(res.has_value());
    REQUIRE(std::get<0>(res.value()).has_value());
    REQUIRE(std::holds_alternative<int>(std::get<0>(res.value()).value()));
    REQUIRE(std::get<int>(std::get<0>(res.value()).value()) == 42);
}

TEST_CASE("SenderBackendTest - CompileToSender") {
    auto program_res =
        smd::smdscheme::sender::compile_to_sender<32, 16>("(+ 1 (* 2 3))");
    REQUIRE(program_res.has_value());

    auto program = program_res.value();
    auto e = smd::smdscheme::closure::default_env<
        smd::smdscheme::elaborator::core_type<32, 16>, 16>();

    auto s = program(e);
    auto res = smd::smdscheme::sender_v::sync_wait(std::move(s));

    REQUIRE(res.has_value());
    REQUIRE(std::get<0>(res.value()).has_value());
    REQUIRE(std::holds_alternative<int>(std::get<0>(res.value()).value()));
    REQUIRE(std::get<int>(std::get<0>(res.value()).value()) == 7);
}
