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

struct closure {
    node_id node;
    friend constexpr auto operator==(closure, closure) -> bool = default;
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
