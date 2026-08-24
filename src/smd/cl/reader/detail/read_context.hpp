// src/smd/cl/reader/detail/read_context.hpp                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_READER_DETAIL_READ_CONTEXT_HPP
#define SRC_SMD_CL_READER_DETAIL_READ_CONTEXT_HPP

#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/foundation/result_instances.hpp>
#include <smd/cl/foundation/source_pos.hpp>
#include <smd/cl/reader/datum.hpp>
#include <smd/cl/reader/readtable.hpp>
#include <smd/cl/symbol/symbol_table.hpp>

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
///
/// @c and_then is now a spelling of the Monad instance's @c bind rather
/// than its own implementation, which is why nothing in this header had to
/// change when the typeclass arrived. Keeping the domain name is what buys
/// that: the @c bind CPO is an object, so ADL would not have found it and
/// this using-declaration would have had to become load-bearing.
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

} // namespace detail

} // namespace smd::cl::reader

#endif
