// src/smd/smdscheme/closure/value.test.cpp                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/elaborator/elaborated_core.hpp>
using core = smd::smdscheme::elaborator::core_type<32, 16>;
#include <smd/smdscheme/closure/value.hpp>
#include <smd/smdscheme/closure/value.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <variant>

namespace {
using namespace smd::smdscheme;
using namespace std::string_view_literals;

static_assert([] {
    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  8>();
    auto r = e.lookup("+"sv);
    return r.has_value() &&
           std::holds_alternative<smd::smdscheme::closure::builtin>(
               r.value()) &&
           std::get<smd::smdscheme::closure::builtin>(r.value()).op ==
               smd::smdscheme::closure::builtin_op::add;
}());

static_assert([] {
    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  8>();
    auto r = e.lookup("*"sv);
    return r.has_value() &&
           std::holds_alternative<smd::smdscheme::closure::builtin>(
               r.value()) &&
           std::get<smd::smdscheme::closure::builtin>(r.value()).op ==
               smd::smdscheme::closure::builtin_op::multiply;
}());

static_assert([] {
    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  8>();
    auto r = e.lookup("x"sv);
    return !r.has_value();
}());

static_assert([] {
    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  8>();
    e.define("x"sv,
             smd::smdscheme::closure::value<elaborator::core_type<32, 16>>{42});
    auto r = e.lookup("x"sv);
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 42;
}());

static_assert([] {
    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  8>();
    e.define("x"sv,
             smd::smdscheme::closure::value<elaborator::core_type<32, 16>>{1});
    e.define("x"sv,
             smd::smdscheme::closure::value<elaborator::core_type<32, 16>>{2});
    auto r = e.lookup("x"sv);
    return r.has_value() && std::holds_alternative<int>(r.value()) &&
           std::get<int>(r.value()) == 2;
}());

} // namespace

TEST_CASE("ValueTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ValueTest - DefaultEnvHasAdd") {
    using namespace smd::smdscheme;
    using namespace std::string_view_literals;
    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  8>();
    auto r = e.lookup("+"sv);
    REQUIRE(r.has_value());
    REQUIRE(
        std::holds_alternative<smd::smdscheme::closure::builtin>(r.value()));
    REQUIRE(std::get<smd::smdscheme::closure::builtin>(r.value()).op ==
            smd::smdscheme::closure::builtin_op::add);
}

TEST_CASE("ValueTest - DefaultEnvHasMultiply") {
    using namespace smd::smdscheme;
    using namespace std::string_view_literals;
    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  8>();
    auto r = e.lookup("*"sv);
    REQUIRE(r.has_value());
    REQUIRE(
        std::holds_alternative<smd::smdscheme::closure::builtin>(r.value()));
    REQUIRE(std::get<smd::smdscheme::closure::builtin>(r.value()).op ==
            smd::smdscheme::closure::builtin_op::multiply);
}

TEST_CASE("ValueTest - UnboundVariableIsError") {
    using namespace smd::smdscheme;
    using namespace std::string_view_literals;
    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  8>();
    auto r = e.lookup("x"sv);
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("ValueTest - DefineAndLookup") {
    using namespace smd::smdscheme;
    using namespace std::string_view_literals;
    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  8>();
    e.define("x"sv,
             smd::smdscheme::closure::value<elaborator::core_type<32, 16>>{42});
    auto r = e.lookup("x"sv);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<int>(r.value()));
    REQUIRE(std::get<int>(r.value()) == 42);
}

TEST_CASE("ValueTest - DefineShadows") {
    using namespace smd::smdscheme;
    using namespace std::string_view_literals;
    auto e = smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                                  8>();
    e.define("x"sv,
             smd::smdscheme::closure::value<elaborator::core_type<32, 16>>{1});
    e.define("x"sv,
             smd::smdscheme::closure::value<elaborator::core_type<32, 16>>{2});
    auto r = e.lookup("x"sv);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 2);
}

