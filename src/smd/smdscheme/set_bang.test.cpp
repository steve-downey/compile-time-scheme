// src/smd/smdscheme/set_bang.test.cpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Cross-backend behavioural tests for the `set!` and `begin` special forms.
// Each backend is driven through its own compile entry point under a
// store-backed (mutable) environment.  The decisive case is the shared counter:
// a closure captures a variable and mutates it across calls, which is only
// correct when every environment copy shares one store.

#include <smd/smdscheme/closure/closure_program.hpp>
#include <smd/smdscheme/sender/fixpoint_program.hpp>
#include <smd/smdscheme/sender/sender_cps_program.hpp>
#include <smd/smdscheme/sender/sender_mendler_program.hpp>
#include <smd/smdscheme/sender/sender_program.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {
using namespace smd::smdscheme;
using namespace std::string_view_literals;

constexpr int N = 128; ///< Core arena capacity (large enough for the counter).
using Core = elaborator::core_type<N, 16>;
using CompT = fixpoint_eval::Comp<16>;

/// A counter closure that mutates a captured binding across two calls.
/// Correct result is 2.
constexpr auto counter_src =
    "(let ((c 0))"
    "  (let ((f (lambda () (begin (set! c (+ c 1)) c))))"
    "    (begin (f) (f))))"sv;

// --- one store-backed runner per backend -------------------------------------

auto run_closure(std::string_view src)
    -> foundation::result<closure::value<Core>> {
    auto pr = closure::compile_to_closure<N, 16>(src);
    if (!pr.has_value())
        return foundation::result<closure::value<Core>>{pr.error()};
    closure::store<Core, closure::default_max_store> st;
    auto env = closure::default_env<Core, 16>(st);
    return pr.value()(env);
}

auto run_sender(std::string_view src)
    -> foundation::result<closure::value<Core>> {
    auto pr = sender::compile_to_sender<N, 16>(src);
    if (!pr.has_value())
        return foundation::result<closure::value<Core>>{pr.error()};
    closure::store<Core, closure::default_max_store> st;
    auto env = closure::default_env<Core, 16>(st);
    return pr.value()(env);
}

auto run_sender_cps(std::string_view src)
    -> foundation::result<closure::value<Core>> {
    auto pr = sender_cps::compile_sender_cps<N, 16>(src);
    if (!pr.has_value())
        return foundation::result<closure::value<Core>>{pr.error()};
    closure::store<Core, closure::default_max_store> st;
    auto env = closure::default_env<Core, 16>(st);
    return pr.value()(env);
}

auto run_fixpoint(std::string_view src)
    -> foundation::result<closure::value<CompT>> {
    auto pr = fixpoint_eval::compile_fixpoint<N, 16>(src);
    if (!pr.has_value())
        return foundation::result<closure::value<CompT>>{pr.error()};
    closure::store<CompT, closure::default_max_store> st;
    auto env = closure::default_env<CompT, 16>(st);
    return pr.value()(env);
}

auto run_mendler(std::string_view src)
    -> foundation::result<closure::value<CompT>> {
    auto pr = sender_mendler::compile_sender_mendler<N, 16>(src);
    if (!pr.has_value())
        return foundation::result<closure::value<CompT>>{pr.error()};
    closure::store<CompT, closure::default_max_store> st;
    auto env = closure::default_env<CompT, 16>(st);
    return pr.value()(env);
}

// --- shared assertions (backend-agnostic) ------------------------------------

template <class R> void expect_int(R const &r, int expected) {
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == expected);
}

template <class R> void expect_unspecified(R const &r) {
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<closure::unspecified>(r.value()));
}

/// Exercises every set!/begin behaviour through @p run.
template <class Run> void check_backend(Run run) {
    expect_int(run("(begin 1 2 3)"sv), 3);                        // sequencing
    expect_int(run("(let ((x 1)) (begin (set! x 2) x))"sv), 2);  // local mutate
    expect_unspecified(run("(let ((x 1)) (set! x 2))"sv));       // result value
    REQUIRE_FALSE(run("(set! nope 1)"sv).has_value());           // unbound
    expect_int(run(counter_src), 2);                             // shared cell
}

} // namespace

TEST_CASE("SetBang - ClosureCpsBackend") { check_backend(run_closure); }
TEST_CASE("SetBang - SenderBackend") { check_backend(run_sender); }
TEST_CASE("SetBang - SenderCpsBackend") { check_backend(run_sender_cps); }
TEST_CASE("SetBang - FixpointMendlerBackend") { check_backend(run_fixpoint); }
TEST_CASE("SetBang - SenderMendlerBackend") { check_backend(run_mendler); }
