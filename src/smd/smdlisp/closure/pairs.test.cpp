// src/smd/smdlisp/closure/pairs.test.cpp                         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/closure/pairs.hpp>
#include <smd/smdlisp/closure/pairs.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <variant>

namespace {

/// Stand-in core AST type; the pairs machinery only needs some type to
/// instantiate @c value<Core>/@c pair_heap<Core,...> with. The real core
/// arena arrives in step L10.
struct dummy_core {};

using val = smd::smdlisp::closure::value<dummy_core>;
using smd::smdlisp::closure::apply_prim;
using smd::smdlisp::closure::list_op;
using smd::smdlisp::closure::nil_t;
using smd::smdlisp::closure::pair_cell;
using smd::smdlisp::closure::pair_heap;
using smd::smdlisp::closure::pair_ref;
using smd::smdlisp::closure::symbol;

// Merge criteria (docs/cl-pivot-plan.md, Step L8): a quoted-list value
// structure satisfies the car/cdr/null laws, including the nil cases:
// (car nil) = nil, (cdr nil) = nil, (null nil) is true, (atom nil) is true,
// (atom (cons 1 nil)) is false, and eq/eql on symbols/integers/nil.

static_assert([] {
    // (car nil) = nil.
    pair_heap<dummy_core, 4> heap;
    std::array<val, 1> args{val{nil_t{}}};
    auto r = apply_prim<dummy_core>(list_op::car, args, &heap);
    return r.has_value() && std::holds_alternative<nil_t>(r.value());
}());

static_assert([] {
    // (cdr nil) = nil.
    pair_heap<dummy_core, 4> heap;
    std::array<val, 1> args{val{nil_t{}}};
    auto r = apply_prim<dummy_core>(list_op::cdr, args, &heap);
    return r.has_value() && std::holds_alternative<nil_t>(r.value());
}());

static_assert([] {
    // (null nil) is true (the canonical true value, symbol T).
    pair_heap<dummy_core, 4> heap;
    std::array<val, 1> args{val{nil_t{}}};
    auto r = apply_prim<dummy_core>(list_op::null, args, &heap);
    return r.has_value() && std::holds_alternative<symbol>(r.value()) &&
           std::get<symbol>(r.value()) == symbol{"T"};
}());

static_assert([] {
    // (null (cons 1 nil)) is nil (false): a cons cell is not null.
    pair_heap<dummy_core, 4> heap;
    std::array<val, 2> cons_args{val{1}, val{nil_t{}}};
    auto consed = apply_prim<dummy_core>(list_op::cons, cons_args, &heap);
    std::array<val, 1> args{consed.value()};
    auto r = apply_prim<dummy_core>(list_op::null, args, &heap);
    return r.has_value() && std::holds_alternative<nil_t>(r.value());
}());

static_assert([] {
    // (atom nil) is true: nil is an atom, not a cons cell.
    pair_heap<dummy_core, 4> heap;
    std::array<val, 1> args{val{nil_t{}}};
    auto r = apply_prim<dummy_core>(list_op::atom, args, &heap);
    return r.has_value() && std::holds_alternative<symbol>(r.value()) &&
           std::get<symbol>(r.value()) == symbol{"T"};
}());

static_assert([] {
    // (atom (cons 1 nil)) is false: a cons cell is not an atom.
    pair_heap<dummy_core, 4> heap;
    std::array<val, 2> cons_args{val{1}, val{nil_t{}}};
    auto consed = apply_prim<dummy_core>(list_op::cons, cons_args, &heap);
    std::array<val, 1> args{consed.value()};
    auto r = apply_prim<dummy_core>(list_op::atom, args, &heap);
    return r.has_value() && std::holds_alternative<nil_t>(r.value());
}());

static_assert([] {
    // eq/eql on symbols: (eq 'foo 'foo) is true, (eq 'foo 'bar) is false.
    pair_heap<dummy_core, 4> heap;
    std::array<val, 2> same{val{symbol{"FOO"}}, val{symbol{"FOO"}}};
    std::array<val, 2> diff{val{symbol{"FOO"}}, val{symbol{"BAR"}}};
    auto r_same = apply_prim<dummy_core>(list_op::eq, same, &heap);
    auto r_diff = apply_prim<dummy_core>(list_op::eq, diff, &heap);
    auto rl_same = apply_prim<dummy_core>(list_op::eql, same, &heap);
    auto rl_diff = apply_prim<dummy_core>(list_op::eql, diff, &heap);
    return r_same.has_value() &&
           std::holds_alternative<symbol>(r_same.value()) &&
           r_diff.has_value() &&
           std::holds_alternative<nil_t>(r_diff.value()) &&
           rl_same.has_value() &&
           std::holds_alternative<symbol>(rl_same.value()) &&
           rl_diff.has_value() &&
           std::holds_alternative<nil_t>(rl_diff.value());
}());

static_assert([] {
    // eq/eql on integers: (eq 1 1) is true, (eq 1 2) is false.
    pair_heap<dummy_core, 4> heap;
    std::array<val, 2> same{val{1}, val{1}};
    std::array<val, 2> diff{val{1}, val{2}};
    auto r_same = apply_prim<dummy_core>(list_op::eq, same, &heap);
    auto r_diff = apply_prim<dummy_core>(list_op::eq, diff, &heap);
    auto rl_same = apply_prim<dummy_core>(list_op::eql, same, &heap);
    auto rl_diff = apply_prim<dummy_core>(list_op::eql, diff, &heap);
    return r_same.has_value() &&
           std::holds_alternative<symbol>(r_same.value()) &&
           r_diff.has_value() &&
           std::holds_alternative<nil_t>(r_diff.value()) &&
           rl_same.has_value() &&
           std::holds_alternative<symbol>(rl_same.value()) &&
           rl_diff.has_value() &&
           std::holds_alternative<nil_t>(rl_diff.value());
}());

static_assert([] {
    // eq/eql on nil: (eq nil nil) is true.
    pair_heap<dummy_core, 4> heap;
    std::array<val, 2> both_nil{val{nil_t{}}, val{nil_t{}}};
    auto r = apply_prim<dummy_core>(list_op::eq, both_nil, &heap);
    auto rl = apply_prim<dummy_core>(list_op::eql, both_nil, &heap);
    return r.has_value() && std::holds_alternative<symbol>(r.value()) &&
           rl.has_value() && std::holds_alternative<symbol>(rl.value());
}());

static_assert([] {
    // A quoted-list value structure: '(1 2 3), built bottom-up with cons,
    // satisfies car/cdr/null laws all the way to the nil tail.
    pair_heap<dummy_core, 8> heap;
    val tail{nil_t{}};
    std::array<val, 2> c3_args{val{3}, tail};
    auto c3 = apply_prim<dummy_core>(list_op::cons, c3_args, &heap).value();
    std::array<val, 2> c2_args{val{2}, c3};
    auto c2 = apply_prim<dummy_core>(list_op::cons, c2_args, &heap).value();
    std::array<val, 2> c1_args{val{1}, c2};
    auto c1 = apply_prim<dummy_core>(list_op::cons, c1_args, &heap).value();

    std::array<val, 1> a1{c1};
    auto car1 = apply_prim<dummy_core>(list_op::car, a1, &heap).value();
    auto cdr1 = apply_prim<dummy_core>(list_op::cdr, a1, &heap).value();
    std::array<val, 1> a2{cdr1};
    auto car2 = apply_prim<dummy_core>(list_op::car, a2, &heap).value();
    auto cdr2 = apply_prim<dummy_core>(list_op::cdr, a2, &heap).value();
    std::array<val, 1> a3{cdr2};
    auto car3 = apply_prim<dummy_core>(list_op::car, a3, &heap).value();
    auto cdr3 = apply_prim<dummy_core>(list_op::cdr, a3, &heap).value();

    return std::holds_alternative<int>(car1) && std::get<int>(car1) == 1 &&
           std::holds_alternative<int>(car2) && std::get<int>(car2) == 2 &&
           std::holds_alternative<int>(car3) && std::get<int>(car3) == 3 &&
           std::holds_alternative<nil_t>(cdr3);
}());

static_assert([] {
    // (list 1 2 3) builds the same structure as nested cons.
    pair_heap<dummy_core, 8> heap;
    std::array<val, 3> args{val{1}, val{2}, val{3}};
    auto built = apply_prim<dummy_core>(list_op::list, args, &heap).value();

    std::array<val, 1> a1{built};
    auto car1 = apply_prim<dummy_core>(list_op::car, a1, &heap).value();
    auto cdr1 = apply_prim<dummy_core>(list_op::cdr, a1, &heap).value();
    std::array<val, 1> a2{cdr1};
    auto car2 = apply_prim<dummy_core>(list_op::car, a2, &heap).value();
    auto cdr2 = apply_prim<dummy_core>(list_op::cdr, a2, &heap).value();
    std::array<val, 1> a3{cdr2};
    auto car3 = apply_prim<dummy_core>(list_op::car, a3, &heap).value();
    auto cdr3 = apply_prim<dummy_core>(list_op::cdr, a3, &heap).value();

    return std::holds_alternative<int>(car1) && std::get<int>(car1) == 1 &&
           std::holds_alternative<int>(car2) && std::get<int>(car2) == 2 &&
           std::holds_alternative<int>(car3) && std::get<int>(car3) == 3 &&
           std::holds_alternative<nil_t>(cdr3);
}());

static_assert([] {
    // (list) = nil; no heap needed for the empty case.
    std::array<val, 0> args{};
    auto r = apply_prim<dummy_core>(
        list_op::list, args, static_cast<pair_heap<dummy_core, 4> *>(nullptr));
    return r.has_value() && std::holds_alternative<nil_t>(r.value());
}());

} // namespace

