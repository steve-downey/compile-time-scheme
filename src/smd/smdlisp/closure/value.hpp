// src/smd/smdlisp/closure/value.hpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_CLOSURE_VALUE_HPP
#define SRC_SMD_SMDLISP_CLOSURE_VALUE_HPP

#include <smd/smdscheme/foundation/result.hpp>

#include <span>
#include <string_view>
#include <variant>

namespace smd::smdlisp::closure {

/// Built-in operations supported at the value level.
///
/// A placeholder set for now; list/predicate builtins (`cons`, `car`, `cdr`,
/// `null`, `eq`, `eql`, `atom`, ...) arrive with the environment work in
/// steps L8/L9.
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
                               closure<Core>, foreign_function>;
    using sig_t =
        smd::smdscheme::foundation::result<val_t> (*)(std::span<val_t const>);

    sig_t fn = nullptr; ///< Pointer to the native implementation.

    friend constexpr auto operator==(foreign_function const &lhs,
                                     foreign_function const &rhs) -> bool {
        return lhs.fn == rhs.fn;
    }
};

/// The runtime value type: `nil`, integer, symbol, keyword, builtin,
/// closure, or foreign function.
///
/// Cons cells are not a value kind yet; they arrive in step L8 (decision
/// D6, proper lists first).
///
/// @tparam Core The core AST type (used to type @ref closure and
///              @ref foreign_function).
template <typename Core>
using value = std::variant<nil_t, int, symbol, keyword, builtin, closure<Core>,
                           foreign_function<Core>>;

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
