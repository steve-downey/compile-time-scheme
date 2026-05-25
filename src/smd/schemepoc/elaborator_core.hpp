// src/smd/schemepoc/elaborator_core.hpp                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SCHEMEPOC_ELABORATOR_CORE_HPP
#define SRC_SMD_SCHEMEPOC_ELABORATOR_CORE_HPP

#include <smd/schemepoc/datum_tree.hpp> // needed for static_vector and basic types
#include <smd/schemepoc/fix.hpp>
#include <string_view>
#include <variant>

namespace smd::schemepoc {

struct core_integer {
    int value;
};
struct core_boolean {
    bool value;
};
struct core_symbol {
    std::string_view name;
};
struct core_quote {
    std::variant<int, bool, std::string_view> atom;
};

template <typename R, int MaxNodes>
struct core_if {
    arena_box<R, MaxNodes> condition;
    arena_box<R, MaxNodes> consequent;
    arena_box<R, MaxNodes> alternative;
};

template <typename R, int MaxNodes, int MaxList>
struct core_lambda {
    static_vector<std::string_view, MaxList> params;
    arena_box<R, MaxNodes> body;
};

template <typename R, int MaxNodes, int MaxList>
struct core_application {
    arena_box<R, MaxNodes> func;
    static_vector<arena_box<R, MaxNodes>, MaxList> args;
};

template <typename R, int MaxNodes>
struct core_define {
    std::string_view name;
    arena_box<R, MaxNodes> value;
};

template <int MaxNodes, int MaxList>
struct core_f_factory {
    template <typename R>
    using type =
        std::variant<core_integer, core_boolean, core_symbol, core_quote,
                     core_if<R, MaxNodes>, core_lambda<R, MaxNodes, MaxList>,
                     core_application<R, MaxNodes, MaxList>,
                     core_define<R, MaxNodes>>;
};

template <int MaxNodes, int MaxList>
using core_type = fix<core_f_factory<MaxNodes, MaxList>::template type>;

template <int MaxNodes, int MaxList>
struct elaborated_core {
    using core = core_type<MaxNodes, MaxList>;
    tree_arena<core, MaxNodes> arena;
    core root;
};

} // namespace smd::schemepoc
#endif
