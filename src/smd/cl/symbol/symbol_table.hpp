// src/smd/cl/symbol/symbol_table.hpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_SYMBOL_SYMBOL_TABLE_HPP
#define SRC_SMD_CL_SYMBOL_SYMBOL_TABLE_HPP

#include <smd/cl/foundation/static_vector.hpp>
#include <smd/cl/symbol/symbol_id.hpp>

#include <algorithm>
#include <cassert>
#include <optional>
#include <string_view>
#include <utility>

namespace smd::cl::symbol {

/// The interned symbol table of decision D12.
///
/// A symbol is an entry in this table, referred to by a stable
/// @ref symbol_id, carrying a name, a value slot, a function slot, and a
/// macro slot. Identity is id comparison, not string comparison:
/// @c intern("x") twice yields equal ids, and equality of ids is what @c eq
/// means on symbols.
///
/// The table owns its name characters. Interning copies the characters into
/// the table's own pool, so a compiled program that carries this table is
/// self-contained: names are never views into the reader's datum arena or
/// any other caller storage.
///
/// An uninterned symbol (the substrate for @c gensym) is an ordinary entry
/// created by @ref make_uninterned: it carries a name for printing, but no
/// name lookup ever finds it, and interning its name yields a different
/// symbol.
///
/// The three slots are independently readable and writable, and each is
/// empty (unbound) until written. Their types are template parameters
/// because what a binding *is* belongs to later pipeline stages; the table
/// only stores them.
///
/// Capacities parameterise the table because the table is storage; they
/// never leak into @ref symbol_id or any value type (decision D14).
///
/// @tparam ValueSlot    What the value slot of a symbol holds.
/// @tparam FunctionSlot What the function slot of a symbol holds.
/// @tparam MacroSlot    What the macro slot of a symbol holds.
/// @tparam MaxSymbols   Maximum number of entries, interned or not.
/// @tparam MaxNameChars Maximum total characters across all names.
template <class ValueSlot, class FunctionSlot, class MacroSlot, int MaxSymbols,
          int MaxNameChars>
class symbol_table {
  public:
    constexpr symbol_table() = default;

    /// Interns @p name: returns the id of the existing interned entry with
    /// this name if there is one, otherwise creates a fresh entry whose name
    /// is a copy of @p name in the table's own pool.
    /// @pre if @p name is not already interned, there is room for one more
    ///      entry and for @c name.size() more pooled characters
    constexpr auto intern(std::string_view name) -> symbol_id;

    /// Creates a fresh uninterned entry carrying a copy of @p name.
    /// The entry is never found by @ref find or @ref intern, and repeated
    /// calls with the same name yield distinct ids.
    /// @pre there is room for one more entry and for @c name.size() more
    ///      pooled characters
    constexpr auto make_uninterned(std::string_view name) -> symbol_id;

    /// Returns the id of the interned entry named @p name, or @c nullopt if
    /// no interned entry has that name. Uninterned entries are invisible.
    [[nodiscard]] constexpr auto find(std::string_view name) const
        -> std::optional<symbol_id>;

    /// Returns the name of the entry @p id, as a view into the table's own
    /// character pool.
    /// @pre @p id names an entry of this table
    [[nodiscard]] constexpr auto name(symbol_id id) const -> std::string_view;

    /// Returns true if @p id names an interned entry, false for an
    /// uninterned one.
    /// @pre @p id names an entry of this table
    [[nodiscard]] constexpr auto is_interned(symbol_id id) const -> bool;

    /// Returns the number of entries, interned or not.
    [[nodiscard]] constexpr auto size() const -> int;

    /// Returns @p MaxSymbols, so callers can check for room before an
    /// @ref intern or @ref make_uninterned that must not overflow.
    [[nodiscard]] constexpr auto capacity() const -> int;

    /// Returns the number of pooled name characters in use.
    [[nodiscard]] constexpr auto name_chars_used() const -> int;

    /// Returns @p MaxNameChars, the pooled-character capacity.
    [[nodiscard]] constexpr auto name_chars_capacity() const -> int;

    /// Returns the value slot of @p id; empty means unbound.
    /// @pre @p id names an entry of this table
    [[nodiscard]] constexpr auto value(symbol_id id) const
        -> std::optional<ValueSlot> const &;

    /// Writes the value slot of @p id, leaving the other slots untouched.
    /// @pre @p id names an entry of this table
    constexpr auto set_value(symbol_id id, ValueSlot v) -> void;

    /// Returns the function slot of @p id; empty means unbound.
    /// @pre @p id names an entry of this table
    [[nodiscard]] constexpr auto function(symbol_id id) const
        -> std::optional<FunctionSlot> const &;

