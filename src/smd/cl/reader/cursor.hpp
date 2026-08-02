// src/smd/cl/reader/cursor.hpp                                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_READER_CURSOR_HPP
#define SRC_SMD_CL_READER_CURSOR_HPP

#include <smd/cl/foundation/source_pos.hpp>

#include <cassert>
#include <string_view>

namespace smd::cl::reader {

/// A read position within a source text: the remaining input plus the
/// @ref foundation::source_pos of its first character.
///
/// An immutable value: @ref bump returns the advanced cursor rather than
/// mutating, so a failed parse can resume from a saved cursor.
class cursor {
  public:
    constexpr cursor() = default;

    /// Constructs a cursor at the start of @p text.
    constexpr explicit cursor(std::string_view text);

    /// Returns true if no input remains.
    [[nodiscard]] constexpr auto empty() const -> bool;

    /// Returns the current character.
    /// @pre !empty()
    [[nodiscard]] constexpr auto peek() const -> char;

    /// Returns the cursor advanced past the current character, with line
    /// and column tracking (a newline starts the next line at column 1).
    /// @pre !empty()
    [[nodiscard]] constexpr auto bump() const -> cursor;

    /// Returns the position of the current character (or of end-of-input).
    [[nodiscard]] constexpr auto position() const -> foundation::source_pos;

    /// Returns the remaining input as a view.
    [[nodiscard]] constexpr auto remaining() const -> std::string_view;

    // HIDDEN FRIEND
    friend constexpr auto operator==(cursor const &, cursor const &)
        -> bool = default;

  private:
    std::string_view text_{};
    foundation::source_pos pos_{};
};

constexpr cursor::cursor(std::string_view text) : text_{text} {}

constexpr auto cursor::empty() const -> bool { return text_.empty(); }

constexpr auto cursor::peek() const -> char {
    assert(!text_.empty());
    return text_.front();
}

constexpr auto cursor::bump() const -> cursor {
    assert(!text_.empty());
    cursor next = *this;
    char const c = text_.front();
    next.text_.remove_prefix(1);
    ++next.pos_.offset;
    if (c == '\n') {
        ++next.pos_.line;
        next.pos_.column = 1;
    } else {
        ++next.pos_.column;
    }
    return next;
}

constexpr auto cursor::position() const -> foundation::source_pos {
    return pos_;
}

constexpr auto cursor::remaining() const -> std::string_view { return text_; }

/// The value produced by one parse step: what was read plus the cursor
/// positioned after it.
///
/// @tparam T The parsed value type.
template <class T>
struct parse_state {
    T value;     ///< The parsed value.
    cursor rest; ///< The input remaining after the value.

    // HIDDEN FRIEND
    friend constexpr auto operator==(parse_state const &, parse_state const &)
        -> bool = default;
};

/// Advances @p cur while @p keep_going holds for the current character,
/// stopping at end of input. This is the reader's character-level
/// iteration primitive; higher layers compose it rather than writing
/// their own character loops.
template <class Pred>
[[nodiscard]] constexpr auto advance_while(cursor cur, Pred keep_going)
    -> cursor {
    // Substrate generic algorithm: the one character-stepping loop the
    // reader's skipping and scanning layers are built from.
    while (!cur.empty() && keep_going(cur.peek())) {
        cur = cur.bump();
    }
    return cur;
}

} // namespace smd::cl::reader

#endif
