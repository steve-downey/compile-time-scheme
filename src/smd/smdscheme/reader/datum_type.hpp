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

/// A Scheme integer datum, e.g. @c 42.
struct datum_integer {
    int value{};
};

/// A Scheme symbol datum, e.g. @c foo.
struct datum_symbol {
    std::string_view name{}; ///< View into the source text.
};

/// A Scheme boolean datum: @c #t or @c #f.
struct datum_boolean {
    bool value{};
};

// e30e1970-1410-46bc-bc98-47f3e5854497
/// A Scheme list datum, e.g. @c (a b c).
///
/// Elements are stored as @ref foundation::arena_box handles into the reader
/// arena, avoiding recursion during the parse phase.
///
/// @tparam R        The recursive element type (the datum fix-point).
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum number of list elements.
template <typename R, int MaxNodes, int MaxList>
struct datum_list {
    foundation::static_vector<foundation::arena_box<R, MaxNodes>, MaxList>
        elements{};
};
// e30e1970-1410-46bc-bc98-47f3e5854497 end

/// A Scheme quote form, e.g. @c 'x stored as a handle to the quoted datum.
///
/// @tparam R        The recursive element type.
/// @tparam MaxNodes Arena capacity.
template <typename R, int MaxNodes>
struct datum_quote {
    foundation::arena_box<R, MaxNodes>
        quoted{}; ///< Handle to the quoted datum.
};

// 4c343e79-7835-4862-afee-9e74d3b6098c
/// Factory template that produces the open-recursive functor used by
/// @ref datum_type.
///
/// The @c type member template is the @c std::variant layer for one level of
/// the datum tree; @ref foundation::fix ties the recursion.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
struct datum_f_factory {
    /// The one-layer variant type; @p R is the recursive self-reference.
    template <typename R>
    using type = std::variant<datum_integer, datum_symbol, datum_boolean,
                              datum_list<R, MaxNodes, MaxList>,
                              datum_quote<R, MaxNodes>>;
};

/// The concrete recursive Scheme datum type, formed as the fixed point of
/// @ref datum_f_factory.
///
/// @tparam MaxNodes Maximum tree nodes (arena size).
/// @tparam MaxList  Maximum list elements per node.
template <int MaxNodes, int MaxList>
using datum_type =
    foundation::fix<datum_f_factory<MaxNodes, MaxList>::template type>;
// 4c343e79-7835-4862-afee-9e74d3b6098c end

} // namespace smd::smdscheme::reader

#endif
