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
#include <smd/kit/parser/parser.hpp>
#include <smd/kit/parser/parser_instances.hpp>

#include <utility>

namespace smd::cl::reader::detail {

// a7927622-983d-47f3-bf8e-33985ae5db64
/// Reads a token datum: scans the whole token, then classifies it
/// (DIV-0003, whole-token classification): a number by the decimal
/// grammar, else a keyword (leading unescaped colon), else a symbol —
/// interned either way (decision D12).
template <reader_context Ctx>
[[nodiscard]] constexpr auto read_token_datum(cursor cur, Ctx &ctx)
    -> foundation::result<parse_state<int>> {
    auto const where = cur.position();
    auto const classify_and_intern = [where](token_text const &token) {
        return smd::kit::parser::parser{
            [where, token](cursor rest, reader_context auto &rc)
                -> foundation::result<parse_state<int>> {
                auto const finish = [&](datum_atom atom) {
                    return bind(
                        add_leaf_checked(rc, std::move(atom), where),
                        [rest](int id) -> foundation::result<parse_state<int>> {
                            return parse_state<int>{id, rest};
                        });
                };
                if (!token.has_escape) {
                    auto const classified = classify_number(token.view(), 10);
                    switch (classified.cls) {
                    case number_class::fixnum:
                        return finish(
                            datum_atom{datum_fixnum{classified.value}});
                    case number_class::bignum:
                        return finish(datum_atom{
                            datum_tower{tower_kind::bignum, 10, token}});
                    case number_class::ratio:
                        return finish(datum_atom{
                            datum_tower{tower_kind::ratio, 10, token}});
                    case number_class::floating:
                        return finish(datum_atom{
                            datum_tower{tower_kind::floating, 10, token}});
                    case number_class::none:
                        break;
                    }
                }
                // A keyword interns under its colon-carrying spelling, so
                // :FOO and FOO are distinct entries until a package system
                // exists — see datum.hpp on @c datum_keyword.
                auto const view = token.view();
                bool const keyword =
                    !token.has_escape && view.size() > 1 && view.front() == ':';
                return bind(intern_checked(rc.symbols, view, where),
                            [&](symbol::symbol_id id)
                                -> foundation::result<parse_state<int>> {
                                return finish(
                                    keyword ? datum_atom{datum_keyword{id}}
                                            : datum_atom{datum_symbol{id}});
                            });
            }};
    };
    // DIV-0003, accepted-permanent, and the shape here is what enforces it
    // rather than merely agreeing with it. @ref token_p scans a *whole*
    // token — the maximal run of constituents, @c scan_token unmodified —
    // and only the continuation classifies what came back. Nothing in this
    // function may decide anything before the token ends: not a @c satisfy
    // over constituent characters, not a choice between a number parser
    // and a symbol parser, neither of which can see the token it is half
    // way through. The retired Scheme reader had a greedy-digit integer
    // parser and that is precisely the shape DIV-0003 records as wrong for
    // Common Lisp, where a symbol may start with a digit: it reads `1+` as
    // the fixnum 1 with a stray `+` left over, instead of the symbol `1+`.
    return bind(token_p, classify_and_intern)(cur, ctx);
}
// a7927622-983d-47f3-bf8e-33985ae5db64 end

} // namespace smd::cl::reader::detail

#endif
