// src/smd/smdscheme/sender/sender_program.test.cpp -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/sender/sender_program.hpp>
#include <smd/smdscheme/sender/sender_program.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

TEST_CASE("SenderBackendTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("SenderBackendTest - EvalIntegerNode") {
    using Core = smd::smdscheme::elaborator::core_type<32, 16>;

    Core node;
    node.inner = smd::smdscheme::elaborator::core_integer{42};

    smd::smdscheme::foundation::tree_arena<Core, 32> arena;
    auto e = smd::smdscheme::closure::default_env<Core, 16>();

    auto res = smd::smdscheme::sender::eval_node<32, 16, 16>(node, arena, e);

    REQUIRE(res.has_value());
    REQUIRE(std::holds_alternative<int>(res.value()));
    REQUIRE(std::get<int>(res.value()) == 42);
}

TEST_CASE("SenderBackendTest - CompileToSender") {
    auto program_res =
        smd::smdscheme::sender::compile_to_sender<32, 16>("(+ 1 (* 2 3))");
    REQUIRE(program_res.has_value());

    auto program = program_res.value();
    auto e       = smd::smdscheme::closure::default_env<
        smd::smdscheme::elaborator::core_type<32, 16>, 16>();

    auto res = program(e);

    REQUIRE(res.has_value());
    REQUIRE(std::holds_alternative<int>(res.value()));
    REQUIRE(std::get<int>(res.value()) == 7);
}
