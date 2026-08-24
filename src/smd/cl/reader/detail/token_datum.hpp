// src/smd/cl/reader/detail/token_datum.hpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_READER_DETAIL_TOKEN_DATUM_HPP
#define SRC_SMD_CL_READER_DETAIL_TOKEN_DATUM_HPP

#include <smd/cl/foundation/result.hpp>
#include <smd/cl/foundation/source_pos.hpp>
#include <smd/cl/reader/cursor.hpp>
#include <smd/cl/reader/datum.hpp>
#include <smd/cl/reader/detail/read_context.hpp>
#include <smd/cl/reader/number.hpp>
#include <smd/cl/reader/token.hpp>
#include <smd/cl/symbol/symbol_id.hpp>

#include <utility>

namespace smd::cl::reader::detail {

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

} // namespace smd::cl::reader::detail

#endif
