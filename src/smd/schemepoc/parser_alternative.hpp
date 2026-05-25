// src/smd/schemepoc/parser_alternative.hpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SCHEMEPOC_PARSER_ALTERNATIVE_HPP
#define SRC_SMD_SCHEMEPOC_PARSER_ALTERNATIVE_HPP

#include <smd/schemepoc/parser.hpp>
#include <smd/schemepoc/reader_cursor.hpp>
#include <smd/schemepoc/static_vector.hpp>

#include <optional>

namespace smd::schemepoc {

template <class PA, class PB>
[[nodiscard]] constexpr auto alt(PA pa, PB pb) {
    return pa | pb;
}

template <int Capacity, class P>
[[nodiscard]] constexpr auto many(P p) {
    return parser{[p](cursor cur) {
        using V = decltype(p(cur).value().value);
        static_vector<V, Capacity> result{};
        while (result.size() < Capacity) {
            auto r = p(cur);
            if (!r.has_value())
                break;
            result.push_back(r.value().value);
            cur = r.value().rest;
        }
        return parse_result<static_vector<V, Capacity>>{
            parse_state<static_vector<V, Capacity>>{result, cur}};
    }};
}

template <int Capacity, class P>
[[nodiscard]] constexpr auto some(P p) {
    return parser{[p](cursor cur) {
        using V = decltype(p(cur).value().value);
        auto first = p(cur);
        if (!first.has_value()) {
            return parse_result<static_vector<V, Capacity>>{first.error()};
        }
        static_vector<V, Capacity> result{};
        result.push_back(first.value().value);
        cur = first.value().rest;
        while (result.size() < Capacity) {
            auto r = p(cur);
            if (!r.has_value())
                break;
            result.push_back(r.value().value);
            cur = r.value().rest;
        }
        return parse_result<static_vector<V, Capacity>>{
            parse_state<static_vector<V, Capacity>>{result, cur}};
    }};
}

template <class P>
[[nodiscard]] constexpr auto optional(P p) {
    return parser{[p](cursor cur) {
        using V = decltype(p(cur).value().value);
        auto r  = p(cur);
        if (r.has_value()) {
            return parse_result<std::optional<V>>{
                parse_state<std::optional<V>>{r.value().value, r.value().rest}};
        }
        return parse_result<std::optional<V>>{
            parse_state<std::optional<V>>{std::optional<V>{}, cur}};
    }};
}

template <class P>
[[nodiscard]] constexpr auto lexeme(P p) {
    return parser{[p](cursor cur) {
        auto start = skip_intertoken_space(cur);
        auto r     = p(start);
        if (!r.has_value())
            return r;
        auto rest = skip_intertoken_space(r.value().rest);
        using V   = decltype(r.value().value);
        return parse_result<V>{parse_state<V>{r.value().value, rest}};
    }};
}

} // namespace smd::schemepoc

#endif
