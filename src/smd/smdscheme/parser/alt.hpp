// src/smd/smdscheme/parser/alt.hpp -*-C++-*- SPDX-License-Identifier:
// Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_PARSER_ALT_HPP
#define SRC_SMD_SMDSCHEME_PARSER_ALT_HPP

#include <smd/smdscheme/foundation/static_vector.hpp>
#include <smd/smdscheme/parser/cursor.hpp>
#include <smd/smdscheme/parser/parser.hpp>

#include <optional>

namespace smd::smdscheme::parser {

/// Ordered-choice combinator; thin alias for the @c | operator.
///
/// Tries @p pa; if it fails without consuming input, tries @p pb.
///
/// @tparam PA First alternative parser.
/// @tparam PB Second alternative parser.
template <parser_like PA, parser_like PB>
[[nodiscard]] constexpr auto alt(PA pa, PB pb) {
    return pa | pb;
}

// e7d2b575-1f00-46bd-8365-1abbe0de1b63
/// Returns a parser that applies @p p zero or more times, collecting at most
/// @c Capacity results into a @ref foundation::static_vector.
///
/// Always succeeds (zero repetitions is valid).
///
/// @tparam Capacity Maximum number of repetitions.
/// @tparam P        Parser to repeat.
template <int Capacity, parser_like P>
[[nodiscard]] constexpr auto many(P p) {
    return parser{[p](cursor cur) {
        using V = decltype(p(cur).value().value);
        foundation::static_vector<V, Capacity> result{};
        while (result.size() < Capacity) {
            auto r = p(cur);
            if (!r.has_value())
                break;
            result.push_back(r.value().value);
            cur = r.value().rest;
        }
        return parse_result<foundation::static_vector<V, Capacity>>{
            parse_state<foundation::static_vector<V, Capacity>>{result, cur}};
    }};
}
// e7d2b575-1f00-46bd-8365-1abbe0de1b63 end

/// Returns a parser that applies @p p one or more times, collecting at most
/// @c Capacity results.
///
/// Fails if @p p does not match at least once.
///
/// @tparam Capacity Maximum number of repetitions.
/// @tparam P        Parser to repeat.
template <int Capacity, parser_like P>
[[nodiscard]] constexpr auto some(P p) {
    return parser{[p](cursor cur) {
        using V = decltype(p(cur).value().value);
        auto first = p(cur);
        if (!first.has_value()) {
            return parse_result<foundation::static_vector<V, Capacity>>{
                first.error()};
        }
        foundation::static_vector<V, Capacity> result{};
        result.push_back(first.value().value);
        cur = first.value().rest;
        while (result.size() < Capacity) {
            auto r = p(cur);
            if (!r.has_value())
                break;
            result.push_back(r.value().value);
            cur = r.value().rest;
        }
        return parse_result<foundation::static_vector<V, Capacity>>{
            parse_state<foundation::static_vector<V, Capacity>>{result, cur}};
    }};
}

/// Returns a parser that tries @p p and wraps the result in @c std::optional.
///
/// Always succeeds: yields @c std::nullopt if @p p fails without consuming
/// input, otherwise yields the value.
///
/// @tparam P Parser to attempt.
template <parser_like P>
[[nodiscard]] constexpr auto optional(P p) {
    return parser{[p](cursor cur) {
        using V = decltype(p(cur).value().value);
        auto r = p(cur);
        if (r.has_value()) {
            return parse_result<std::optional<V>>{
                parse_state<std::optional<V>>{r.value().value, r.value().rest}};
        }
        return parse_result<std::optional<V>>{
            parse_state<std::optional<V>>{std::optional<V>{}, cur}};
    }};
}

// 07cc42ff-6ee4-4b1a-90b0-b81c99574723
/// Returns a parser that strips leading and trailing inter-token whitespace
/// around @p p.
///
/// This is the standard way to make a token parser whitespace-insensitive
/// in a recursive-descent setting.
///
/// @tparam P Parser to wrap.
template <parser_like P>
[[nodiscard]] constexpr auto lexeme(P p) {
    return parser{[p](cursor cur) {
        auto start = skip_intertoken_space(cur);
        auto r = p(start);
        if (!r.has_value())
            return r;
        auto rest = skip_intertoken_space(r.value().rest);
        using V = decltype(r.value().value);
        return parse_result<V>{parse_state<V>{r.value().value, rest}};
    }};
}
// 07cc42ff-6ee4-4b1a-90b0-b81c99574723 end

} // namespace smd::smdscheme::parser

#endif
