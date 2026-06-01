// src/smd/smdscheme/closure/value.hpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_CLOSURE_VALUE_HPP
#define SRC_SMD_SMDSCHEME_CLOSURE_VALUE_HPP

#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/foundation/static_vector.hpp>
#include <smd/smdscheme/reader/datum_type.hpp>

#include <span>
#include <string_view>
#include <variant>

namespace smd::smdscheme::closure {

/// Built-in arithmetic operations supported at the value level.
enum class builtin_op { add, multiply };

/// A built-in operator value (holds the operation kind).
struct builtin {
    builtin_op op;
    friend constexpr auto operator==(builtin, builtin) -> bool = default;
};

// Forward declaration of env to resolve circular dependency
template <typename Core, int MaxBindings>
class env;

/// A copyable, constexpr-compatible owning pointer.
///
/// @c std::unique_ptr cannot be used in constexpr because it has a deleted
/// copy constructor; @c constexpr_box provides deep-copy semantics while
/// still destroying via @c delete.  It is used to break the circular
/// dependency between @ref closure and @ref env.
///
/// @tparam T Pointee type.
template <class T>
struct constexpr_box {
    T *ptr = nullptr; ///< Raw owning pointer; null means "no value".

    constexpr constexpr_box() = default;

    /// Takes ownership of @p p.
    constexpr explicit constexpr_box(T *p) : ptr(p) {}

    /// Deep-copies @p other's pointee.
    constexpr constexpr_box(constexpr_box const &other) {
        if (other.ptr)
            ptr = new T(*other.ptr);
    }

    constexpr constexpr_box(constexpr_box &&other) noexcept {
        ptr = other.ptr;
        other.ptr = nullptr;
    }

    constexpr auto operator=(constexpr_box const &other) -> constexpr_box & {
        if (this == &other)
            return *this;
        delete ptr;
        if (other.ptr)
            ptr = new T(*other.ptr);
        else
            ptr = nullptr;
        return *this;
    }

    constexpr auto operator=(constexpr_box &&other) noexcept
        -> constexpr_box & {
        delete ptr;
        ptr = other.ptr;
        other.ptr = nullptr;
        return *this;
    }

    constexpr ~constexpr_box() { delete ptr; }

    /// Dereferences the pointer.
    constexpr auto operator*() const -> T & { return *ptr; }

    /// Pointer access.
    constexpr auto operator->() const -> T * { return ptr; }

    /// Returns true when the pointer is non-null.
    constexpr explicit operator bool() const { return ptr != nullptr; }

    /// Returns the raw pointer without transferring ownership.
    constexpr auto get() const -> T * { return ptr; }
};

/// A first-class Scheme closure: a lambda node paired with a captured
/// environment.
///
/// @c node points into the core arena; it is non-owning.  @c captured is an
/// owned copy of the lexical environment at the time of closure creation.
///
/// @tparam Core The core AST type.
// 8335a467-f7e9-4d18-8d5b-54931b1d5c18
template <typename Core>
struct closure {
    Core const
        *node; ///< Non-owning pointer to the lambda node in the core arena.
    constexpr_box<env<Core, 16>> captured; ///< Captured lexical environment.

    friend constexpr auto operator==(closure<Core> const &lhs,
                                     closure<Core> const &rhs) -> bool {
        // Simple structural equality for test purposes.
        return lhs.node == rhs.node;
    }
};
// 8335a467-f7e9-4d18-8d5b-54931b1d5c18 end

/// An interned Scheme symbol at runtime (not a compile-time identifier).
struct symbol {
    std::string_view name;
    friend constexpr auto operator==(symbol const &lhs, symbol const &rhs)
        -> bool {
        return lhs.name == rhs.name;
    }
};

/// A foreign (C++) function callable from Scheme.
///
/// @c fn is a function pointer with signature @c result<value>(span<value
/// const>). This lets test harnesses and extension points install native
/// callbacks without defining new core forms.
///
/// @tparam Core The core AST type.
template <typename Core>
struct foreign_function {
    using val_t = std::variant<int, bool, builtin, closure<Core>, symbol,
                               foreign_function>;
    using sig_t = foundation::result<val_t> (*)(std::span<val_t const>);

    sig_t fn; ///< Pointer to the native implementation.

    friend constexpr auto operator==(foreign_function const &lhs,
                                     foreign_function const &rhs) -> bool {
        return lhs.fn == rhs.fn;
    }
};

/// The runtime Scheme value type: integer, boolean, builtin, closure,
/// symbol, or foreign function.
///
/// @tparam Core The core AST type (used to type @ref closure and
///              @ref foreign_function).
// 7251dd2e-066e-4c1e-8360-c33b15301355
template <typename Core>
using value = std::variant<int, bool, builtin, closure<Core>, symbol,
                           foreign_function<Core>>;
// 7251dd2e-066e-4c1e-8360-c33b15301355 end

/// A variable binding environment with linear search and most-recent-first
/// shadowing.
///
/// Uses a @ref foundation::static_vector to remain constexpr and
/// allocation-free.
/// @c lookup searches from the most recently defined binding toward the oldest,
/// so later @c define calls shadow earlier ones.
///
/// @tparam Core        The core AST type.
/// @tparam MaxBindings Maximum number of bindings.
template <typename Core, int MaxBindings>
class env {
  public:
    /// Adds a new binding for @p name → @p val.
    /// Later bindings shadow earlier ones for the same name.
    constexpr auto define(std::string_view name, value<Core> val) -> void;

    /// Looks up @p name, returning its value or an "unbound variable" error.
    [[nodiscard]] constexpr auto lookup(std::string_view name) const
        -> foundation::result<value<Core>>;

  private:
    struct binding {
        std::string_view name;
        value<Core> val;
    };
    foundation::static_vector<binding, MaxBindings> bindings_{};
};

template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::define(std::string_view name,
                                              value<Core> val) -> void {
    bindings_.push_back(binding{name, val});
}

template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::lookup(std::string_view name) const
    -> foundation::result<value<Core>> {
    for (int i = bindings_.size() - 1; i >= 0; --i) {
        if (bindings_[i].name == name)
            return bindings_[i].val;
    }
    return foundation::parse_error{{}, "unbound variable"};
}

/// Returns an environment pre-populated with the built-in @c + and @c *
/// operators.
///
/// @tparam Core        Core AST type.
/// @tparam MaxBindings Environment capacity.
template <typename Core, int MaxBindings>
[[nodiscard]] constexpr auto default_env() -> env<Core, MaxBindings> {
    env<Core, MaxBindings> e{};
    e.define("+", value<Core>{builtin{builtin_op::add}});
    e.define("*", value<Core>{builtin{builtin_op::multiply}});
    return e;
}

} // namespace smd::smdscheme::closure

#endif
