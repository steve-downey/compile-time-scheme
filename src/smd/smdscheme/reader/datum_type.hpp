// src/smd/smdscheme/reader/datum_type.hpp -*-C++-*- SPDX-License-Identifier:
// Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_READER_DATUM_TYPE_HPP
#define SRC_SMD_SMDSCHEME_READER_DATUM_TYPE_HPP

#include <smd/smdscheme/foundation/arena_box.hpp>
#include <smd/smdscheme/foundation/fix.hpp>
#include <smd/smdscheme/foundation/static_vector.hpp>

#include <string_view>
#include <variant>

namespace smd::smdscheme::reader {

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
    foundation::static_vector<foundation::arena_box<R, MaxNodes>, MaxList>
        elements{};
};

template <typename R, int MaxNodes>
struct datum_quote {
    foundation::arena_box<R, MaxNodes> quoted{};
};

template <int MaxNodes, int MaxList>
struct datum_f_factory {
    template <typename R>
    using type = std::variant<datum_integer, datum_symbol, datum_boolean,
                              datum_list<R, MaxNodes, MaxList>,
                              datum_quote<R, MaxNodes>>;
};

template <int MaxNodes, int MaxList>
using datum_type =
    foundation::fix<datum_f_factory<MaxNodes, MaxList>::template type>;

} // namespace smd::smdscheme::reader

#endif
