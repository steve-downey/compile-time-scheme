// src/smd/smdscheme/pairs.test.cpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Cross-backend behavioural tests for pairs and the list primitives (cons, car,
// cdr, set-car!, set-cdr!, pair?, null?, list, eq?/eqv?/equal?, and '()).  Each
// backend is driven through its own compile entry point under an environment
// backed by both a store (for set!) and a pair heap (for cons).  The decisive
// case is aliasing: two names bound to the same cell must both observe a
// set-car!, which is only correct when a pair is a shared handle rather than a
// deep copy.

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

constexpr int N = 128; ///< Core arena capacity.
using Core = elaborator::core_type<N, 16>;
using CompT = fixpoint_eval::Comp<16>;

// --- one store+heap-backed runner per backend --------------------------------

auto run_closure(std::string_view src)
    -> foundation::result<closure::value<Core>> {
    auto pr = closure::compile_to_closure<N, 16>(src);
    if (!pr.has_value())
        return foundation::result<closure::value<Core>>{pr.error()};
    closure::store<Core, closure::default_max_store> st;
    closure::pair_heap<Core, closure::default_max_pairs> ph;
    auto env = closure::default_env<Core, 16>(st, ph);
    return pr.value()(env);
}

auto run_sender(std::string_view src)
    -> foundation::result<closure::value<Core>> {
    auto pr = sender::compile_to_sender<N, 16>(src);
    if (!pr.has_value())
        return foundation::result<closure::value<Core>>{pr.error()};
    closure::store<Core, closure::default_max_store> st;
    closure::pair_heap<Core, closure::default_max_pairs> ph;
    auto env = closure::default_env<Core, 16>(st, ph);
    return pr.value()(env);
}

auto run_sender_cps(std::string_view src)
    -> foundation::result<closure::value<Core>> {
    auto pr = sender_cps::compile_sender_cps<N, 16>(src);
    if (!pr.has_value())
        return foundation::result<closure::value<Core>>{pr.error()};
    closure::store<Core, closure::default_max_store> st;
    closure::pair_heap<Core, closure::default_max_pairs> ph;
    auto env = closure::default_env<Core, 16>(st, ph);
    return pr.value()(env);
}

auto run_fixpoint(std::string_view src)
    -> foundation::result<closure::value<CompT>> {
    auto pr = fixpoint_eval::compile_fixpoint<N, 16>(src);
    if (!pr.has_value())
        return foundation::result<closure::value<CompT>>{pr.error()};
    closure::store<CompT, closure::default_max_store> st;
    closure::pair_heap<CompT, closure::default_max_pairs> ph;
    auto env = closure::default_env<CompT, 16>(st, ph);
    return pr.value()(env);
}

auto run_mendler(std::string_view src)
    -> foundation::result<closure::value<CompT>> {
    auto pr = sender_mendler::compile_sender_mendler<N, 16>(src);
    if (!pr.has_value())
        return foundation::result<closure::value<CompT>>{pr.error()};
    closure::store<CompT, closure::default_max_store> st;
    closure::pair_heap<CompT, closure::default_max_pairs> ph;
    auto env = closure::default_env<CompT, 16>(st, ph);
    return pr.value()(env);
}

// --- shared assertions (backend-agnostic) ------------------------------------

template <class R>
void expect_int(R const &r, int expected) {
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == expected);
}

template <class R>
void expect_bool(R const &r, bool expected) {
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<bool>(r.value()));
    REQUIRE(std::get<bool>(r.value()) == expected);
}

template <class R>
void expect_unspecified(R const &r) {
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<closure::unspecified>(r.value()));
}

/// Exercises every pair/list behaviour through @p run.  Every case reduces to a
/// scalar/bool/unspecified because pair handles cannot outlive the per-run heap
/// (the same boundary compile-time evaluation has).
template <class Run>
void check_backend(Run run) {
    // Construction and access.
    expect_int(run("(car (cons 10 20))"sv), 10);
    expect_int(run("(cdr (cons 10 20))"sv), 20);
    expect_int(run("(car (cdr (list 1 2 3)))"sv), 2);
    expect_int(run("(car '(7 8 9))"sv), 7);

    // Predicates and the empty list.
    expect_bool(run("(null? '())"sv), true);
    expect_bool(run("(null? '(1))"sv), false);
    expect_bool(run("(null? (list))"sv), true);
    expect_bool(run("(pair? (cons 1 2))"sv), true);
    expect_bool(run("(pair? '())"sv), false);
    expect_bool(run("(pair? 5)"sv), false);

    // Aliasing / shared mutation — impossible with deep-copy pairs.
    expect_int(run("(let ((p (cons 1 2)))"
                   "  (let ((q p)) (begin (set-car! q 9) (car p))))"sv),
               9);
    expect_unspecified(run("(let ((p (cons 1 2))) (set-car! p 5))"sv));

    // Identity vs structure.
    expect_bool(run("(let ((p (cons 1 2))) (eq? p p))"sv), true);
    expect_bool(run("(eq? (cons 1 2) (cons 1 2))"sv), false);
    expect_bool(run("(equal? (cons 1 2) (cons 1 2))"sv), true);
    expect_bool(run("(equal? '(1 2 3) '(1 2 3))"sv), true);
    expect_bool(run("(equal? '(1 2 3) '(1 2 9))"sv), false);

    // Errors.
    REQUIRE_FALSE(run("(car 5)"sv).has_value());  // not a pair
    REQUIRE_FALSE(run("(cons 1)"sv).has_value()); // arity
}

} // namespace

TEST_CASE("Pairs - ClosureCpsBackend") { check_backend(run_closure); }
TEST_CASE("Pairs - SenderBackend") { check_backend(run_sender); }
TEST_CASE("Pairs - SenderCpsBackend") { check_backend(run_sender_cps); }
TEST_CASE("Pairs - FixpointMendlerBackend") { check_backend(run_fixpoint); }
TEST_CASE("Pairs - SenderMendlerBackend") { check_backend(run_mendler); }

TEST_CASE("Pairs - RuntimeListEscapesToHost") {
    // "Merely compiled at compile time": the program is a constexpr-capable
    // object, but here it is evaluated at runtime against a caller-owned heap
    // that outlives the call — so the resulting list is a live value the host
    // can walk.  This is exactly what cannot cross the boundary of a *compile
    // time* evaluation, where the heap is transient.
    auto pr = closure::compile_to_closure<N, 16>("(list 10 20 30)"sv);
    REQUIRE(pr.has_value());
    closure::store<Core, closure::default_max_store> st;
    closure::pair_heap<Core, closure::default_max_pairs> heap; // outlives call
    auto env = closure::default_env<Core, 16>(st, heap);

    auto r = pr.value()(env);
    REQUIRE(r.has_value());

    // Walk the returned list through the still-alive heap.
    int sum = 0;
    int count = 0;
    auto cur = r.value();
    while (std::holds_alternative<closure::pair_ref>(cur)) {
        auto const &cell = heap.get(std::get<closure::pair_ref>(cur).loc);
        REQUIRE(std::holds_alternative<int>(cell.car));
        sum += std::get<int>(cell.car);
        ++count;
        cur = cell.cdr;
    }
    REQUIRE(std::holds_alternative<closure::null_t>(cur)); // proper list
    REQUIRE(count == 3);
    REQUIRE(sum == 60);
}
