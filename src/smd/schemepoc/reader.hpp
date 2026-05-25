// src/smd/schemepoc/reader.hpp                                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SCHEMEPOC_READER_HPP
#define SRC_SMD_SCHEMEPOC_READER_HPP

#include <smd/schemepoc/datum_tree.hpp>
#include <smd/schemepoc/parser.hpp>
#include <smd/schemepoc/reader_atom.hpp>
#include <smd/schemepoc/reader_cursor.hpp>

namespace smd::schemepoc {

namespace detail {

template <int MaxNodes, int MaxList>
constexpr auto read_datum_node(cursor cur, datum_tree<MaxNodes, MaxList> &tree)
    -> parse_result<node_id> {
    cur = skip_intertoken_space(cur);
    if (cur.empty())
        return parse_error{cur.position(), "unexpected end of input"};

    char c = cur.peek();

    if (c == '#') {
        cursor after = cur.bump();
        if (!after.empty()) {
            char b = after.peek();
            if (b == 't' || b == 'f') {
                cursor after_bool = after.bump();
                if (after_bool.empty() || is_delimiter(after_bool.peek())) {
                    node_id id = tree.add(datum_boolean{b == 't'});
                    return parse_state<node_id>{id, after_bool};
                }
            }
        }
        return parse_error{cur.position(), "boolean"};
    }

    if (c == '\'') {
        cursor after = cur.bump();
        auto inner = read_datum_node(after, tree);
        if (!inner.has_value())
            return inner;
        node_id id = tree.add(datum_quote{inner.value().value});
        return parse_state<node_id>{id, inner.value().rest};
    }

    if (c == '(') {
        cursor after = cur.bump();
        datum_list<MaxList> list{};
        while (true) {
            after = skip_intertoken_space(after);
            if (after.empty())
                return parse_error{after.position(), "expected ')'"};
            if (after.peek() == ')') {
                node_id id = tree.add(list);
                return parse_state<node_id>{id, after.bump()};
            }
            auto elem = read_datum_node(after, tree);
            if (!elem.has_value())
                return elem;
            list.elements.push_back(elem.value().value);
            after = elem.value().rest;
        }
    }

    {
        auto int_r = integer_p()(cur);
        if (int_r.has_value()) {
            node_id id = tree.add(datum_integer{int_r.value().value.value});
            return parse_state<node_id>{id, int_r.value().rest};
        }
    }

    {
        auto sym_r = symbol_p()(cur);
        if (sym_r.has_value()) {
            node_id id = tree.add(datum_symbol{sym_r.value().value.name});
            return parse_state<node_id>{id, sym_r.value().rest};
        }
    }

    return parse_error{cur.position(), "expected datum"};
}

} // namespace detail

template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto read_datum(cursor cur)
    -> parse_result<datum_tree<MaxNodes, MaxList>> {
    datum_tree<MaxNodes, MaxList> tree{};
    auto r = detail::read_datum_node(cur, tree);
    if (!r.has_value())
        return r.error();
    return parse_state<datum_tree<MaxNodes, MaxList>>{std::move(tree),
                                                      r.value().rest};
}

} // namespace smd::schemepoc

#endif
