// src/smd/cl/eval/builtin.hpp                                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_EVAL_BUILTIN_HPP
#define SRC_SMD_CL_EVAL_BUILTIN_HPP

#include <smd/cl/eval/heap.hpp>
#include <smd/cl/eval/outcome.hpp>
#include <smd/cl/eval/value.hpp>
#include <smd/cl/foundation/fold_left_short.hpp>
#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/symbol/symbol_id.hpp>
#include <smd/cl/symbol/symbol_table.hpp>

#include <algorithm>
#include <array>
#include <functional>
#include <ranges>
#include <string_view>
#include <utility>
#include <variant>

namespace smd::cl::eval {

/// A function the evaluator implements itself.
///
/// A builtin lives in a symbol's function slot exactly as a closure does
/// (decision D12), so `car` is not a special case in the call path — it is
/// a symbol whose function slot happens to hold one of these. That is also
/// why redefining one is not a language question the evaluator has to
/// answer: `(defun car ...)` overwrites the slot like any other.
enum class builtin_op : unsigned char {
    car,           ///< `(car list)`
    cdr,           ///< `(cdr list)`
    cons,          ///< `(cons car cdr)`
    list,          ///< `(list object...)`
    null,          ///< `(null object)`
    not_,          ///< `(not object)`, the same test as `null`
    eq,            ///< `(eq a b)`
    eql,           ///< `(eql a b)`
    atom,          ///< `(atom object)`
    consp,         ///< `(consp object)`
    symbolp,       ///< `(symbolp object)`
    length,        ///< `(length list)`
    add,           ///< `(+ number...)`
    subtract,      ///< `(- number number...)`
    multiply,      ///< `(* number...)`
    numeric_equal, ///< `(= number...)`
    less,          ///< `(< number...)`
    greater        ///< `(> number...)`
};

/// One builtin's ANSI name, spelled as the reader's `:upcase` read case
/// leaves it, and the operation it names.
struct builtin_entry {
    std::string_view name{}; ///< The symbol name to install it under.
    builtin_op op{};         ///< What it does.