TEST_CASE("PairsTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("PairsTest - ConsProducesPairRef") {
    pair_heap<dummy_core, 4> heap;
    std::array<val, 2> args{val{1}, val{2}};
    auto r = apply_prim<dummy_core>(list_op::cons, args, &heap);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<pair_ref>(r.value()));
    auto loc = std::get<pair_ref>(r.value()).loc;
    REQUIRE(std::get<int>(heap.get(loc).car) == 1);
    REQUIRE(std::get<int>(heap.get(loc).cdr) == 2);
}

TEST_CASE("PairsTest - CarCdrOfNilIsNil") {
    pair_heap<dummy_core, 4> heap;
    std::array<val, 1> args{val{nil_t{}}};
    auto car_r = apply_prim<dummy_core>(list_op::car, args, &heap);
    auto cdr_r = apply_prim<dummy_core>(list_op::cdr, args, &heap);
    REQUIRE(car_r.has_value());
    REQUIRE(std::holds_alternative<nil_t>(car_r.value()));
    REQUIRE(cdr_r.has_value());
    REQUIRE(std::holds_alternative<nil_t>(cdr_r.value()));
}

TEST_CASE("PairsTest - CarCdrOfNonPairIsError") {
    pair_heap<dummy_core, 4> heap;
    std::array<val, 1> args{val{42}};
    auto car_r = apply_prim<dummy_core>(list_op::car, args, &heap);
    REQUIRE_FALSE(car_r.has_value());
}

