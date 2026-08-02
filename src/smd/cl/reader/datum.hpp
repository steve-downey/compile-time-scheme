// src/smd/cl/reader/datum.hpp                                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_READER_DATUM_HPP
#define SRC_SMD_CL_READER_DATUM_HPP

#include <smd/cl/foundation/tagged_tree.hpp>
#include <smd/cl/foundation/tagged_tree_instances.hpp>
#include <smd/cl/reader/token.hpp>
#include <smd/cl/symbol/symbol_id.hpp>

#include <array>
#include <cstddef>
#include <string_view>
#include <variant>

namespace smd::cl::reader {

/// Maximum accepted length, in characters, of a string literal.
inline constexpr int max_string_chars = 128;

/// A fixnum datum, e.g. @c 42 — the one numeric-tower member that is
/// executable today. The fixnum range is that of @c int, matching the
/// old tree; integer spellings outside it read as @ref datum_tower.
struct datum_fixnum {
    int value = 0; ///< The parsed integer value.

    // HIDDEN FRIEND
    friend constexpr auto operator==(datum_fixnum, datum_fixnum)
        -> bool = default;
};

/// A symbol datum. The name is interned (decision D12): the datum carries
/// a @ref symbol::symbol_id into the symbol table the reader was given,
/// never a view into source text, so nothing dangles when the source
/// buffer goes away and @c eq is id comparison.
struct datum_symbol {
    symbol::symbol_id id{}; ///< The interned symbol.

    // HIDDEN FRIEND
    friend constexpr auto operator==(datum_symbol, datum_symbol)
        -> bool = default;
};

/// A keyword datum (@c :foo). A distinct datum kind from @ref datum_symbol
/// (the old tree's decision D7 carries over), and interned like one — under
/// its source spelling with the leading colon (@c ":FOO"), so a keyword and
/// a like-named symbol are distinct table entries and @c eq works on
/// keywords. A future package system replaces the colon-in-name convention;
/// until then @c symbol-name recovery must strip it.
struct datum_keyword {
    symbol::symbol_id id{}; ///< The interned keyword, name including colon.

    // HIDDEN FRIEND
    friend constexpr auto operator==(datum_keyword, datum_keyword)
        -> bool = default;
};

/// A character datum, e.g. @c #\a or @c #\Newline (decision D19: new in
/// the rebuild's reader).
struct datum_character {
    char value = 0; ///< The character.

    // HIDDEN FRIEND
    friend constexpr auto operator==(datum_character, datum_character)
        -> bool = default;
};

/// A string datum, e.g. @c "hello" (decision D19: new in the rebuild's
/// reader). Own fixed storage — escape processing rewrites characters, so
/// the contents cannot alias the source text.
struct datum_string {
    std::array<char, max_string_chars> storage{}; ///< The characters.
    int length = 0; ///< Number of valid characters in storage.

    /// Returns the contents as a view into this object's own storage,
    /// valid while this @c datum_string (or the copy it was taken from)
    /// is alive.
    [[nodiscard]] constexpr auto view() const -> std::string_view {
        return std::string_view(storage.data(),
                                static_cast<std::size_t>(length));
    }

    // HIDDEN FRIEND
    friend constexpr auto operator==(datum_string const &, datum_string const &)
        -> bool = default;
};

/// Which numeric-tower member a @ref datum_tower spelling denotes.
enum class tower_kind : unsigned char {
    bignum,  ///< Integer syntax outside the fixnum range.
    ratio,   ///< Ratio syntax, e.g. @c 1/2.
    floating ///< Float syntax, e.g. @c 1.5e3.
};

/// A numeric-tower literal that is readable but not yet executable
/// (decision D19): the datum carries what the evaluator cannot yet
/// represent as its spelling — digits and radix — so the syntax lands now
/// and each tower member's semantic lane can pick its literals up later
/// without re-reading source.
struct datum_tower {
    tower_kind kind = tower_kind::bignum; ///< Which tower member.
    int radix = 10;        ///< The radix the digits are spelled in.
    token_text spelling{}; ///< The token's spelling, sign included, radix
                           ///< prefix excluded (own storage).

    // HIDDEN FRIEND
    friend constexpr auto operator==(datum_tower const &, datum_tower const &)
        -> bool = default;
};

// 23ee18c4-af42-42b1-b97a-3513b932e5ef
/// A datum-tree leaf: one atom.
using datum_atom = std::variant<datum_fixnum, datum_symbol, datum_keyword,
                                datum_character, datum_string, datum_tower>;

/// A datum-tree branch tag: what a compound datum is.
///
/// The quote family records source reality (@c 'x is not a @c (quote x)
/// list token) and defers all interpretation to later passes, carrying
/// over the old tree's split; @c vector is new with decision D19.
enum class datum_branch : unsigned char {
    list,          ///< @c (a b c); children are the elements.
    vector,        ///< @c #(a b c); children are the elements.
    quote,         ///< @c 'x; one child, the quoted datum.
    function,      ///< @c #'f; one child, the referenced datum.
    backquote,     ///< @c `template; one child, the template.
    unquote,       ///< @c ,x; one child, the unquoted datum.
    unquote_splice ///< @c ,@x; one child, the spliced datum.
};
// 23ee18c4-af42-42b1-b97a-3513b932e5ef end

/// The datum tree: a self-contained @ref foundation::tagged_tree value
/// whose leaves are @ref datum_atom and whose branches are
/// @ref datum_branch, carrying the Foldable and Traversable instances of
/// <smd/cl/foundation/tagged_tree_instances.hpp> (decision D15). The
/// reader adds children before parents, left to right, so those
/// instances' documented leaf order is left-to-right source order.
///
/// @tparam MaxNodes Maximum tree nodes.
/// @tparam MaxList  Maximum elements per list or vector.
template <int MaxNodes, int MaxList>
using datum_tree =
    foundation::tagged_tree<datum_atom, datum_branch, MaxNodes, MaxList>;

} // namespace smd::cl::reader

#endif
