// src/smd/smdscheme/reader/reader.hpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_READER_READER_HPP
#define SRC_SMD_SMDSCHEME_READER_READER_HPP

// src/smd/smdscheme/reader/reader.hpp                          -*-C++-*-// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception#ifndef SRC_SMD_SMDSCHEME_READER_READER_HPP#define SRC_SMD_SMDSCHEME_READER_READER_HPP
#include <smd/smdscheme/parser/parser.hpp>
#include <smd/smdscheme/parser/reader_cursor.hpp>
#include <smd/smdscheme/reader/datum_tree.hpp>
#include <smd/smdscheme/reader/reader_atom.hpp>

namespace smd::smdscheme::reader {

namespace detail {

template <int MaxNodes, int MaxList>
constexpr auto read_datum_node(
    parser::cursor cur,
    foundation::tree_arena<datum_type<MaxNodes, MaxList>, MaxNodes> &arena)
    -> parser::parse_result<datum_type<MaxNodes, MaxList>> {
    using datum = datum_type<MaxNodes, MaxList>;
    using datum_f =
        typename datum_f_factory<MaxNodes, MaxList>::template type<datum>;

    cur = skip_intertoken_space(cur);
    if (cur.empty())
        return foundation::parse_error{cur.position(),
                                       "unexpected end of input"};

    char c = cur.peek();

    if (c == '#') {
        parser::cursor after = cur.bump();
        if (!after.empty()) {
            char b = after.peek();
            if (b == 't' || b == 'f') {
                parser::cursor after_bool = after.bump();
                if (after_bool.empty() ||
                    parser::is_delimiter(after_bool.peek())) {
                    datum d{datum_f{datum_boolean{b == 't'}}};
                    return parser::parse_state<datum>{d, after_bool};
                }
            }
        }
        return foundation::parse_error{cur.position(), "boolean"};
    }

    if (c == '\'') {
        parser::cursor after = cur.bump();
        auto inner = read_datum_node<MaxNodes, MaxList>(after, arena);
        if (!inner.has_value())
            return inner;
        datum d{datum_f{datum_quote<datum, MaxNodes>{
            make_arena_box(arena, inner.value().value)}}};
        return parser::parse_state<datum>{d, inner.value().rest};
    }

    if (c == '(') {
        parser::cursor after = cur.bump();
        datum_list<datum, MaxNodes, MaxList> list{};
        while (true) {
            after = skip_intertoken_space(after);
            if (after.empty())
                return foundation::parse_error{after.position(),
                                               "expected ')'"};
            if (after.peek() == ')') {
                datum d{datum_f{list}};
                return parser::parse_state<datum>{d, after.bump()};
            }
            auto elem = read_datum_node<MaxNodes, MaxList>(after, arena);
            if (!elem.has_value())
                return elem;
            list.elements.push_back(make_arena_box(arena, elem.value().value));
            after = elem.value().rest;
        }
    }

    {
        auto int_r = integer_p()(cur);
        if (int_r.has_value()) {
            datum d{datum_f{datum_integer{int_r.value().value.value}}};
            return parser::parse_state<datum>{d, int_r.value().rest};
        }
    }

    {
        auto sym_r = symbol_p()(cur);
        if (sym_r.has_value()) {
            datum d{datum_f{datum_symbol{sym_r.value().value.name}}};
            return parser::parse_state<datum>{d, sym_r.value().rest};
        }
    }

    return foundation::parse_error{cur.position(), "expected datum"};
}

} // namespace detail

template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto read_datum(
    parser::cursor cur,
    foundation::tree_arena<datum_type<MaxNodes, MaxList>, MaxNodes> &arena)
    -> foundation::result<parser::parse_state<datum_type<MaxNodes, MaxList>>> {
    auto r = detail::read_datum_node<MaxNodes, MaxList>(cur, arena);
    if (!r.has_value())
        return r.error();
    return r.value();
}
} // namespace smd::smdscheme::reader

#endif
