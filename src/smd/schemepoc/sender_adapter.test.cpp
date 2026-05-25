// src/smd/schemepoc/sender_adapter.test.cpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/sender_adapter.hpp>
#include <smd/schemepoc/sender_adapter.hpp> // idempotency

#include <catch2/catch_test_macros.hpp>
#include <tuple>
#include <optional>

TEST_CASE("SenderAdapter - just and sync_wait") {
    auto s = smd::schemepoc::sender_v::just(42);
    auto result = smd::schemepoc::sender_v::sync_wait(s);
    REQUIRE(std::get<0>(result.value()) == 42);
}

TEST_CASE("SenderAdapter - then") {
    auto s = smd::schemepoc::sender_v::then(
        smd::schemepoc::sender_v::just(21),
        [](int x) { return x * 2; }
    );
    auto result = smd::schemepoc::sender_v::sync_wait(s);
    REQUIRE(std::get<0>(result.value()) == 42);
}

TEST_CASE("SenderAdapter - when_all") {
    auto all_s = smd::schemepoc::sender_v::when_all(
        smd::schemepoc::sender_v::just(21),
        smd::schemepoc::sender_v::just(2)
    );
    
    auto s = smd::schemepoc::sender_v::then(
        all_s,
        [](int a, int b) { return a * b; }
    );
    
    auto result = smd::schemepoc::sender_v::sync_wait(s);
    REQUIRE(std::get<0>(result.value()) == 42);
}

#include <smd/schemepoc/value.hpp>
#include <smd/schemepoc/elaborator_core.hpp>

using Core = smd::schemepoc::core_type<32, 16>;

TEST_CASE("SenderAdapter - Maps Dynamically to SchemePoC Values") {
    using val_t = smd::schemepoc::value<Core>;
    using res_t = smd::schemepoc::result<val_t>;
    
    // Simulate lookup / literal
    auto v1 = res_t{val_t{10}};
    auto v2 = res_t{val_t{20}};
    
    auto s1 = smd::schemepoc::sender_v::just(v1);
    auto s2 = smd::schemepoc::sender_v::just(v2);
    
    auto all_s = smd::schemepoc::sender_v::when_all(s1, s2);
    
    // Simulate apply
    auto s = smd::schemepoc::sender_v::then(
        all_s,
        [](res_t a, res_t b) -> res_t {
            if (!a.has_value() || !b.has_value()) return smd::schemepoc::parse_error{{}, "error"};
            if (!std::holds_alternative<int>(a.value()) || !std::holds_alternative<int>(b.value())) return smd::schemepoc::parse_error{{}, "type error"};
            return res_t{val_t{std::get<int>(a.value()) + std::get<int>(b.value())}};
        }
    );
    
    auto result = smd::schemepoc::sender_v::sync_wait(s);
    auto final_res = std::get<0>(result.value());
    REQUIRE(final_res.has_value());
    REQUIRE(std::get<int>(final_res.value()) == 30);
}
