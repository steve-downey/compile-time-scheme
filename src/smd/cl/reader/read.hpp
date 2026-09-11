// src/smd/cl/reader/read.hpp                                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_READER_READ_HPP
#define SRC_SMD_CL_READER_READ_HPP

// e6e0bb21-34e0-44cb-92ef-79f8580a1581
// The Ctx-templated reader machinery, split (step B1) into these eight
// headers under detail/. clang-format sorts this list alphabetically
// rather than by dependency, which is fine: each header is self-contained
// and pulls in whatever it needs, so repeated inclusion through several
// paths is a no-op under the usual include guards. The one dependency
// worth naming is read_node_fwd.hpp, the forward declaration that lets
// read_wrapped, read_delimited and read_sharpsign (in forms.hpp,
// sharpsign.hpp) call read_node before node.hpp defines it -- every
// function on both sides of that cycle is a template, so a declaration
// suffices at the call site and the definition need only be visible at
// the point of instantiation, which this umbrella guarantees by including
// all eight.
#include <smd/cl/reader/detail/forms.hpp>
#include <smd/cl/reader/detail/node.hpp>
#include <smd/cl/reader/detail/read_context.hpp>
#include <smd/cl/reader/detail/read_node_fwd.hpp>
#include <smd/cl/reader/detail/sharpsign.hpp>
#include <smd/cl/reader/detail/skip.hpp>
#include <smd/cl/reader/detail/text.hpp>
#include <smd/cl/reader/detail/token_datum.hpp>
// e6e0bb21-34e0-44cb-92ef-79f8580a1581 end

#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/reader/cursor.hpp>
#include <smd/cl/reader/datum.hpp>
#include <smd/cl/reader/readtable.hpp>
#include <smd/cl/symbol/symbol_table.hpp>
#include <smd/kit/foundation/monad.hpp>
#include <smd/kit/parser/parser.hpp>
#include <smd/kit/parser/parser_instances.hpp>

#include <string_view>

namespace smd::cl::reader {

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
    using tree_type = datum_tree<MaxNodes, MaxList>;
    tree_type tree;
    detail::read_context<SymbolTable, MaxNodes, MaxList> ctx{tree, symbols,
                                                             table};
    // Read one node, then set it as the root: two steps, the second taking
    // the first's output. That is a bind, and after step B7 there is no
    // reader function left that spells one by hand.
    auto const node_p =
        smd::kit::parser::parser{[](cursor c, detail::reader_context auto &rc) {
            return detail::read_node(c, rc);
        }};
    auto const set_root = [&tree](int node) {
        return smd::kit::parser::parser{
            [&tree, node](cursor rest, detail::reader_context auto &)
                -> foundation::result<parse_state<tree_type>> {
                tree.set_root(node);
                return parse_state<tree_type>{tree, rest};
            }};
    };
    return smd::kit::foundation::bind(node_p, set_root)(cur, ctx);
}

/// Reads exactly one datum from @p source: convenience over @ref
/// read_datum for callers with a whole spelling in hand. Anything but
/// intertoken space after the datum is an error.
template <int MaxNodes = default_max_nodes, int MaxList = default_max_list,
          class SymbolTable>
[[nodiscard]] constexpr auto read(std::string_view source, SymbolTable &symbols,
                                  readtable const &table = standard_readtable)
    -> foundation::result<datum_tree<MaxNodes, MaxList>> {
    using tree_type = datum_tree<MaxNodes, MaxList>;
    // The readtable is this parse's whole context. @ref read_datum builds
    // its own reader context per call, and there is none in hand here --
    // the same situation @ref detail::skip_intertoken_space is in, and the
    // same answer: `readtable const` models parse_context directly (B3).
    auto const datum_p = smd::kit::parser::parser{
        [&symbols](cursor c, readtable const &rt)
            -> foundation::result<parse_state<tree_type>> {
            return read_datum<MaxNodes, MaxList>(c, symbols, rt);
        }};
    auto const require_end = [](tree_type const &tree) {
        return smd::kit::parser::parser{
            [tree](cursor rest, readtable const &rt)
                -> foundation::result<parse_state<tree_type>> {
                cursor const after = detail::skip_intertoken_space(rest, rt);
                if (!after.empty()) {
                    return foundation::parse_error{after.position(),
                                                   "unexpected trailing input"};
                }
                return parse_state<tree_type>{tree, after};
            }};
    };
    // The last bind is @c result's own: dropping the cursor a caller with
    // a whole spelling in hand has no use for is not a parse step, which
    // is the same reason the tree appends throughout detail/ never moved
    // onto the parser layer either.
    return smd::kit::foundation::bind(
        smd::kit::foundation::bind(datum_p, require_end)(cursor{source}, table),
        [](parse_state<tree_type> const &state)
            -> foundation::result<tree_type> { return state.value; });
}

} // namespace smd::cl::reader

#endif
