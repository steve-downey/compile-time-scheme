// src/smd/smdlisp/closure/value.hpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_CLOSURE_VALUE_HPP
#define SRC_SMD_SMDLISP_CLOSURE_VALUE_HPP

#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/foundation/static_vector.hpp>

#include <span>
#include <string_view>
#include <utility>
#include <variant>

namespace smd::smdlisp::closure {

/// Built-in operations supported at the value level.
///
/// A placeholder set for now; the list/predicate builtins (`cons`, `car`,
/// `cdr`, `null`, `eq`, `eql`, `atom`, ...) live in their own @ref list_op
/// enum in `pairs.hpp` (step L8) rather than growing this one; the
/// environment work in step L9 installs both sets as function-namespace
/// builtins.
enum class builtin_op { add, multiply };

/// A built-in operator value (holds the operation kind).
struct builtin {
    builtin_op op;
    friend constexpr auto operator==(builtin, builtin) -> bool = default;
};

/// The distinguished `nil` value.
///
/// Per decision D3 (docs/cl-pivot-plan.md), `nil` is a single value kind that
/// simultaneously serves as the sole false value, the empty list, and the
/// symbol `NIL`.  There is no separate boolean kind: truthiness is decided
/// exclusively by @ref is_true, never by a per-site encoding.
struct nil_t {
    friend constexpr auto operator==(nil_t, nil_t) -> bool = default;
};

/// A cons cell handle: a non-owning, stable-location reference into a
/// @ref pair_heap.
///
/// Adapted by copy from `smd::smdscheme::closure::pair_ref` (PR #26, step
/// L8).  A pair is the first value that is recursive, shareable, and
/// mutable, so — as with the Scheme original — it cannot be a deep-copy
/// value the way a scalar is: two names must be able to refer to the *same*
/// cell.  Unlike Scheme, there is no separate `null_t` empty-list kind: per
/// decision D3, `nil` (@ref nil_t) already serves as the empty list, so a
/// proper list's final `cdr` is a @ref nil_t value rather than a distinct
/// end-marker type.  Equality is identity: two @ref pair_ref compare equal
/// iff they name the same cell (CL's `eq`).
struct pair_ref {
    int loc = -1; ///< Stable heap location; -1 means "no pair".
    friend constexpr auto operator==(pair_ref lhs, pair_ref rhs) -> bool {
        return lhs.loc == rhs.loc;
    }
};

/// Forward declaration of the Lisp-2 environment (defined in step L9's
/// `env.hpp`) needed only to name the pointer type captured by @ref closure.
template <typename Core, int MaxBindings>
class env;

/// A first-class closure: a lambda node paired with a captured environment.
///
/// `node` points into the core arena; it is non-owning.  `captured` is a
/// non-owning pointer to the lexical environment at closure-creation time.
/// It is deliberately a raw pointer rather than an owning box: `env` is not
/// defined until step L9, and a raw pointer to an incomplete type needs no
/// destructor machinery, so `closure` (and therefore @ref value) can be a
/// complete, usable type before `env` exists.  Steps that actually build
/// closures (L9 onward) may need to revisit ownership once capture is wired
/// up.
///
/// @tparam Core The core AST type.
template <typename Core>
struct closure {
    Core const *node = nullptr; ///< Non-owning pointer to the lambda node.
    env<Core, 16> const *captured =
        nullptr; ///< Non-owning; environment ownership lands with L9.

