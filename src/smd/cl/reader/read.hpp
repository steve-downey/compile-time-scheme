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