    // HIDDEN FRIEND
    friend constexpr auto operator==(builtin_entry, builtin_entry)
        -> bool = default;
};

// 9c1c6a0d-3a34-4b0f-9b6a-0a4a9f2d5e10
/// Every builtin, as name/operation pairs; the interpreter installs these
/// into function slots before it runs anything.
inline constexpr std::array<builtin_entry, 18> builtins{{
    {"CAR", builtin_op::car},
    {"CDR", builtin_op::cdr},
    {"CONS", builtin_op::cons},
    {"LIST", builtin_op::list},
    {"NULL", builtin_op::null},
    {"NOT", builtin_op::not_},
    {"EQ", builtin_op::eq},
    {"EQL", builtin_op::eql},
    {"ATOM", builtin_op::atom},
    {"CONSP", builtin_op::consp},
    {"SYMBOLP", builtin_op::symbolp},
    {"LENGTH", builtin_op::length},
    {"+", builtin_op::add},
    {"-", builtin_op::subtract},
    {"*", builtin_op::multiply},
    {"=", builtin_op::numeric_equal},
    {"<", builtin_op::less},
    {">", builtin_op::greater},
}};
// 9c1c6a0d-3a34-4b0f-9b6a-0a4a9f2d5e10 end

/// What a symbol's function slot holds (decision D12): a function the
/// evaluator implements, or one the program defined.
///
/// The slot is what makes recursion work and what makes a builtin
/// redefinable, and it is one slot either way — the call path asks the
/// symbol, not a table of special names.
using function_binding = std::variant<builtin_op, closure_ref>;

/// The symbol table a running program resolves names against: a @ref value
/// in the value slot, a @ref function_binding in the function slot.
///
/// The macro slot holds @c std::monostate, which is the honest type for
/// "there is no macroexpander yet" — a placeholder that can hold nothing is
/// better than a placeholder that can hold the wrong thing.
///
/// @tparam MaxSymbols   Maximum entries.
/// @tparam MaxNameChars Maximum total characters across all names.
template <int MaxSymbols, int MaxNameChars>
using symbol_table =
    symbol::symbol_table<value, function_binding, std::monostate, MaxSymbols,
                         MaxNameChars>;

// feb024b9-0150-4cda-a395-00cdf2d2474c
/// Interns `NIL`, `T` and every builtin's name into @p symbols, and puts
/// each builtin in the function slot of its own symbol.
///
/// Run before anything is read, for two reasons that are easy to conflate.
/// The elaborator asks the table what `NIL` and `T` are interned as, so they
/// must be there before a form mentions them; and a builtin is not a special
/// case in the call path at all — `(car x)` is a @c core_call whose callee
/// symbol happens to have one of these in its slot, so `(defun car ...)`
/// overwrites it exactly as it would overwrite any other definition.
///
/// @return `NIL` and `T`, resolved, for the machine to compare ids against.
template <class SymbolTable>
[[nodiscard]] constexpr auto install_builtins(SymbolTable &symbols)
    -> foundation::result<standard_symbols> {
    return foundation::and_then(
        symbol::intern_checked(symbols, "NIL"),
        [&symbols](symbol::symbol_id nil) {
            return foundation::and_then(
                symbol::intern_checked(symbols, "T"),
                [&symbols, nil](symbol::symbol_id t) {
                    return foundation::fold_left_short(
                        builtins, standard_symbols{nil, t},
                        [&symbols](standard_symbols known, builtin_entry entry)
                            -> foundation::result<standard_symbols> {
                            return foundation::and_then(
                                symbol::intern_checked(symbols, entry.name),
                                [&symbols, known,
                                 op = entry.op](symbol::symbol_id id)
                                    -> foundation::result<standard_symbols> {
                                    symbols.set_function(id,
                                                         function_binding{op});
                                    return known;
                                });
                        });
                });
        });
}
// feb024b9-0150-4cda-a395-00cdf2d2474c end

namespace detail {

/// Returns true if @p args holds between @p least and @p most arguments;
/// a @p most of -1 means unbounded.
template <class Args>
[[nodiscard]] constexpr auto arity_ok(Args const &args, int least, int most)
    -> bool {
    return args.size() >= least && (most < 0 || args.size() <= most);
}

/// Returns true if @p v is a cons cell.
[[nodiscard]] constexpr auto is_cons(value const &v) -> bool {
    return std::holds_alternative<value_cons>(v);
}

/// Returns true if @p v is a fixnum.
[[nodiscard]] constexpr auto is_fixnum(value const &v) -> bool {
    return std::holds_alternative<value_fixnum>(v);
}

/// Returns the half of a cons cell @p half selects, `nil` for `nil`, and a
/// diagnosed type error for anything else — which is ANSI's rule for `car`
/// and `cdr` both.
template <class Heap, class Half>
[[nodiscard]] constexpr auto cons_half(Heap const &store, value const &v,
                                       standard_symbols known, Half half)
    -> outcome {
    if (is_false(v, known)) {
        return outcome{v};
    }
    cons_ref const pair = as_cons(v);
    if (!pair.valid()) {
        return foundation::parse_error{{}, "argument must be a list"};
    }
    return outcome{half(store.cell(pair))};
}

/// Emits @p args as a fresh proper list, right to left from `nil`.
template <class Heap, class Args>
[[nodiscard]] constexpr auto make_list(Heap &store, Args const &args,
                                       standard_symbols known) -> outcome {
    return to_outcome(foundation::fold_left_short(
        args | std::views::reverse, nil_value(known),
        [&store](value tail, value const &element) {
            return store.cons(element, std::move(tail));
        }));
}

/// Applies an arithmetic or comparison operation to @p args.
///
/// Every one of these needs its arguments to be numbers, so the check is
/// made once here and the operations below see a range of `int`.
template <class Args>
[[nodiscard]] constexpr auto apply_numeric(builtin_op op, Args const &args,
                                           standard_symbols known) -> outcome {
    if (!std::ranges::all_of(args, is_fixnum)) {
        return foundation::parse_error{{}, "argument must be a number"};
    }
    auto const numbers = args | std::views::transform([](value const &v) {
                             return std::get<value_fixnum>(v).value;
                         });
    // Ordering is "no adjacent pair out of order", which is n-ary in ANSI
    // and reads as one algorithm rather than a chain of special cases.
    auto const ordered = [&numbers](auto violation) {
        return std::ranges::adjacent_find(numbers, violation) ==
               std::ranges::end(numbers);
    };
    switch (op) {
    case builtin_op::add:
        return outcome{value{value_fixnum{
            std::ranges::fold_left(numbers, 0, std::plus<int>{})}}};
    case builtin_op::multiply:
        return outcome{value{value_fixnum{
            std::ranges::fold_left(numbers, 1, std::multiplies<int>{})}}};
    case builtin_op::subtract:
        if (!arity_ok(args, 1, -1)) {
            return foundation::parse_error{{}, "- needs at least one argument"};
        }
        if (args.size() == 1) {
            return outcome{value{value_fixnum{-numbers.front()}}};
        }
        return outcome{value{value_fixnum{
            std::ranges::fold_left(numbers | std::views::drop(1),
                                   numbers.front(), std::minus<int>{})}}};
    case builtin_op::numeric_equal:
    case builtin_op::less:
    case builtin_op::greater:
        if (!arity_ok(args, 1, -1)) {
            return foundation::parse_error{
                {}, "a numeric comparison needs at least one argument"};
        }
        if (op == builtin_op::numeric_equal) {
            return outcome{
                boolean_value(ordered(std::ranges::not_equal_to{}), known)};
        }
        if (op == builtin_op::less) {
            return outcome{
                boolean_value(ordered(std::ranges::greater_equal{}), known)};
        }
        return outcome{
            boolean_value(ordered(std::ranges::less_equal{}), known)};
    default:
        break;
    }
    return foundation::parse_error{{}, "not a numeric operation"};
}

} // namespace detail

// 1f9a4b57-2c1d-4a75-9a17-6b8b4b0f2a63
/// Applies the builtin @p op to @p args.
///
/// Returns an @ref outcome rather than a value because a builtin can fail —
/// but never unwinds: nothing here is a control-transfer operator, so the
/// third channel stays empty. A channel with no producer is not a channel
/// this function has to think about.
///
/// @tparam Heap  The @ref heap instantiation the arguments live in.
/// @tparam Args  A random-access range of @ref value.
/// @param  op    Which builtin.
/// @param  args  Its arguments, left to right.
/// @param  store The heap, for the builtins that allocate.
/// @param  known `NIL` and `T`, resolved.
template <class Heap, class Args>
[[nodiscard]] constexpr auto apply_builtin(builtin_op op, Args const &args,
                                           Heap &store, standard_symbols known)
    -> outcome {
    auto const wrong_arity = foundation::parse_error{
        {}, "wrong number of arguments to a builtin function"};
    switch (op) {
    case builtin_op::car:
        if (!detail::arity_ok(args, 1, 1)) {
            return wrong_arity;
        }
        return detail::cons_half(store, args[0], known,
                                 [](cons_cell const &c) { return c.car; });
    case builtin_op::cdr:
        if (!detail::arity_ok(args, 1, 1)) {
            return wrong_arity;
        }
        return detail::cons_half(store, args[0], known,
                                 [](cons_cell const &c) { return c.cdr; });
    case builtin_op::cons:
        if (!detail::arity_ok(args, 2, 2)) {
            return wrong_arity;
        }
        return to_outcome(store.cons(args[0], args[1]));
    case builtin_op::list:
        return detail::make_list(store, args, known);
    case builtin_op::null:
    case builtin_op::not_:
        if (!detail::arity_ok(args, 1, 1)) {
            return wrong_arity;
        }
        return outcome{boolean_value(is_false(args[0], known), known)};
    case builtin_op::eq:
    case builtin_op::eql:
        // The two coincide today, and not for the pivot's reason. DIV-0002
        // had them coincide because both were string comparison on a symbol
        // name. Here identity is real — an id, a cell index, a pool offset —
        // and every number that exists is an immediate, so there is nothing
        // yet for which `eql` is the weaker test. Boxed numbers are what
        // separates them, and boxed numbers are D19's later lanes.
        if (!detail::arity_ok(args, 2, 2)) {
            return wrong_arity;
        }
        return outcome{boolean_value(args[0] == args[1], known)};
    case builtin_op::atom:
        if (!detail::arity_ok(args, 1, 1)) {
            return wrong_arity;
        }
        return outcome{boolean_value(!detail::is_cons(args[0]), known)};
    case builtin_op::consp:
        if (!detail::arity_ok(args, 1, 1)) {
            return wrong_arity;
        }
        return outcome{boolean_value(detail::is_cons(args[0]), known)};
    case builtin_op::symbolp:
        if (!detail::arity_ok(args, 1, 1)) {
            return wrong_arity;
        }
        return outcome{boolean_value(
            std::holds_alternative<value_symbol>(args[0]), known)};
    case builtin_op::length:
        if (!detail::arity_ok(args, 1, 1)) {
            return wrong_arity;
        }
        if (!is_false(args[0], known) && !detail::is_cons(args[0])) {
            return foundation::parse_error{{}, "argument must be a list"};
        }
        return outcome{value{value_fixnum{
            static_cast<int>(std::ranges::distance(store.list(args[0])))}}};
    case builtin_op::add:
    case builtin_op::subtract:
    case builtin_op::multiply:
    case builtin_op::numeric_equal:
    case builtin_op::less:
    case builtin_op::greater:
        return detail::apply_numeric(op, args, known);
    }
    return foundation::parse_error{{}, "unknown builtin function"};
}
// 1f9a4b57-2c1d-4a75-9a17-6b8b4b0f2a63 end

} // namespace smd::cl::eval

#endif
