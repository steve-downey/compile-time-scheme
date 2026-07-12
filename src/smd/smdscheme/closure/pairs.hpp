// src/smd/smdscheme/closure/pairs.hpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_CLOSURE_PAIRS_HPP
#define SRC_SMD_SMDSCHEME_CLOSURE_PAIRS_HPP

#include <smd/smdscheme/closure/value.hpp>
#include <smd/smdscheme/elaborator/elaborated_core.hpp>
#include <smd/smdscheme/foundation/result.hpp>

#include <span>
#include <variant>

namespace smd::smdscheme::closure {

/// Applies a pair/list primitive to already-evaluated arguments.
///
/// This is the single home of the pair-operation semantics.  Every evaluator
/// (direct, CPS, the two sender backends, and the two Mendler interpreters)
/// evaluates the arguments of a @ref elaborator::core_prim / @c comp_prim node
/// in its own idiom, then delegates here.  Keeping the ~dozen operations in one
/// place is why adding pairs costs one @c std::visit arm per backend rather
/// than a dozen: the interpretation lives here, not at each visit site.
///
/// @p heap is the shared pair heap (from the environment); it may be null, in
/// which case any operation that needs to allocate or dereference a cell errors
/// (mirroring how @c set! errors without a @ref store).  Equality on pairs is
/// identity (@c eq?/@c eqv? compare @ref pair_ref by location); @c equal? walks
/// the heap structurally.
///
/// @tparam Core     The core AST type (either the arena core or the Comp tree).
/// @tparam MaxPairs Heap capacity (deduced from @p heap).
/// @param  op    Which primitive to apply.
/// @param  args  The evaluated argument values.
/// @param  heap  The shared pair heap, or null.
/// @return The resulting value or a @ref foundation::parse_error.
template <typename Core, int MaxPairs>
[[nodiscard]] constexpr auto apply_prim(elaborator::prim_op op,
                                        std::span<value<Core> const> args,
                                        pair_heap<Core, MaxPairs> *heap)
    -> foundation::result<value<Core>> {
    using Val = value<Core>;
    using enum elaborator::prim_op;

    auto needs_heap = [&]() -> bool { return heap != nullptr; };

    switch (op) {
    case null:
        if (!args.empty())
            return foundation::parse_error{{}, "(): expected no arguments"};
        return Val{null_t{}};

    case cons:
        if (args.size() != 2)
            return foundation::parse_error{{}, "cons: expected 2 arguments"};
        if (!needs_heap())
            return foundation::parse_error{
                {}, "cons: environment has no pair heap"};
        return Val{pair_ref{heap->alloc(pair_cell<Core>{args[0], args[1]})}};

    case car:
    case cdr:
        if (args.size() != 1)
            return foundation::parse_error{{}, "car/cdr: expected 1 argument"};
        if (!std::holds_alternative<pair_ref>(args[0]))
            return foundation::parse_error{{}, "car/cdr: not a pair"};
        if (!needs_heap())
            return foundation::parse_error{
                {}, "car/cdr: environment has no pair heap"};
        {
            auto const &cell = heap->get(std::get<pair_ref>(args[0]).loc);
            return op == car ? Val{cell.car} : Val{cell.cdr};
        }

    case set_car:
    case set_cdr:
        if (args.size() != 2)
            return foundation::parse_error{
                {}, "set-car!/set-cdr!: expected 2 arguments"};
        if (!std::holds_alternative<pair_ref>(args[0]))
            return foundation::parse_error{{}, "set-car!/set-cdr!: not a pair"};
        if (!needs_heap())
            return foundation::parse_error{
                {}, "set-car!/set-cdr!: environment has no pair heap"};
        {
            auto &cell = heap->at(std::get<pair_ref>(args[0]).loc);
            if (op == set_car)
                cell.car = args[1];
            else
                cell.cdr = args[1];
            return Val{unspecified{}};
        }

    case pairp:
        if (args.size() != 1)
            return foundation::parse_error{{}, "pair?: expected 1 argument"};
        return Val{std::holds_alternative<pair_ref>(args[0])};

    case nullp:
        if (args.size() != 1)
            return foundation::parse_error{{}, "null?: expected 1 argument"};
        return Val{std::holds_alternative<null_t>(args[0])};

    case eq:
    case eqv:
        if (args.size() != 2)
            return foundation::parse_error{{},
                                           "eq?/eqv?: expected 2 arguments"};
        // pair_ref::operator== is identity; every other alternative compares by
        // value — exactly eq?/eqv? semantics.
        return Val{args[0] == args[1]};

    case equal:
        if (args.size() != 2)
            return foundation::parse_error{{}, "equal?: expected 2 arguments"};
        if (!needs_heap()) // no heap => no pairs exist => value equality holds
            return Val{args[0] == args[1]};
        return Val{values_equal(args[0], args[1], *heap)};

    case list:
        if (!args.empty() && !needs_heap())
            return foundation::parse_error{
                {}, "list: environment has no pair heap"};
        {
            Val acc{null_t{}};
            for (int i = static_cast<int>(args.size()) - 1; i >= 0; --i)
                acc = Val{pair_ref{heap->alloc(pair_cell<Core>{args[i], acc})}};
            return acc;
        }
    }
    return foundation::parse_error{{}, "unknown primitive"};
}

} // namespace smd::smdscheme::closure

#endif
