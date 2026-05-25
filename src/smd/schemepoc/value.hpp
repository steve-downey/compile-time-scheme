// src/smd/schemepoc/value.hpp                                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SCHEMEPOC_VALUE_HPP
#define SRC_SMD_SCHEMEPOC_VALUE_HPP

#include <smd/schemepoc/datum_tree.hpp>
#include <smd/schemepoc/result.hpp>
#include <smd/schemepoc/static_vector.hpp>

#include <string_view>
#include <variant>

namespace smd::schemepoc {

enum class builtin_op { add, multiply };

struct builtin {
    builtin_op op;
    friend constexpr auto operator==(builtin, builtin) -> bool = default;
};

// Forward declaration of env to resolve circular dependency
template <int MaxBindings>
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

struct closure {
    node_id node;
    constexpr_box<env<16>> captured; // capture env<16> directly

    friend constexpr auto operator==(closure const &lhs, closure const &rhs)
        -> bool {
        // Simple structural equality for test purposes.
        return lhs.node == rhs.node;
    }
};

using value = std::variant<int, bool, builtin, closure>;

template <int MaxBindings>
class env {
  public:
    constexpr auto define(std::string_view name, value val) -> void;
    [[nodiscard]] constexpr auto lookup(std::string_view name) const
        -> result<value>;

  private:
    struct binding {
        std::string_view name;
        value val;
    };
    static_vector<binding, MaxBindings> bindings_{};
};

template <int MaxBindings>
constexpr auto env<MaxBindings>::define(std::string_view name, value val)
    -> void {
    bindings_.push_back(binding{name, val});
}

template <int MaxBindings>
constexpr auto env<MaxBindings>::lookup(std::string_view name) const
    -> result<value> {
    for (int i = bindings_.size() - 1; i >= 0; --i) {
        if (bindings_[i].name == name)
            return bindings_[i].val;
    }
    return parse_error{{}, "unbound variable"};
}

template <int MaxBindings>
[[nodiscard]] constexpr auto default_env() -> env<MaxBindings> {
    env<MaxBindings> e{};
    e.define("+", value{builtin{builtin_op::add}});
    e.define("*", value{builtin{builtin_op::multiply}});
    return e;
}

} // namespace smd::schemepoc

#endif
