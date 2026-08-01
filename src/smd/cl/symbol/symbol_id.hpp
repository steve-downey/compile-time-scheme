// src/smd/cl/symbol/symbol_id.hpp                                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_SYMBOL_SYMBOL_ID_HPP
#define SRC_SMD_CL_SYMBOL_SYMBOL_ID_HPP

namespace smd::cl::symbol {

/// A stable identity for one symbol-table entry (decision D12).
///
/// Symbol identity is id comparison, never string comparison: two ids name
/// the same symbol exactly when they compare equal, which is what Lisp's
/// @c eq means on symbols. An id is a plain index with no dependence on any
/// table's capacity (decision D14), so program representations may carry
/// @c symbol_id freely; recovering the name or the slots requires the owning
/// @ref symbol_table.
///
/// A default-constructed id is invalid — it names no entry — and exists so
/// containers of ids can be default-initialised; @ref valid distinguishes it.
// f84353ef-c765-455d-addf-27455bba15ea
class symbol_id {
  public:
    /// Constructs the invalid id.
    constexpr symbol_id() = default;

    /// Constructs an id naming the entry at @p index in its owning table.
    /// @pre index >= 0
    constexpr explicit symbol_id(int index);

    /// Returns the entry index, or -1 for the invalid id.
    [[nodiscard]] constexpr auto index() const -> int;

    /// Returns true unless this is the invalid (default-constructed) id.
    [[nodiscard]] constexpr auto valid() const -> bool;

    // HIDDEN FRIEND
    friend constexpr auto operator==(symbol_id, symbol_id) -> bool = default;

  private:
    int index_{-1};
};
// f84353ef-c765-455d-addf-27455bba15ea end

constexpr symbol_id::symbol_id(int index) : index_{index} {}

constexpr auto symbol_id::index() const -> int { return index_; }

constexpr auto symbol_id::valid() const -> bool { return index_ >= 0; }

} // namespace smd::cl::symbol

#endif
