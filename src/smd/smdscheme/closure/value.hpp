// src/smd/smdscheme/closure/value.hpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_CLOSURE_VALUE_HPP
#define SRC_SMD_SMDSCHEME_CLOSURE_VALUE_HPP

#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/foundation/static_vector.hpp>
#include <smd/smdscheme/reader/datum_tree.hpp>

#include <span>
#include <string_view>
#include <variant>

namespace smd::smdscheme::closure {

enum class builtin_op { add, multiply };

struct builtin {
    builtin_op op;
    friend constexpr auto operator==(builtin, builtin) -> bool = default;
};

// Forward declaration of env to resolve circular dependency
template <typename Core, int MaxBindings>
class env;

// A custom copyable constexpr unique pointer to avoid escaping allocations
// and allow value/closure to be copied naturally in constexpr.
template <class T>
struct constexpr_box {
    T *ptr = nullptr;
    constexpr constexpr_box() = default;
    constexpr explicit constexpr_box(T *p) : ptr(p) {}
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
    constexpr auto operator*() const -> T & { return *ptr; }
    constexpr auto operator->() const -> T * { return ptr; }
    constexpr explicit operator bool() const { return ptr != nullptr; }
    constexpr auto get() const -> T * { return ptr; }
};

template <typename Core>
struct closure {
    Core const *node;

    constexpr_box<env<Core, 16>> captured; // capture env<16> directly

    friend constexpr auto operator==(closure<Core> const &lhs,
                                     closure<Core> const &rhs) -> bool {
        // Simple structural equality for test purposes.
        return lhs.node == rhs.node;
    }
};

struct symbol {
    std::string_view name;
    friend constexpr auto operator==(symbol const &lhs, symbol const &rhs)
        -> bool {
        return lhs.name == rhs.name;
    }
};

template <typename Core>
struct foreign_function {
    using val_t = std::variant<int, bool, builtin, closure<Core>, symbol,
                               foreign_function>;
    using sig_t = foundation::result<val_t> (*)(std::span<val_t const>);
    sig_t fn;

    friend constexpr auto operator==(foreign_function const &lhs,
                                     foreign_function const &rhs) -> bool {
        return lhs.fn == rhs.fn;
    }
};

template <typename Core>
using value = std::variant<int, bool, builtin, closure<Core>, symbol,
                           foreign_function<Core>>;

template <typename Core, int MaxBindings>
class env {
  public:
    constexpr auto define(std::string_view name, value<Core> val) -> void;
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

template <typename Core, int MaxBindings>
[[nodiscard]] constexpr auto default_env() -> env<Core, MaxBindings> {
    env<Core, MaxBindings> e{};
    e.define("+", value<Core>{builtin{builtin_op::add}});
    e.define("*", value<Core>{builtin{builtin_op::multiply}});
    return e;
}

} // namespace smd::smdscheme::closure

#endif