    /// Writes the function slot of @p id, leaving the other slots untouched.
    /// @pre @p id names an entry of this table
    constexpr auto set_function(symbol_id id, FunctionSlot f) -> void;

    /// Returns the macro slot of @p id; empty means no macro definition.
    /// @pre @p id names an entry of this table
    [[nodiscard]] constexpr auto macro(symbol_id id) const
        -> std::optional<MacroSlot> const &;

    /// Writes the macro slot of @p id, leaving the other slots untouched.
    /// @pre @p id names an entry of this table
    constexpr auto set_macro(symbol_id id, MacroSlot m) -> void;

  private:
    // e8219367-476a-4b85-882a-87d179d1a271
    struct entry {
        int name_offset{};
        int name_length{};
        bool interned{};
        std::optional<ValueSlot> value{};
        std::optional<FunctionSlot> function{};
        std::optional<MacroSlot> macro{};
    };

    [[nodiscard]] constexpr auto name_of(entry const &e) const
        -> std::string_view;
    constexpr auto append_entry(std::string_view name, bool interned)
        -> symbol_id;
    [[nodiscard]] constexpr auto entry_at(symbol_id id) -> entry &;
    [[nodiscard]] constexpr auto entry_at(symbol_id id) const -> entry const &;

    foundation::static_vector<entry, MaxSymbols> entries_{};
    foundation::static_vector<char, MaxNameChars> chars_{};
    // e8219367-476a-4b85-882a-87d179d1a271 end
};

// 19c820e1-8305-4f63-a239-8234a827ca29
template <class ValueSlot, class FunctionSlot, class MacroSlot, int MaxSymbols,
          int MaxNameChars>
constexpr auto symbol_table<ValueSlot, FunctionSlot, MacroSlot, MaxSymbols,
                            MaxNameChars>::intern(std::string_view name)
    -> symbol_id {
    if (auto const existing = find(name)) {
        return *existing;
    }
    return append_entry(name, true);
}
// 19c820e1-8305-4f63-a239-8234a827ca29 end

template <class ValueSlot, class FunctionSlot, class MacroSlot, int MaxSymbols,
          int MaxNameChars>
constexpr auto
symbol_table<ValueSlot, FunctionSlot, MacroSlot, MaxSymbols,
             MaxNameChars>::make_uninterned(std::string_view name)
    -> symbol_id {
    return append_entry(name, false);
}

// bc4cbac8-befd-44db-a89b-6b6be78b1714
template <class ValueSlot, class FunctionSlot, class MacroSlot, int MaxSymbols,
          int MaxNameChars>
constexpr auto symbol_table<ValueSlot, FunctionSlot, MacroSlot, MaxSymbols,
                            MaxNameChars>::find(std::string_view name) const
    -> std::optional<symbol_id> {
    auto const interned_with_name = [&](entry const &e) {
        return e.interned && name_of(e) == name;
    };
    auto const it = std::ranges::find_if(entries_, interned_with_name);
    if (it == entries_.end()) {
        return std::nullopt;
    }
    return symbol_id{static_cast<int>(it - entries_.begin())};
}
// bc4cbac8-befd-44db-a89b-6b6be78b1714 end

template <class ValueSlot, class FunctionSlot, class MacroSlot, int MaxSymbols,
          int MaxNameChars>
constexpr auto symbol_table<ValueSlot, FunctionSlot, MacroSlot, MaxSymbols,
                            MaxNameChars>::name(symbol_id id) const
    -> std::string_view {
    return name_of(entry_at(id));
}

template <class ValueSlot, class FunctionSlot, class MacroSlot, int MaxSymbols,
          int MaxNameChars>
constexpr auto symbol_table<ValueSlot, FunctionSlot, MacroSlot, MaxSymbols,
                            MaxNameChars>::is_interned(symbol_id id) const
    -> bool {
    return entry_at(id).interned;
}

template <class ValueSlot, class FunctionSlot, class MacroSlot, int MaxSymbols,
          int MaxNameChars>
constexpr auto symbol_table<ValueSlot, FunctionSlot, MacroSlot, MaxSymbols,
                            MaxNameChars>::size() const -> int {
    return entries_.size();
}

template <class ValueSlot, class FunctionSlot, class MacroSlot, int MaxSymbols,
          int MaxNameChars>
constexpr auto symbol_table<ValueSlot, FunctionSlot, MacroSlot, MaxSymbols,
                            MaxNameChars>::capacity() const -> int {
    return MaxSymbols;
}

template <class ValueSlot, class FunctionSlot, class MacroSlot, int MaxSymbols,
          int MaxNameChars>
constexpr auto symbol_table<ValueSlot, FunctionSlot, MacroSlot, MaxSymbols,
                            MaxNameChars>::name_chars_used() const -> int {
    return chars_.size();
}

template <class ValueSlot, class FunctionSlot, class MacroSlot, int MaxSymbols,
          int MaxNameChars>
constexpr auto symbol_table<ValueSlot, FunctionSlot, MacroSlot, MaxSymbols,
                            MaxNameChars>::name_chars_capacity() const -> int {
    return MaxNameChars;
}

template <class ValueSlot, class FunctionSlot, class MacroSlot, int MaxSymbols,
          int MaxNameChars>
constexpr auto symbol_table<ValueSlot, FunctionSlot, MacroSlot, MaxSymbols,
                            MaxNameChars>::value(symbol_id id) const
    -> std::optional<ValueSlot> const & {
    return entry_at(id).value;
}

template <class ValueSlot, class FunctionSlot, class MacroSlot, int MaxSymbols,
          int MaxNameChars>
constexpr auto symbol_table<ValueSlot, FunctionSlot, MacroSlot, MaxSymbols,
                            MaxNameChars>::set_value(symbol_id id, ValueSlot v)
    -> void {
    entry_at(id).value = std::move(v);
}

template <class ValueSlot, class FunctionSlot, class MacroSlot, int MaxSymbols,
          int MaxNameChars>
constexpr auto symbol_table<ValueSlot, FunctionSlot, MacroSlot, MaxSymbols,
                            MaxNameChars>::function(symbol_id id) const
    -> std::optional<FunctionSlot> const & {
    return entry_at(id).function;
}

template <class ValueSlot, class FunctionSlot, class MacroSlot, int MaxSymbols,
          int MaxNameChars>
constexpr auto symbol_table<ValueSlot, FunctionSlot, MacroSlot, MaxSymbols,
                            MaxNameChars>::set_function(symbol_id id,
                                                        FunctionSlot f)
    -> void {
    entry_at(id).function = std::move(f);
}

template <class ValueSlot, class FunctionSlot, class MacroSlot, int MaxSymbols,
          int MaxNameChars>
constexpr auto symbol_table<ValueSlot, FunctionSlot, MacroSlot, MaxSymbols,
                            MaxNameChars>::macro(symbol_id id) const
    -> std::optional<MacroSlot> const & {
    return entry_at(id).macro;
}

template <class ValueSlot, class FunctionSlot, class MacroSlot, int MaxSymbols,
          int MaxNameChars>
constexpr auto symbol_table<ValueSlot, FunctionSlot, MacroSlot, MaxSymbols,
                            MaxNameChars>::set_macro(symbol_id id, MacroSlot m)
    -> void {
    entry_at(id).macro = std::move(m);
}

template <class ValueSlot, class FunctionSlot, class MacroSlot, int MaxSymbols,
          int MaxNameChars>
constexpr auto symbol_table<ValueSlot, FunctionSlot, MacroSlot, MaxSymbols,
                            MaxNameChars>::name_of(entry const &e) const
    -> std::string_view {
    return std::string_view{chars_.begin() + e.name_offset,
                            static_cast<std::size_t>(e.name_length)};
}

template <class ValueSlot, class FunctionSlot, class MacroSlot, int MaxSymbols,
          int MaxNameChars>
constexpr auto symbol_table<ValueSlot, FunctionSlot, MacroSlot, MaxSymbols,
                            MaxNameChars>::append_entry(std::string_view name,
                                                        bool interned)
    -> symbol_id {
    assert(entries_.size() < entries_.capacity());
    assert(chars_.size() + static_cast<int>(name.size()) <= chars_.capacity());
    entry e{};
    e.name_offset = chars_.size();
    e.name_length = static_cast<int>(name.size());
    e.interned = interned;
    std::ranges::for_each(name, [this](char c) { chars_.push_back(c); });
    entries_.push_back(std::move(e));
    return symbol_id{entries_.size() - 1};
}

template <class ValueSlot, class FunctionSlot, class MacroSlot, int MaxSymbols,
          int MaxNameChars>
constexpr auto symbol_table<ValueSlot, FunctionSlot, MacroSlot, MaxSymbols,
                            MaxNameChars>::entry_at(symbol_id id) -> entry & {
    assert(id.valid() && id.index() < entries_.size());
    return entries_[id.index()];
}

template <class ValueSlot, class FunctionSlot, class MacroSlot, int MaxSymbols,
          int MaxNameChars>
constexpr auto symbol_table<ValueSlot, FunctionSlot, MacroSlot, MaxSymbols,
                            MaxNameChars>::entry_at(symbol_id id) const
    -> entry const & {
    assert(id.valid() && id.index() < entries_.size());
    return entries_[id.index()];
}

} // namespace smd::cl::symbol

#endif
