// src/smd/smdlisp/closure/pairs.hpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_CLOSURE_PAIRS_HPP
#define SRC_SMD_SMDLISP_CLOSURE_PAIRS_HPP

#include <smd/smdlisp/closure/value.hpp>
#include <smd/smdscheme/foundation/result.hpp>

#include <span>
#include <variant>

namespace smd::smdlisp::closure {

/// The list/predicate primitive operations built on cons cells.
///
/// Adapted by copy from `smd::smdscheme::elaborator::prim_op` /
/// `smd::smdscheme::closure::apply_prim` (PR #26), narrowed to the CL
/// builtin set named in step L8 of docs/cl-pivot-plan.md: `cons`, `car`,
/// `cdr`, `list`, `null`, `eq`, `eql`, `atom` (CL names, no `?`/`!` suffixes;
/// `set-car!`/`set-cdr!`/`pair?`/`equal?` are not part of this step).
/// `append` joins the set in step L18: the backquote expander's `,@x`
/// (unquote-splicing) lowering needs it to join a spliced sub-list onto the
/// rest of the enclosing template without an extra wrapping cons cell.
enum class list_op { cons, car, cdr, list, null, eq, eql, atom, append };

namespace detail {

/// Copies the proper list @p a onto the front of @p b, sharing @p b's
/// structure rather than copying it (the ANSI CL `append` contract: every
/// argument but the last is copied; the last is used as-is).
///
/// A direct recursive descent over @p a's cons structure -- the natural
/// shape of `append`'s own textbook definition (`(append (cons x xs) b) =
/// (cons x (append xs b))`, `(append nil b) = b`) -- rather than an
/// intermediate buffer: @p a's length is bounded by @p heap's capacity, and
/// this project already accepts unbounded-by-a-template-parameter recursion
/// elsewhere in this exact style (e.g. this file's own `car`/`cdr` chains
/// walked by callers).
///
/// @tparam Core     The core AST type.
/// @tparam MaxPairs Heap capacity (deduced from @p heap).
/// @param  a    The list to copy; must be a proper list ending in @ref
///              nil_t (or @ref nil_t itself).
/// @param  b    The list (or any value) appended after @p a's elements.
/// @param  heap The shared pair heap; must be non-null if @p a is not nil.
template <typename Core, int MaxPairs>
[[nodiscard]] constexpr auto append_two(value<Core> const &a,
                                        value<Core> const &b,
                                        pair_heap<Core, MaxPairs> *heap)
    -> smd::smdscheme::foundation::result<value<Core>> {
    using Val = value<Core>;
    using smd::smdscheme::foundation::parse_error;

    if (std::holds_alternative<nil_t>(a))
        return b;
    if (!std::holds_alternative<pair_ref>(a))
        return parse_error{{}, "append: not a list"};
    if (heap == nullptr)
        return parse_error{{}, "append: environment has no pair heap"};
    auto const &cell = heap->get(std::get<pair_ref>(a).loc);
    auto rest_r = append_two<Core, MaxPairs>(cell.cdr, b, heap);
    if (!rest_r.has_value())
        return rest_r;
    return Val{
        pair_ref{heap->alloc(pair_cell<Core>{cell.car, rest_r.value()})}};
}

} // namespace detail

/// Applies a @ref list_op primitive to already-evaluated arguments.
///
/// This is the single home of the pair-operation semantics, mirroring the
/// Scheme `apply_prim`'s role: every evaluator (direct, CPS, sender) that
/// gains a list-primitive core node delegates the actual semantics here
/// rather than re-encoding them at each visit site.
///
/// Two CL deltas from the Scheme original (decision D3, step L8):
///  - There is no separate empty-list kind; `nil` (@ref nil_t) is both the
///    sole false value and the empty list, so `(null nil)` and building a
///    proper list both bottom out at a @ref nil_t value rather than a
///    distinct `null_t`.
///  - `car`/`cdr` of `nil` return `nil` instead of erroring; applying either
///    to any other non-pair value is still a diagnosed error.
///
/// `eq` and `eql` are implemented identically (structural equality on every
/// alternative except @ref pair_ref, which compares by heap-location
/// identity), the same simplification the Scheme original makes for
/// `eq?`/`eqv?`; `smdlisp` has no floats or characters yet (decision D10),
/// so the ANSI distinction between the two (numeric-type-sensitive `eql` vs.
/// implementation-defined `eq`) has no observable difference here.
///
/// Predicates (`null`, `eq`, `eql`, `atom`) return the canonical CL true
/// value, the symbol `T`, rather than a boolean, per decision D3: there is
/// no boolean value kind, only `nil` (false) and everything else (true).
///
/// @p heap is the shared pair heap (from the environment); it may be null,
/// in which case any operation that needs to allocate or dereference a cell
/// errors.
///
/// @tparam Core     The core AST type.
/// @tparam MaxPairs Heap capacity (deduced from @p heap).
/// @param  op    Which primitive to apply.
/// @param  args  The evaluated argument values.
/// @param  heap  The shared pair heap, or null.
/// @return The resulting value or a @c foundation::parse_error.
template <typename Core, int MaxPairs>
[[nodiscard]] constexpr auto apply_prim(list_op op,
                                        std::span<value<Core> const> args,
                                        pair_heap<Core, MaxPairs> *heap)
    -> smd::smdscheme::foundation::result<value<Core>> {
    using Val = value<Core>;
    using smd::smdscheme::foundation::parse_error;
    using enum list_op;

    Val const true_value{symbol{"T"}};
    Val const false_value{nil_t{}};

    switch (op) {
    case cons:
        if (args.size() != 2)
            return parse_error{{}, "cons: expected 2 arguments"};
        if (heap == nullptr)
            return parse_error{{}, "cons: environment has no pair heap"};
        return Val{pair_ref{heap->alloc(pair_cell<Core>{args[0], args[1]})}};

    case car:
    case cdr:
        if (args.size() != 1)
            return parse_error{{}, "car/cdr: expected 1 argument"};
        if (std::holds_alternative<nil_t>(args[0]))
            return false_value; // (car nil) = (cdr nil) = nil, per D3/L8.
        if (!std::holds_alternative<pair_ref>(args[0]))
            return parse_error{{}, "car/cdr: not a list"};
        if (heap == nullptr)
            return parse_error{{}, "car/cdr: environment has no pair heap"};
        {
            auto const &cell = heap->get(std::get<pair_ref>(args[0]).loc);
            return op == car ? Val{cell.car} : Val{cell.cdr};
        }

    case list:
        if (args.empty())
            return false_value; // (list) = nil.
        if (heap == nullptr)
            return parse_error{{}, "list: environment has no pair heap"};
        {
            Val acc{nil_t{}};
            for (int i = static_cast<int>(args.size()) - 1; i >= 0; --i)
                acc = Val{pair_ref{heap->alloc(pair_cell<Core>{args[i], acc})}};
            return acc;
        }

    case null:
        if (args.size() != 1)
            return parse_error{{}, "null: expected 1 argument"};
        return std::holds_alternative<nil_t>(args[0]) ? true_value
                                                      : false_value;

    case eq:
    case eql:
        if (args.size() != 2)
            return parse_error{{}, "eq/eql: expected 2 arguments"};
        // pair_ref::operator== is heap-location identity; every other
        // alternative compares by value.
        return args[0] == args[1] ? true_value : false_value;

    case atom:
        if (args.size() != 1)
            return parse_error{{}, "atom: expected 1 argument"};
        // Everything that is not a cons cell is an atom, including nil.
        return std::holds_alternative<pair_ref>(args[0]) ? false_value
                                                         : true_value;

    case append:
        // (append) = nil; every argument but the last is copied (@ref
        // detail::append_two), the last is used as-is (ANSI CL contract;
        // step L18 needs this for `,@x` unquote-splicing).
        if (args.empty())
            return false_value;
        {
            Val acc = args[args.size() - 1];
            for (int i = static_cast<int>(args.size()) - 2; i >= 0; --i) {
                auto r = detail::append_two<Core, MaxPairs>(args[i], acc, heap);
                if (!r.has_value())
                    return r;
                acc = r.value();
            }
            return acc;
        }
    }
    return parse_error{{}, "unknown list primitive"};
}

} // namespace smd::smdlisp::closure

#endif