TEST_CASE("ValueTest - StoreAllocGetSet") {
    using Val = closure::value<core>;
    closure::store<core, closure::default_max_store> st;
    int a = st.alloc(Val{1});
    int b = st.alloc(Val{2});
    REQUIRE(a != b);
    REQUIRE(std::get<int>(st.get(a)) == 1);
    REQUIRE(std::get<int>(st.get(b)) == 2);
    st.set(a, Val{42});
    REQUIRE(std::get<int>(st.get(a)) == 42);
    REQUIRE(std::get<int>(st.get(b)) == 2); // untouched
}

TEST_CASE("ValueTest - AssignMutatesBoundInStore") {
    closure::store<core, closure::default_max_store> st;
    auto e = closure::default_env<core, 8>(st);
    e.define("x"sv, closure::value<core>{1});

    auto ar = e.assign("x"sv, closure::value<core>{99});
    REQUIRE(ar.has_value());
    REQUIRE(std::holds_alternative<closure::unspecified>(ar.value()));

    auto r = e.lookup("x"sv);
    REQUIRE(r.has_value());
    REQUIRE(std::get<int>(r.value()) == 99);
}

TEST_CASE("ValueTest - AssignUnboundIsError") {
    closure::store<core, closure::default_max_store> st;
    auto e = closure::default_env<core, 8>(st);
    REQUIRE_FALSE(e.assign("nope"sv, closure::value<core>{1}).has_value());
}

TEST_CASE("ValueTest - AssignWithoutStoreIsError") {
    // Functional (no-store) env cannot support set!.
    auto e = closure::default_env<core, 8>();
    e.define("x"sv, closure::value<core>{1});
    REQUIRE_FALSE(e.assign("x"sv, closure::value<core>{2}).has_value());
}

TEST_CASE("ValueTest - PairHeapAllocGetAt") {
    using Val = closure::value<core>;
    closure::pair_heap<core, closure::default_max_pairs> heap;
    int a = heap.alloc({Val{1}, Val{2}});
    int b = heap.alloc({Val{3}, closure::value<core>{closure::null_t{}}});
    REQUIRE(a != b);
    REQUIRE(std::get<int>(heap.get(a).car) == 1);
    REQUIRE(std::get<int>(heap.get(a).cdr) == 2);
    REQUIRE(std::get<int>(heap.get(b).car) == 3);
    REQUIRE(std::holds_alternative<closure::null_t>(heap.get(b).cdr));
    // set-car! / set-cdr! mutate in place through at().
    heap.at(a).car = Val{42};
    REQUIRE(std::get<int>(heap.get(a).car) == 42);
    REQUIRE(std::get<int>(heap.get(b).car) == 3); // untouched
}

TEST_CASE("ValueTest - PairRefIdentity") {
    // eq? on pairs is handle identity, not structure.
    REQUIRE(closure::pair_ref{3} == closure::pair_ref{3});
    REQUIRE_FALSE(closure::pair_ref{3} == closure::pair_ref{4});
}

TEST_CASE("ValueTest - EqualIsStructural") {
    using Val = closure::value<core>;
    closure::pair_heap<core, closure::default_max_pairs> heap;
    // Two distinct cells (1 . 2): different handles, structurally equal.
    Val p{closure::pair_ref{heap.alloc({Val{1}, Val{2}})}};
    Val q{closure::pair_ref{heap.alloc({Val{1}, Val{2}})}};
    REQUIRE_FALSE(std::get<closure::pair_ref>(p) ==
                  std::get<closure::pair_ref>(q)); // eq? => false
    REQUIRE(closure::values_equal(p, q, heap));    // equal? => true
    // Distinct structure compares unequal.
    Val r{closure::pair_ref{heap.alloc({Val{1}, Val{9}})}};
    REQUIRE_FALSE(closure::values_equal(p, r, heap));
    // Nested pairs recurse.
    Val np{closure::pair_ref{heap.alloc({p, Val{closure::null_t{}}})}};
    Val nq{closure::pair_ref{heap.alloc({q, Val{closure::null_t{}}})}};
    REQUIRE(closure::values_equal(np, nq, heap));
}

static_assert([] {
    using Val = closure::value<core>;
    closure::pair_heap<core, closure::default_max_pairs> heap;
    int loc = heap.alloc({Val{10}, Val{20}});
    heap.at(loc).cdr = Val{99};
    return std::get<int>(heap.get(loc).car) == 10 &&
           std::get<int>(heap.get(loc).cdr) == 99;
}());
