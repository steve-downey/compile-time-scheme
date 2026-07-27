// src/smd/smdlisp/sender/sender_v.test.cpp                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/sender/sender_v.hpp>
#include <smd/smdlisp/sender/sender_v.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <concepts>

namespace {
namespace sv = smd::smdlisp::sender_v;
} // namespace

TEST_CASE("SenderVTest - HeaderIsIdempotent") { REQUIRE(true); }

// The reason this namespace exists at all rather than reusing
// `smd::smdscheme::sender_v`: the Scheme backend only ever needed the value
// channel, so its vocabulary has no error or stopped factories, and
// `smd::smdscheme` is frozen (D1) so it cannot grow them.  These assertions
// pin the additions, since silently losing one would show up much later as a
// confusing failure inside the evaluator.
static_assert(beman::execution::sender<decltype(sv::just(1))>);
static_assert(beman::execution::sender<decltype(sv::just_error(1))>);
static_assert(beman::execution::sender<decltype(sv::just_stopped())>);

// The tag vocabulary a hand-written sender/receiver/operation state must name.
static_assert(std::same_as<sv::sender_tag, beman::execution::sender_tag>);
static_assert(std::same_as<sv::receiver_tag, beman::execution::receiver_tag>);
static_assert(std::same_as<sv::operation_state_tag,
                           beman::execution::operation_state_tag>);
static_assert(std::same_as<sv::set_value_t, beman::execution::set_value_t>);
static_assert(std::same_as<sv::set_error_t, beman::execution::set_error_t>);
static_assert(std::same_as<sv::set_stopped_t, beman::execution::set_stopped_t>);

TEST_CASE("SenderVTest - VocabularyIsUsable") {
    // `sync_wait` is kept for the few value-only joins the evaluator builds;
    // the control-flow-bearing paths use connect/start instead.
    auto const r =
        sv::sync_wait(sv::then(sv::just(20), [](int x) { return x + 22; }));
    REQUIRE(r.has_value());
    REQUIRE(std::get<0>(r.value()) == 42);
}