TEST_CASE("PairsTest - NullPredicate") {
    pair_heap<dummy_core, 4> heap;
    std::array<val, 1> nil_args{val{nil_t{}}};
    std::array<val, 1> int_args{val{0}};
    auto r_nil = apply_prim<dummy_core>(list_op::null, nil_args, &heap);
    auto r_int = apply_prim<dummy_core>(list_op::null, int_args, &heap);
    REQUIRE(r_nil.has_value());
    REQUIRE(std::holds_alternative<symbol>(r_nil.value()));
    REQUIRE(r_int.has_value());
    REQUIRE(std::holds_alternative<nil_t>(r_int.value()));
}

TEST_CASE("PairsTest - AtomPredicate") {
    pair_heap<dummy_core, 4> heap;
    std::array<val, 1> nil_args{val{nil_t{}}};
    std::array<val, 1> int_args{val{5}};
    auto r_nil = apply_prim<dummy_core>(list_op::atom, nil_args, &heap);
    auto r_int = apply_prim<dummy_core>(list_op::atom, int_args, &heap);
    REQUIRE(r_nil.has_value());
    REQUIRE(std::holds_alternative<symbol>(r_nil.value()));
    REQUIRE(r_int.has_value());
    REQUIRE(std::holds_alternative<symbol>(r_int.value()));

    std::array<val, 2> cons_args{val{1}, val{nil_t{}}};
    auto consed = apply_prim<dummy_core>(list_op::cons, cons_args, &heap);
    std::array<val, 1> pair_args{consed.value()};
    auto r_pair = apply_prim<dummy_core>(list_op::atom, pair_args, &heap);
    REQUIRE(r_pair.has_value());
    REQUIRE(std::holds_alternative<nil_t>(r_pair.value()));
}

