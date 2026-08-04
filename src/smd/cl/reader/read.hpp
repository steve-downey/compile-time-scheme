// src/smd/cl/reader/read.hpp                                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_READER_READ_HPP
#define SRC_SMD_CL_READER_READ_HPP

#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/foundation/source_pos.hpp>
#include <smd/cl/reader/cursor.hpp>
#include <smd/cl/reader/datum.hpp>
#include <smd/cl/reader/number.hpp>
#include <smd/cl/reader/readtable.hpp>
#include <smd/cl/reader/token.hpp>
#include <smd/cl/symbol/symbol_id.hpp>
#include <smd/cl/symbol/symbol_table.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <utility>

namespace smd::cl::reader {

/// Default datum-tree node capacity for the convenience entry points.
inline constexpr int default_max_nodes = 256;

/// Default per-list element capacity for the convenience entry points.
inline constexpr int default_max_list = 32;

namespace detail {

/// Monadic sequencing for parse steps. The reader's counterpart of D15's
/// rule that error propagation is not a check ladder: a parse is an unfold
/// — there is no structure to @c traverse until the reader has built it —
/// so sequential steps chain through this bind instead.
///
/// It lived here until the elaborator needed the same shape; it is now
/// @ref foundation::and_then, named here because the two calls in this
/// header's public entry points spell it @c detail::and_then. The calls
/// inside this namespace would find it by argument-dependent lookup
/// regardless — @c result is a @c foundation type.
using foundation::and_then;

/// Everything one read shares: the tree being built, the symbol table
/// names are interned into, and the readtable driving dispatch. The
/// symbol-table type is a template parameter because slot types belong to
/// later pipeline stages; the reader only needs interning.
template <class SymbolTable, int MaxNodes, int MaxList>
struct read_context {
    using tree_type = datum_tree<MaxNodes, MaxList>;
    using child_list = typename tree_type::child_list;

