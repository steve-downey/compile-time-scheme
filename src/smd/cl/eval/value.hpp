// src/smd/cl/eval/value.hpp                                      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_EVAL_VALUE_HPP
#define SRC_SMD_CL_EVAL_VALUE_HPP

#include <smd/cl/symbol/symbol_id.hpp>

#include <variant>

/// The evaluator: a small-step abstract machine over the elaborated core
/// tree, on the three-channel outcome of decision D13.
///
/// Everything a running program manipulates is either an immediate — a
/// fixnum, a character, a symbol id — or a capacity-free handle into the
/// @ref smd::cl::eval::heap. No runtime value type is parameterised on a
/// capacity, which is decision D14: capacities parameterise the heap,
/// because the heap is storage.
namespace smd::cl::eval {

// 5d5f74e8-b4a3-4d0e-9b1f-0f0e3e7f7a11
/// A handle to a cons cell in a @ref heap. The default value is the
/// no-cell handle, which @ref valid rejects.
struct cons_ref {
    int index = -1; ///< Cell index, or -1 for no cell.

    /// Returns true if this handle names a cell.
    [[nodiscard]] constexpr auto valid() const -> bool { return index >= 0; }

    // HIDDEN FRIEND
    friend constexpr auto operator==(cons_ref, cons_ref) -> bool = default;
};

/// A handle to a run of characters in a @ref heap's string pool.
struct string_ref {
    int offset = 0; ///< First character's index in the pool.
    int length = 0; ///< Number of characters.

    // HIDDEN FRIEND
    friend constexpr auto operator==(string_ref, string_ref) -> bool = default;
};

/// A handle to a closure in a @ref heap.
struct closure_ref {
    int index = -1; ///< Closure index, or -1 for no closure.

    /// Returns true if this handle names a closure.
    [[nodiscard]] constexpr auto valid() const -> bool { return index >= 0; }

    // HIDDEN FRIEND
    friend constexpr auto operator==(closure_ref, closure_ref)
        -> bool = default;
};

/// A fixnum, the one numeric-tower member that is executable today.
struct value_fixnum {
    int value = 0; ///< The integer.

    // HIDDEN FRIEND
    friend constexpr auto operator==(value_fixnum, value_fixnum)
        -> bool = default;
};

/// A character object.
struct value_character {
    char value = 0; ///< The character.

    // HIDDEN FRIEND
    friend constexpr auto operator==(value_character, value_character)
        -> bool = default;
};

/// A symbol object, by interned id (decision D12).
///
/// `NIL` and `T` are ordinary symbols here, as ANSI says they are, not
/// alternatives of their own: `nil` is the symbol `NIL`, which is why it is
/// simultaneously the false value, the empty list, and something
/// `symbolp` answers true for. Which id that is belongs to the symbol
/// table, so a value alone cannot tell you it is `NIL` — the machine
/// resolves the name once and compares ids after.
struct value_symbol {
    symbol::symbol_id id{}; ///< The symbol.

    // HIDDEN FRIEND
    friend constexpr auto operator==(value_symbol, value_symbol)
        -> bool = default;
};

/// A string object, by handle into the heap's character pool.
struct value_string {
    string_ref ref{}; ///< The characters.

    // HIDDEN FRIEND
    friend constexpr auto operator==(value_string, value_string)
        -> bool = default;
};

/// A cons cell, by handle into the heap.
struct value_cons {
    cons_ref ref{}; ///< The cell.

    // HIDDEN FRIEND
    friend constexpr auto operator==(value_cons, value_cons) -> bool = default;
};

/// One Common Lisp object.
///
/// There is deliberately no function alternative yet. Nothing in the R5
/// object language produces one: `defun` names a function rather than
/// returning it (ANSI says it returns the name), a call resolves its callee
/// in the function namespace, and `#'f`, `lambda` as an expression and
/// `funcall` are all still diagnosed as not yet executable. Adding the
/// alternative before there is a form that yields one would be a channel
/// with no producer.
using value = std::variant<value_fixnum, value_character, value_symbol,
                           value_string, value_cons>;
// 5d5f74e8-b4a3-4d0e-9b1f-0f0e3e7f7a11 end

/// Returns the value of the symbol @p id.
[[nodiscard]] constexpr auto symbol_value(symbol::symbol_id id) -> value {
    return value{value_symbol{id}};
}

/// Returns true if @p v is the symbol @p id.
///
/// The machine's test for `nil` (falsity, the empty list) and for `t`, both
/// of which are this question asked about a resolved id.
[[nodiscard]] constexpr auto is_symbol(value const &v, symbol::symbol_id id)
    -> bool {
    auto const *named = std::get_if<value_symbol>(&v);
    return named != nullptr && named->id == id;
}

/// The two symbols the machine must know by identity rather than by name,
/// resolved once against the symbol table it was given.
struct standard_symbols {
    symbol::symbol_id nil{}; ///< `NIL`: false, and the empty list.
    symbol::symbol_id t{};   ///< `T`: the canonical true value.

    // HIDDEN FRIEND
    friend constexpr auto operator==(standard_symbols, standard_symbols)
        -> bool = default;
};

/// Returns `nil`, the false value and the empty list.
[[nodiscard]] constexpr auto nil_value(standard_symbols known) -> value {
    return symbol_value(known.nil);
}

/// Returns the canonical Lisp truth value for @p condition: `t` or `nil`.
[[nodiscard]] constexpr auto boolean_value(bool condition,
                                           standard_symbols known) -> value {
    return symbol_value(condition ? known.t : known.nil);
}

/// Returns true if @p v is false — that is, if it is the symbol `nil`.
/// Every other object is true, which is the whole of Common Lisp's rule.
[[nodiscard]] constexpr auto is_false(value const &v, standard_symbols known)
    -> bool {
    return is_symbol(v, known.nil);
}

} // namespace smd::cl::eval

#endif
