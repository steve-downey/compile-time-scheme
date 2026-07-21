// src/smd/smdlisp/reader/read_datum.hpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_READER_READ_DATUM_HPP
#define SRC_SMD_SMDLISP_READER_READ_DATUM_HPP

#include <smd/smdlisp/reader/atom.hpp>
#include <smd/smdlisp/reader/cl_chars.hpp>
#include <smd/smdlisp/reader/datum_type.hpp>
#include <smd/smdscheme/parser/cursor.hpp>
#include <smd/smdscheme/parser/parser.hpp>

#include <variant>

namespace smd::smdlisp::reader {

namespace detail {

/// Recursively reads a single datum node from @p cur, allocating sub-nodes
/// into @p arena.
///
/// Grammar: @c datum := atom | list | 'datum | #'datum | \`datum | ,datum |
/// ,\@datum (decision D6: no dotted pairs). Dispatches on the first
/// non-intertoken-space character:
/// - @c ' — quote shorthand, lowers to @ref datum_quote
/// - @c # — must be followed by @c ', sharpsign-quote shorthand, lowers to
///   @ref datum_function (see that type's docs for why this is a distinct
///   kind rather than a @c (function x) list)
/// - @c \` — backquote, lowers to @ref datum_backquote (step L18)
/// - @c , — unquote, or (if followed by @c \@) unquote-splicing; lowers to
///   @ref datum_unquote / @ref datum_unquote_splice (step L18)
/// - @c ( — list, lowers to @ref datum_list
/// - @c ) — a stray close paren is always an error here; a well-formed list
///   consumes its own closing @c ) internally
/// - anything else — delegates to @ref atom_p (integer, keyword, or
///   symbol); this function does not reimplement token scanning itself
///   (see docs/divergences/DIV-0003-atom-maximal-munch.md)
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list elements per node.
/// @param  cur    Current read position.
/// @param  arena  Arena in which new datum nodes are allocated.
/// @return A @ref smdscheme::parser::parse_result containing the datum and
///         the remaining cursor, or a @ref smdscheme::foundation::parse_error.
template <int MaxNodes, int MaxList>
constexpr auto
read_datum_node(smdscheme::parser::cursor cur,
                smdscheme::foundation::tree_arena<datum_type<MaxNodes, MaxList>,
                                                  MaxNodes> &arena)
    -> smdscheme::parser::parse_result<datum_type<MaxNodes, MaxList>> {
    using datum = datum_type<MaxNodes, MaxList>;
    using datum_f =
        typename datum_f_factory<MaxNodes, MaxList>::template type<datum>;

    cur = skip_cl_intertoken_space(cur);
    if (cur.empty())
        return smdscheme::foundation::parse_error{cur.position(),
                                                  "unexpected end of input"};

    char c = cur.peek();

    if (c == ')') {
        return smdscheme::foundation::parse_error{cur.position(),
                                                  "unexpected ')'"};
    }

    // 6bda5219-9ebb-4595-88ec-a863f20325f1
    if (c == '\'') {
        smdscheme::parser::cursor after = cur.bump();
        auto inner = read_datum_node<MaxNodes, MaxList>(after, arena);
        if (!inner.has_value())
            return inner;
        datum d{datum_f{
            datum_quote<datum, MaxNodes>{smdscheme::foundation::make_arena_box(
                arena, inner.value().value)}}};
        return smdscheme::parser::parse_state<datum>{d, inner.value().rest};
    }

    if (c == '#') {
        smdscheme::parser::cursor after = cur.bump();
        if (after.empty() || after.peek() != '\'') {
            return smdscheme::foundation::parse_error{cur.position(),
                                                      "expected #'"};
        }
        smdscheme::parser::cursor after_quote = after.bump();
        auto inner = read_datum_node<MaxNodes, MaxList>(after_quote, arena);
        if (!inner.has_value())
            return inner;
        datum d{datum_f{datum_function<datum, MaxNodes>{
            smdscheme::foundation::make_arena_box(arena,
                                                  inner.value().value)}}};
        return smdscheme::parser::parse_state<datum>{d, inner.value().rest};
    }
    // 6bda5219-9ebb-4595-88ec-a863f20325f1 end

    // Step L18 (docs/cl-pivot-plan.md): backquote/unquote/unquote-splice
    // lower to their own dedicated datum kinds, mirroring the quote /
    // sharpsign-quote treatment above. What a backquote template *means*
    // (expansion into cons/list/append builds) is decided downstream, in
    // smd::smdlisp::macroexpand; the reader only records the syntax.
    if (c == '`') {
        smdscheme::parser::cursor after = cur.bump();
        auto inner = read_datum_node<MaxNodes, MaxList>(after, arena);
        if (!inner.has_value())
            return inner;
        datum d{datum_f{datum_backquote<datum, MaxNodes>{
            smdscheme::foundation::make_arena_box(arena,
                                                  inner.value().value)}}};
        return smdscheme::parser::parse_state<datum>{d, inner.value().rest};
    }

    if (c == ',') {
        smdscheme::parser::cursor after = cur.bump();
        bool splice = !after.empty() && after.peek() == '@';
        smdscheme::parser::cursor after_marker = splice ? after.bump() : after;
        auto inner = read_datum_node<MaxNodes, MaxList>(after_marker, arena);
        if (!inner.has_value())
            return inner;
        if (splice) {
            datum d{datum_f{datum_unquote_splice<datum, MaxNodes>{
                smdscheme::foundation::make_arena_box(arena,
                                                      inner.value().value)}}};
            return smdscheme::parser::parse_state<datum>{d, inner.value().rest};
        }
        datum d{datum_f{datum_unquote<datum, MaxNodes>{
            smdscheme::foundation::make_arena_box(arena,
                                                  inner.value().value)}}};
        return smdscheme::parser::parse_state<datum>{d, inner.value().rest};
    }

    if (c == '(') {
        smdscheme::parser::cursor after = cur.bump();
        datum_list<datum, MaxNodes, MaxList> list{};
        while (true) {
            after = skip_cl_intertoken_space(after);
            if (after.empty())
                return smdscheme::foundation::parse_error{after.position(),
                                                          "expected ')'"};
            if (after.peek() == ')') {
                datum d{datum_f{list}};
                return smdscheme::parser::parse_state<datum>{d, after.bump()};
            }
            auto elem = read_datum_node<MaxNodes, MaxList>(after, arena);
            if (!elem.has_value())
                return elem;
            list.elements.push_back(smdscheme::foundation::make_arena_box(
                arena, elem.value().value));
            after = elem.value().rest;
        }
    }

    {
        auto atom_r = atom_p()(cur);
        if (!atom_r.has_value())
            return smdscheme::foundation::parse_error{cur.position(),
                                                      "expected datum"};
        atom const &a = atom_r.value().value;
        if (std::holds_alternative<atom_integer>(a)) {
            datum d{datum_f{datum_integer{std::get<atom_integer>(a).value}}};
            return smdscheme::parser::parse_state<datum>{d,
                                                         atom_r.value().rest};
        }
        if (std::holds_alternative<atom_keyword>(a)) {
            datum d{datum_f{datum_keyword{std::get<atom_keyword>(a).name}}};
            return smdscheme::parser::parse_state<datum>{d,
                                                         atom_r.value().rest};
        }
        datum d{datum_f{datum_symbol{std::get<atom_symbol>(a).name}}};
        return smdscheme::parser::parse_state<datum>{d, atom_r.value().rest};
    }
}

} // namespace detail

/// Reads a single Common Lisp datum from @p cur, allocating into @p arena.
///
/// This is the public entry point for the reader phase of the pipeline.
/// Strips leading intertoken space (whitespace and @c ; comments), then
/// delegates to the recursive @c detail::read_datum_node. The datum tree is
/// built inside @p arena; all @ref smdscheme::foundation::arena_box handles
/// in the returned datum refer to nodes in the same arena.
///
/// @tparam MaxNodes Arena capacity; also bounds tree depth.
/// @tparam MaxList  Maximum list elements per node.
/// @param  cur    Start of the source text to read.
/// @param  arena  Arena that receives all allocated nodes.
/// @return A @ref smdscheme::foundation::result wrapping a
///         @ref smdscheme::parser::parse_state (datum + remaining cursor)
///         on success, or a @ref smdscheme::foundation::parse_error on
///         failure.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
read_datum(smdscheme::parser::cursor cur,
           smdscheme::foundation::tree_arena<datum_type<MaxNodes, MaxList>,
                                             MaxNodes> &arena)
    -> smdscheme::foundation::result<
        smdscheme::parser::parse_state<datum_type<MaxNodes, MaxList>>> {
    auto r = detail::read_datum_node<MaxNodes, MaxList>(cur, arena);
    if (!r.has_value())
        return r.error();
    return r.value();
}

} // namespace smd::smdlisp::reader

#endif