    tree_type &tree;        ///< Receives every node the read allocates.
    SymbolTable &symbols;   ///< Receives every interned name.
    readtable const &table; ///< Drives every character decision.
};

/// Advances past a `#| ... |#` block comment body (the opening `#|`
/// already consumed), honouring nesting. An unterminated comment consumes
/// to end of input, where the caller's end-of-input handling reports.
[[nodiscard]] constexpr auto skip_block_comment(cursor cur) -> cursor {
    // Substrate generic algorithm: two-character lookahead with a nesting
    // depth makes this the skipping layer's one stateful loop.
    int depth = 1;
    while (!cur.empty() && depth > 0) {
        char const c = cur.peek();
        cursor next = cur.bump();
        if (c == '#' && !next.empty() && next.peek() == '|') {
            ++depth;
            next = next.bump();
        } else if (c == '|' && !next.empty() && next.peek() == '#') {
            --depth;
            next = next.bump();
        }
        cur = next;
    }
    return cur;
}

/// Advances past all leading intertoken space: interleaved runs of
/// whitespace, `;` line comments, and `#|...|#` block comments, to a
/// fixpoint, every decision read from @p table.
[[nodiscard]] constexpr auto skip_intertoken_space(cursor cur,
                                                   readtable const &table)
    -> cursor {
    // Substrate generic algorithm: fixpoint iteration of the three
    // skippable syntaxes, each step built on advance_while.
    while (true) {
        cur = advance_while(
            cur, [&table](char c) { return table.is_whitespace(c); });
        if (cur.empty()) {
            return cur;
        }
        if (table.macro_of(cur.peek()) == macro_kind::semicolon) {
            cur = advance_while(cur, [](char c) { return c != '\n'; });
            continue;
        }
        if (table.macro_of(cur.peek()) == macro_kind::sharpsign) {
            cursor const after = cur.bump();
            if (!after.empty() &&
                table.sharp_of(after.peek()) == sharpsign_kind::block_comment) {
                cur = skip_block_comment(after.bump());
                continue;
            }
        }
        return cur;
    }
}

/// Appends a leaf to the tree, surfacing a full tree as a @ref
/// foundation::parse_error rather than tripping the capacity assert on
/// untrusted input.
template <class Ctx>
[[nodiscard]] constexpr auto add_leaf_checked(Ctx &ctx, datum_atom atom,
                                              foundation::source_pos where)
    -> foundation::result<int> {
    if (ctx.tree.size() >= ctx.tree.capacity()) {
        return foundation::parse_error{where, "datum tree full"};
    }
    return ctx.tree.add_leaf(std::move(atom));
}

/// Appends a branch to the tree, surfacing a full tree as a @ref
/// foundation::parse_error.
template <class Ctx>
[[nodiscard]] constexpr auto
add_branch_checked(Ctx &ctx, datum_branch kind,
                   typename Ctx::child_list children,
                   foundation::source_pos where) -> foundation::result<int> {
    if (ctx.tree.size() >= ctx.tree.capacity()) {
        return foundation::parse_error{where, "datum tree full"};
    }
    return ctx.tree.add_branch(kind, std::move(children));
}

/// Interning that diagnoses a full table or name pool rather than tripping
/// the table's asserts.
///
/// It lived here until the elaborator and then the evaluator wanted copies
/// of it; it is now @ref symbol::intern_checked, and takes the table rather
/// than a read context. Note that the two definitions could not coexist —
/// while both existed, every call here was ambiguous, because the context's
/// own template argument puts @c smd::cl::symbol among its associated
/// namespaces.
using symbol::intern_checked;

template <class Ctx>
[[nodiscard]] constexpr auto read_node(cursor cur, Ctx &ctx)
    -> foundation::result<parse_state<int>>;

/// Reads the datum after a quote-family marker and wraps it in a
/// one-child branch of @p kind.
template <class Ctx>
[[nodiscard]] constexpr auto read_wrapped(cursor after_marker, Ctx &ctx,
                                          datum_branch kind,
                                          foundation::source_pos where)
    -> foundation::result<parse_state<int>> {
    return and_then(
        read_node(after_marker, ctx),
        [&](parse_state<int> const &inner)
            -> foundation::result<parse_state<int>> {
            typename Ctx::child_list children;
            children.push_back(inner.value);
            return and_then(
                add_branch_checked(ctx, kind, children, where),
                [&](int id) -> foundation::result<parse_state<int>> {
                    return parse_state<int>{id, inner.rest};
                });
        });
}

/// Reads `)`-delimited elements (the opening delimiter already consumed)
/// into a branch of @p kind: lists and vectors.
template <class Ctx>
[[nodiscard]] constexpr auto read_delimited(cursor cur, Ctx &ctx,
                                            datum_branch kind,
                                            foundation::source_pos where)
    -> foundation::result<parse_state<int>> {
    typename Ctx::child_list children;
    // Substrate generic algorithm: the reader's element unfold. The
    // elements do not exist as a structure until this loop reads them,
    // so this produces what traverse later consumes.
    while (true) {
        cur = skip_intertoken_space(cur, ctx.table);
        if (cur.empty()) {
            return foundation::parse_error{cur.position(), "expected ')'"};
        }
        if (ctx.table.macro_of(cur.peek()) == macro_kind::right_paren) {
            return and_then(
                add_branch_checked(ctx, kind, children, where),
                [&](int id) -> foundation::result<parse_state<int>> {
                    return parse_state<int>{id, cur.bump()};
                });
        }
        auto const element = read_node(cur, ctx);
        if (!element.has_value()) {
            return element;
        }
        if (children.size() >= children.capacity()) {
            return foundation::parse_error{cur.position(), "too many elements"};
        }
        children.push_back(element.value().value);
        cur = element.value().rest;
    }
}

/// Reads a string literal (the opening `"` already consumed): characters
/// verbatim, `\c` taking @c c literally, until the closing `"`.
template <class Ctx>
[[nodiscard]] constexpr auto read_string(cursor cur, Ctx &ctx,
                                         foundation::source_pos where)
    -> foundation::result<parse_state<int>> {
    datum_string contents{};
    // Substrate generic algorithm: the string accumulator; the escape
    // state makes each step depend on the previous character.
    while (!cur.empty()) {
        char c = cur.peek();
        if (ctx.table.macro_of(c) == macro_kind::double_quote) {
            return and_then(
                add_leaf_checked(ctx, datum_atom{contents}, where),
                [&](int id) -> foundation::result<parse_state<int>> {
                    return parse_state<int>{id, cur.bump()};
                });
        }
        if (ctx.table.syntax_of(c) == syntax_type::single_escape) {
            cur = cur.bump();
            if (cur.empty()) {
                break;
            }
            c = cur.peek();
        }
        if (contents.length >= max_string_chars) {
            return foundation::parse_error{where, "string too long"};
        }
        contents.storage[static_cast<std::size_t>(contents.length)] = c;
        ++contents.length;
        cur = cur.bump();
    }
    return foundation::parse_error{where, "unterminated string"};
}

/// One standard character name and its character.
struct named_char {
    std::string_view name;
    char value;
};

/// The character names this reader recognizes: the two ANSI-required
/// names, the ANSI semi-standard names, and NUL/NULL.
inline constexpr std::array<named_char, 10> named_chars{{
    {"SPACE", ' '},
    {"NEWLINE", '\n'},
    {"TAB", '\t'},
    {"PAGE", '\f'},
    {"RETURN", '\r'},
    {"LINEFEED", '\n'},
    {"BACKSPACE", '\b'},
    {"RUBOUT", '\x7f'},
    {"NUL", '\0'},
    {"NULL", '\0'},
}};

/// Case-insensitively looks up @p raw among @ref named_chars.
[[nodiscard]] constexpr auto lookup_char_name(std::string_view raw)
    -> std::optional<char> {
    auto const matches = [raw](named_char const &candidate) {
        return std::ranges::equal(raw, candidate.name, [](char a, char b) {
            return to_upper_char(a) == b;
        });
    };
    auto const found = std::ranges::find_if(named_chars, matches);
    if (found == named_chars.end()) {
        return std::nullopt;
    }
    return found->value;
}

/// Reads a character literal (the `#\` already consumed): the next
/// character itself, case-sensitively, unless it begins a run of
/// constituents — then a character name, case-insensitively (ANSI 2.4.8.1).
template <class Ctx>
[[nodiscard]] constexpr auto read_character(cursor cur, Ctx &ctx,
                                            foundation::source_pos where)
    -> foundation::result<parse_state<int>> {
    if (cur.empty()) {
        return foundation::parse_error{where, "expected character after #\\"};
    }
    char const first = cur.peek();
    cursor const after_first = cur.bump();
    bool const named =
        ctx.table.syntax_of(first) == syntax_type::constituent &&
        !after_first.empty() &&
        ctx.table.syntax_of(after_first.peek()) == syntax_type::constituent;
    auto const finish = [&](char value, cursor rest) {
        return and_then(
            add_leaf_checked(ctx, datum_atom{datum_character{value}}, where),
            [&](int id) -> foundation::result<parse_state<int>> {
                return parse_state<int>{id, rest};
            });
    };
    if (!named) {
        return finish(first, after_first);
    }
    cursor const name_end = advance_while(after_first, [&ctx](char c) {
        return ctx.table.syntax_of(c) == syntax_type::constituent;
    });
    auto const raw = cur.remaining().substr(
        0, static_cast<std::size_t>(name_end.position().offset -
                                    cur.position().offset));
    if (auto const value = lookup_char_name(raw)) {
        return finish(*value, name_end);
    }
    return foundation::parse_error{where, "unknown character name"};
}

/// Reads a rational token in @p radix (the radix prefix already
/// consumed): a fixnum when it fits, otherwise a tower spelling carrying
/// the radix (decision D19). Float syntax is decimal-only, so anything
/// but a rational here is an error.
template <class Ctx>
[[nodiscard]] constexpr auto read_radix_number(cursor cur, Ctx &ctx, int radix,
                                               foundation::source_pos where)
    -> foundation::result<parse_state<int>> {
    return and_then(
        scan_token(cur, ctx.table),
        [&](parse_state<token_text> const &token)
            -> foundation::result<parse_state<int>> {
            auto const finish = [&](datum_atom atom) {
                return and_then(
                    add_leaf_checked(ctx, std::move(atom), where),
                    [&](int id) -> foundation::result<parse_state<int>> {
                        return parse_state<int>{id, token.rest};
                    });
            };
            if (token.value.has_escape) {
                return foundation::parse_error{
                    where, "expected a rational after radix prefix"};
            }
            auto const classified = classify_number(token.value.view(), radix);
            switch (classified.cls) {
            case number_class::fixnum:
                return finish(datum_atom{datum_fixnum{classified.value}});
            case number_class::bignum:
                return finish(datum_atom{
                    datum_tower{tower_kind::bignum, radix, token.value}});
            case number_class::ratio:
                return finish(datum_atom{
                    datum_tower{tower_kind::ratio, radix, token.value}});
            case number_class::floating:
            case number_class::none:
                break;
            }
            return foundation::parse_error{
                where, "expected a rational after radix prefix"};
        });
}

/// Reads a sharpsign dispatch form (positioned at the `#`): the optional
/// infix numeric argument, then the sub-handler @ref readtable::sharp_of
/// selects.
template <class Ctx>
[[nodiscard]] constexpr auto read_sharpsign(cursor cur, Ctx &ctx)
    -> foundation::result<parse_state<int>> {
    auto const where = cur.position();
    cursor after = cur.bump();
    int argument = -1; // -1: absent
    cursor const digits_end =
        advance_while(after, [](char c) { return c >= '0' && c <= '9'; });
    if (digits_end.position().offset > after.position().offset) {
        auto const digits = after.remaining().substr(
            0, static_cast<std::size_t>(digits_end.position().offset -
                                        after.position().offset));
        argument = std::ranges::fold_left(digits, 0, [](int acc, char c) {
            // Cap far above the largest valid radix; enough to reject.
            return std::min(acc * 10 + (c - '0'), 999);
        });
        after = digits_end;
    }
    if (after.empty()) {
        return foundation::parse_error{where,
                                       "unexpected end of input after '#'"};
    }
    auto const reject_argument =
        [&](auto continuation) -> foundation::result<parse_state<int>> {
        if (argument >= 0) {
            return foundation::parse_error{
                where, "unexpected numeric argument after '#'"};
        }
        return continuation();
    };
    switch (ctx.table.sharp_of(after.peek())) {
    case sharpsign_kind::function_quote:
        return reject_argument([&] {
            return read_wrapped(after.bump(), ctx, datum_branch::function,
                                where);
        });
    case sharpsign_kind::vector_open:
        if (argument >= 0) {
            return foundation::parse_error{
                where, "sized #n(...) vectors not yet supported"};
        }
        return read_delimited(after.bump(), ctx, datum_branch::vector, where);
    case sharpsign_kind::character_literal:
        return reject_argument(
            [&] { return read_character(after.bump(), ctx, where); });
    case sharpsign_kind::radix_binary:
        return reject_argument(
            [&] { return read_radix_number(after.bump(), ctx, 2, where); });
    case sharpsign_kind::radix_octal:
        return reject_argument(
            [&] { return read_radix_number(after.bump(), ctx, 8, where); });
    case sharpsign_kind::radix_hex:
        return reject_argument(
            [&] { return read_radix_number(after.bump(), ctx, 16, where); });
    case sharpsign_kind::radix_n:
        if (argument < 2 || argument > 36) {
            return foundation::parse_error{where,
                                           "radix must be between 2 and 36"};
        }
        return read_radix_number(after.bump(), ctx, argument, where);
    case sharpsign_kind::block_comment: // consumed as intertoken space
    case sharpsign_kind::none:
        break;
    }
    return foundation::parse_error{where, "unsupported '#' syntax"};
}

/// Reads a token datum: scans the whole token, then classifies it
/// (DIV-0003, whole-token classification): a number by the decimal
/// grammar, else a keyword (leading unescaped colon), else a symbol —
/// interned either way (decision D12).
template <class Ctx>
[[nodiscard]] constexpr auto read_token_datum(cursor cur, Ctx &ctx)
    -> foundation::result<parse_state<int>> {
    auto const where = cur.position();
    return and_then(
        scan_token(cur, ctx.table),
        [&](parse_state<token_text> const &token)
            -> foundation::result<parse_state<int>> {
            auto const finish = [&](datum_atom atom) {
                return and_then(
                    add_leaf_checked(ctx, std::move(atom), where),
                    [&](int id) -> foundation::result<parse_state<int>> {
                        return parse_state<int>{id, token.rest};
                    });
            };
            if (!token.value.has_escape) {
                auto const classified = classify_number(token.value.view(), 10);
                switch (classified.cls) {
                case number_class::fixnum:
                    return finish(datum_atom{datum_fixnum{classified.value}});
                case number_class::bignum:
                    return finish(datum_atom{
                        datum_tower{tower_kind::bignum, 10, token.value}});
                case number_class::ratio:
                    return finish(datum_atom{
                        datum_tower{tower_kind::ratio, 10, token.value}});
                case number_class::floating:
                    return finish(datum_atom{
                        datum_tower{tower_kind::floating, 10, token.value}});
                case number_class::none:
                    break;
                }
            }
            auto const view = token.value.view();
            bool const keyword = !token.value.has_escape && view.size() > 1 &&
                                 view.front() == ':';
            return and_then(intern_checked(ctx.symbols, view, where),
                            [&](symbol::symbol_id id)
                                -> foundation::result<parse_state<int>> {
                                return finish(
                                    keyword ? datum_atom{datum_keyword{id}}
                                            : datum_atom{datum_symbol{id}});
                            });
        });
}

// b803edcd-959d-4364-94f8-13fb256e7b9b
/// Reads one datum node: skips intertoken space, then dispatches on the
/// current character through the readtable (decision D19: this lookup —
/// not character tests — is the reader's spine).
template <class Ctx>
[[nodiscard]] constexpr auto read_node(cursor cur, Ctx &ctx)
    -> foundation::result<parse_state<int>> {
    cur = skip_intertoken_space(cur, ctx.table);
    if (cur.empty()) {
        return foundation::parse_error{cur.position(),
                                       "unexpected end of input"};
    }
    auto const where = cur.position();
    switch (ctx.table.macro_of(cur.peek())) {
    case macro_kind::none:
        return read_token_datum(cur, ctx);
    case macro_kind::left_paren:
        return read_delimited(cur.bump(), ctx, datum_branch::list, where);
    case macro_kind::right_paren:
        return foundation::parse_error{where, "unexpected ')'"};
    case macro_kind::single_quote:
        return read_wrapped(cur.bump(), ctx, datum_branch::quote, where);
    case macro_kind::backquote:
        return read_wrapped(cur.bump(), ctx, datum_branch::backquote, where);
    case macro_kind::comma: {
        cursor const after = cur.bump();
        if (!after.empty() && after.peek() == '@') {
            return read_wrapped(after.bump(), ctx, datum_branch::unquote_splice,
                                where);
        }
        return read_wrapped(after, ctx, datum_branch::unquote, where);
    }
    case macro_kind::double_quote:
        return read_string(cur.bump(), ctx, where);
    case macro_kind::semicolon: // consumed as intertoken space
        break;
    case macro_kind::sharpsign:
        return read_sharpsign(cur, ctx);
    }
    return foundation::parse_error{where, "expected datum"};
}
// b803edcd-959d-4364-94f8-13fb256e7b9b end

} // namespace detail

/// Reads one Common Lisp datum from @p cur, interning symbol names into
/// @p symbols, with every character decision driven by @p table.
///
/// Returns the datum as a self-contained @ref datum_tree value (root set)
/// plus the cursor after the datum, so callers can read a sequence of
/// data; or a @ref foundation::parse_error with the failure position.
///
/// @tparam MaxNodes    Datum-tree node capacity.
/// @tparam MaxList     Maximum elements per list or vector.
/// @tparam SymbolTable The symbol table instantiation; needs @c find,
///                     @c intern, and the capacity observers of
///                     @c symbol::symbol_table.
template <int MaxNodes = default_max_nodes, int MaxList = default_max_list,
          class SymbolTable>
[[nodiscard]] constexpr auto
read_datum(cursor cur, SymbolTable &symbols,
           readtable const &table = standard_readtable)
    -> foundation::result<parse_state<datum_tree<MaxNodes, MaxList>>> {
    datum_tree<MaxNodes, MaxList> tree;
    detail::read_context<SymbolTable, MaxNodes, MaxList> ctx{tree, symbols,
                                                             table};
    return detail::and_then(
        detail::read_node(cur, ctx),
        [&](parse_state<int> const &node)
            -> foundation::result<parse_state<datum_tree<MaxNodes, MaxList>>> {
            tree.set_root(node.value);
            return parse_state<datum_tree<MaxNodes, MaxList>>{tree, node.rest};
        });
}

/// Reads exactly one datum from @p source: convenience over @ref
/// read_datum for callers with a whole spelling in hand. Anything but
/// intertoken space after the datum is an error.
template <int MaxNodes = default_max_nodes, int MaxList = default_max_list,
          class SymbolTable>
[[nodiscard]] constexpr auto read(std::string_view source, SymbolTable &symbols,
                                  readtable const &table = standard_readtable)
    -> foundation::result<datum_tree<MaxNodes, MaxList>> {
    return detail::and_then(
        read_datum<MaxNodes, MaxList>(cursor{source}, symbols, table),
        [&](parse_state<datum_tree<MaxNodes, MaxList>> const &state)
            -> foundation::result<datum_tree<MaxNodes, MaxList>> {
            cursor const rest =
                detail::skip_intertoken_space(state.rest, table);
            if (!rest.empty()) {
                return foundation::parse_error{rest.position(),
                                               "unexpected trailing input"};
            }
            return state.value;
        });
}

} // namespace smd::cl::reader

#endif
