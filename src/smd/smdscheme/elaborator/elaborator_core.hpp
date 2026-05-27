// src/smd/schemepoc/elaborator_core.hpp                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_ELABORATOR_ELABORATOR_CORE_HPP
#define SRC_SMD_SMDSCHEME_ELABORATOR_ELABORATOR_CORE_HPP

#include <smd/smdscheme/foundation/fix.hpp>
#include <smd/smdscheme/reader/datum_tree.hpp> // needed for foundation::static_vector and basic types
#include <string_view>
#include <variant>

namespace smd::smdscheme::elaborator {

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
    foundation::arena_box<R, MaxNodes> condition;
    foundation::arena_box<R, MaxNodes> consequent;
    foundation::arena_box<R, MaxNodes> alternative;
};

template <typename R, int MaxNodes, int MaxList>
struct core_lambda {
    foundation::static_vector<std::string_view, MaxList> params;
    foundation::arena_box<R, MaxNodes> body;
};

template <typename R, int MaxNodes, int MaxList>
struct core_application {
    foundation::arena_box<R, MaxNodes> func;
    foundation::static_vector<foundation::arena_box<R, MaxNodes>, MaxList> args;
};

template <typename R, int MaxNodes>
struct core_define {
    std::string_view name;
    foundation::arena_box<R, MaxNodes> value;
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
using core_type = foundation::fix<core_f_factory<MaxNodes, MaxList>::template type>;

template <int MaxNodes, int MaxList>
struct elaborated_core {
    using core = core_type<MaxNodes, MaxList>;
    foundation::tree_arena<core, MaxNodes> arena;
    core root;
};

} // namespace smd::smdscheme::elaborator
#endif