    friend constexpr auto operator==(closure<Core> const &lhs,
                                     closure<Core> const &rhs) -> bool {
        // Simple structural equality for test purposes.
        return lhs.node == rhs.node;
    }
};

/// An interned symbol at runtime (not a compile-time identifier).
///
/// Per decision D2, unescaped symbol names are folded to uppercase at read
/// time; this type just holds whatever spelling it is given.
struct symbol {
    std::string_view name;
    friend constexpr auto operator==(symbol const &lhs, symbol const &rhs)
        -> bool {
        return lhs.name == rhs.name;
    }
};

/// A self-evaluating keyword (`:foo`), folded to uppercase per D2.
///
/// A keyword is a distinct value kind from @ref symbol so that keyword-ness
/// is a static, checkable property rather than a naming convention.
struct keyword {
    std::string_view name;
    friend constexpr auto operator==(keyword const &lhs, keyword const &rhs)
        -> bool {
        return lhs.name == rhs.name;
    }
};

/// A foreign (C++) function callable from the compiled Lisp program.
///
/// `fn` is a function pointer with signature `result<value>(span<value
/// const>)`.  This lets test harnesses and extension points install native
/// callbacks without defining new core forms.
///
/// @tparam Core The core AST type.
template <typename Core>
struct foreign_function {
    using val_t = std::variant<nil_t, int, symbol, keyword, builtin,
                               closure<Core>, foreign_function, pair_ref>;
    using sig_t =
        smd::smdscheme::foundation::result<val_t> (*)(std::span<val_t const>);

    sig_t fn = nullptr; ///< Pointer to the native implementation.

    friend constexpr auto operator==(foreign_function const &lhs,
                                     foreign_function const &rhs) -> bool {
        return lhs.fn == rhs.fn;
    }
};

/// The runtime value type: `nil`, integer, symbol, keyword, builtin,
/// closure, foreign function, or cons cell.
///
/// Cons cells (@ref pair_ref) arrive in step L8 (decision D6, proper lists
/// first); there is no separate empty-list kind because `nil` already fills
/// that role (decision D3).
///
/// @tparam Core The core AST type (used to type @ref closure and
///              @ref foreign_function).
template <typename Core>
using value = std::variant<nil_t, int, symbol, keyword, builtin, closure<Core>,
                           foreign_function<Core>, pair_ref>;

/// Fixed capacity of the @ref pair_heap shared by an environment.
inline constexpr int default_max_pairs = 256;

/// One cons cell: a `car` and a `cdr`, each an arbitrary @ref value.
///
/// Adapted by copy from `smd::smdscheme::closure::pair_cell` (PR #26).
///
/// @tparam Core The core AST type.
template <typename Core>
struct pair_cell {
    value<Core> car{};
    value<Core> cdr{};
};

/// A flat, constexpr, allocation-free heap of cons cells, addressed by
/// stable integer locations (@ref pair_ref).
///
/// Adapted by copy from `smd::smdscheme::closure::pair_heap` (PR #26):
/// @ref smd::smdscheme::foundation::static_vector is array-backed, so
/// locations never move once allocated, which is what lets @ref pair_ref
/// hold a bare stable index instead of a pointer.  The heap is shared (by
/// non-owning pointer) across every environment that needs it; it must
/// outlive every @ref pair_ref it hands out.
///
/// @tparam Core     The core AST type.
/// @tparam MaxPairs Maximum number of cons cells.
template <typename Core, int MaxPairs>
class pair_heap {
  public:
    /// Appends @p c and returns its stable location.
    constexpr auto alloc(pair_cell<Core> c) -> int {
        int loc = cells_.size();
        cells_.push_back(std::move(c));
        return loc;
    }

    /// Returns the cell at @p loc (read-only).
    [[nodiscard]] constexpr auto get(int loc) const -> pair_cell<Core> const & {
        return cells_[loc];
    }

    /// Returns the cell at @p loc for mutation.
    [[nodiscard]] constexpr auto at(int loc) -> pair_cell<Core> & {
        return cells_[loc];
    }

  private:
    smd::smdscheme::foundation::static_vector<pair_cell<Core>, MaxPairs>
        cells_{};
};

/// The sole truthiness authority for the whole project (decision D3).
///
/// `nil` is the only false value; every other value, including integer `0`
/// and every keyword, is true.  Every later `if` site (elaborator,
/// evaluators, CPS/sender backends) must route through this function rather
/// than re-encoding truthiness locally.
///
/// @tparam Core The core AST type.
template <typename Core>
[[nodiscard]] constexpr auto is_true(value<Core> const &v) -> bool {
    return !std::holds_alternative<nil_t>(v);
}

} // namespace smd::smdlisp::closure

#endif