TEST_CASE("PairsTest - EqEqlOnSymbolsIntegersAndNil") {
    pair_heap<dummy_core, 4> heap;

    std::array<val, 2> same_sym{val{symbol{"A"}}, val{symbol{"A"}}};
    auto r1 = apply_prim<dummy_core>(list_op::eq, same_sym, &heap);
    REQUIRE(r1.has_value());
    REQUIRE(std::holds_alternative<symbol>(r1.value()));

    std::array<val, 2> diff_sym{val{symbol{"A"}}, val{symbol{"B"}}};
    auto r2 = apply_prim<dummy_core>(list_op::eql, diff_sym, &heap);
    REQUIRE(r2.has_value());
    REQUIRE(std::holds_alternative<nil_t>(r2.value()));

    std::array<val, 2> same_int{val{7}, val{7}};
    auto r3 = apply_prim<dummy_core>(list_op::eql, same_int, &heap);
    REQUIRE(r3.has_value());
    REQUIRE(std::holds_alternative<symbol>(r3.value()));

    std::array<val, 2> both_nil{val{nil_t{}}, val{nil_t{}}};
    auto r4 = apply_prim<dummy_core>(list_op::eq, both_nil, &heap);
    REQUIRE(r4.has_value());
    REQUIRE(std::holds_alternative<symbol>(r4.value()));
}

TEST_CASE("PairsTest - EqOnPairsIsIdentityNotStructure") {
    pair_heap<dummy_core, 4> heap;
    std::array<val, 2> args1{val{1}, val{2}};
    std::array<val, 2> args2{val{1}, val{2}};
    auto p = apply_prim<dummy_core>(list_op::cons, args1, &heap).value();
    auto q = apply_prim<dummy_core>(list_op::cons, args2, &heap).value();
    std::array<val, 2> eq_args{p, q};
    auto r = apply_prim<dummy_core>(list_op::eq, eq_args, &heap);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<nil_t>(r.value())); // distinct cells.

    std::array<val, 2> eq_self{p, p};
    auto r_self = apply_prim<dummy_core>(list_op::eq, eq_self, &heap);
    REQUIRE(r_self.has_value());
    REQUIRE(std::holds_alternative<symbol>(r_self.value())); // same handle.
}

TEST_CASE("PairsTest - ListBuildsProperList") {
    pair_heap<dummy_core, 8> heap;
    std::array<val, 3> args{val{1}, val{2}, val{3}};
    auto built = apply_prim<dummy_core>(list_op::list, args, &heap);
    REQUIRE(built.has_value());
    REQUIRE(std::holds_alternative<pair_ref>(built.value()));

    std::array<val, 1> a1{built.value()};
    auto cdr1 = apply_prim<dummy_core>(list_op::cdr, a1, &heap).value();
    std::array<val, 1> a2{cdr1};
    auto cdr2 = apply_prim<dummy_core>(list_op::cdr, a2, &heap).value();
    std::array<val, 1> a3{cdr2};
    auto cdr3 = apply_prim<dummy_core>(list_op::cdr, a3, &heap).value();
    REQUIRE(std::holds_alternative<nil_t>(cdr3)); // proper list ends in nil.
}

TEST_CASE("PairsTest - EmptyListIsNil") {
    std::array<val, 0> args{};
    auto r = apply_prim<dummy_core>(
        list_op::list, args, static_cast<pair_heap<dummy_core, 4> *>(nullptr));
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<nil_t>(r.value()));
}

TEST_CASE("PairsTest - ConsWithoutHeapIsError") {
    std::array<val, 2> args{val{1}, val{2}};
    auto r = apply_prim<dummy_core>(
        list_op::cons, args, static_cast<pair_heap<dummy_core, 4> *>(nullptr));
    REQUIRE_FALSE(r.has_value());
}

// -- append (step L18: backquote's ,@ unquote-splicing lowering) ------------

