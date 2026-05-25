// src/smd/schemepoc/datum_tree.hpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SCHEMEPOC_DATUM_TREE_HPP
#define SRC_SMD_SCHEMEPOC_DATUM_TREE_HPP

#include <smd/schemepoc/arena_box.hpp>
#include <smd/schemepoc/fix.hpp>
#include <smd/schemepoc/static_vector.hpp>

#include <string_view>
#include <variant>

namespace smd::schemepoc {

struct datum_integer {
    int value{};
};

struct datum_symbol {
    std::string_view name{};
};

struct datum_boolean {
    bool value{};
};

template <typename R, int MaxNodes, int MaxList>
struct datum_list {
    static_vector<arena_box<R, MaxNodes>, MaxList> elements{};
};

template <typename R, int MaxNodes>
struct datum_quote {
    arena_box<R, MaxNodes> quoted{};
};

template <int MaxNodes, int MaxList>
struct datum_f_factory {
    template <typename R>
    using type = std::variant<datum_integer, datum_symbol, datum_boolean,
                              datum_list<R, MaxNodes, MaxList>,
                              datum_quote<R, MaxNodes>>;
};

template <int MaxNodes, int MaxList>
using datum_type = fix<datum_f_factory<MaxNodes, MaxList>::template type>;

} // namespace smd::schemepoc

#endif