TEST_CASE("PairsTest - AppendJoinsTwoLists") {
    pair_heap<dummy_core, 8> heap;
    std::array<val, 3> l1_args{val{1}, val{2}, val{3}};
    auto l1 = apply_prim<dummy_core>(list_op::list, l1_args, &heap).value();
    std::array<val, 2> l2_args{val{4}, val{5}};
    auto l2 = apply_prim<dummy_core>(list_op::list, l2_args, &heap).value();

    std::array<val, 2> append_args{l1, l2};
    auto r = apply_prim<dummy_core>(list_op::append, append_args, &heap);
    REQUIRE(r.has_value());

    std::array<val, 1> a1{r.value()};
    auto car1 = apply_prim<dummy_core>(list_op::car, a1, &heap).value();
    auto cdr1 = apply_prim<dummy_core>(list_op::cdr, a1, &heap).value();
    std::array<val, 1> a2{cdr1};
    auto car2 = apply_prim<dummy_core>(list_op::car, a2, &heap).value();
    auto cdr2 = apply_prim<dummy_core>(list_op::cdr, a2, &heap).value();
    std::array<val, 1> a3{cdr2};
    auto car3 = apply_prim<dummy_core>(list_op::car, a3, &heap).value();
    auto cdr3 = apply_prim<dummy_core>(list_op::cdr, a3, &heap).value();
    std::array<val, 1> a4{cdr3};
    auto car4 = apply_prim<dummy_core>(list_op::car, a4, &heap).value();
    auto cdr4 = apply_prim<dummy_core>(list_op::cdr, a4, &heap).value();
    std::array<val, 1> a5{cdr4};
    auto car5 = apply_prim<dummy_core>(list_op::car, a5, &heap).value();
    auto cdr5 = apply_prim<dummy_core>(list_op::cdr, a5, &heap).value();

    REQUIRE(std::get<int>(car1) == 1);
    REQUIRE(std::get<int>(car2) == 2);
    REQUIRE(std::get<int>(car3) == 3);
    REQUIRE(std::get<int>(car4) == 4);
    REQUIRE(std::get<int>(car5) == 5);
    REQUIRE(std::holds_alternative<nil_t>(cdr5));
}

TEST_CASE("PairsTest - AppendWithNilFirstArgIsSecondArg") {
    pair_heap<dummy_core, 4> heap;
    std::array<val, 2> l_args{val{1}, val{2}};
    auto l = apply_prim<dummy_core>(list_op::list, l_args, &heap).value();
    std::array<val, 2> args{val{nil_t{}}, l};
    auto r = apply_prim<dummy_core>(list_op::append, args, &heap);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<pair_ref>(r.value()));
    REQUIRE(std::get<pair_ref>(r.value()).loc == std::get<pair_ref>(l).loc);
}

TEST_CASE("PairsTest - AppendWithNoArgsIsNil") {
    std::array<val, 0> args{};
    auto r = apply_prim<dummy_core>(
        list_op::append, args,
        static_cast<pair_heap<dummy_core, 4> *>(nullptr));
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<nil_t>(r.value()));
}

TEST_CASE("PairsTest - AppendLastArgSharedNotCopied") {
    // Appending onto a shared tail returns the SAME cell for the tail's
    // first pair (identity, not a structural copy) -- only every argument
    // but the last is copied.
    pair_heap<dummy_core, 4> heap;
    std::array<val, 2> tail_args{val{9}, val{nil_t{}}};
    auto tail = apply_prim<dummy_core>(list_op::cons, tail_args, &heap).value();
    std::array<val, 1> single{val{1}};
    auto l1 = apply_prim<dummy_core>(list_op::list, single, &heap).value();

    std::array<val, 2> args{l1, tail};
    auto r = apply_prim<dummy_core>(list_op::append, args, &heap);
    REQUIRE(r.has_value());
    auto cdr1 = apply_prim<dummy_core>(list_op::cdr,
                                       std::array<val, 1>{r.value()}, &heap)
                    .value();
    REQUIRE(std::holds_alternative<pair_ref>(cdr1));
    REQUIRE(std::get<pair_ref>(cdr1).loc == std::get<pair_ref>(tail).loc);
}

TEST_CASE("PairsTest - AppendOnNonListIsError") {
    pair_heap<dummy_core, 4> heap;
    std::array<val, 2> args{val{42}, val{nil_t{}}};
    auto r = apply_prim<dummy_core>(list_op::append, args, &heap);
    REQUIRE_FALSE(r.has_value());
}
