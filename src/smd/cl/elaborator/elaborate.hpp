// src/smd/cl/elaborator/elaborate.hpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_ELABORATOR_ELABORATE_HPP
#define SRC_SMD_CL_ELABORATOR_ELABORATE_HPP

#include <smd/cl/core/ast.hpp>
#include <smd/cl/foundation/fold_left_short.hpp>
#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/foundation/result_instances.hpp>
#include <smd/cl/foundation/static_vector.hpp>
#include <smd/cl/foundation/tagged_tree.hpp>
#include <smd/cl/foundation/tagged_tree_instances.hpp>
#include <smd/cl/foundation/traversable.hpp>
#include <smd/cl/reader/datum.hpp>
#include <smd/cl/symbol/symbol_id.hpp>

#include <algorithm>
#include <ranges>
#include <string_view>
#include <utility>
#include <variant>

/// The elaborator: lowers a @ref smd::cl::reader::datum_tree, which is what
/// the source said, to a @ref smd::cl::core::core_tree, which is what the
/// program means.
///
/// The reader classifies no special operators; this is where a list stops
/// being a list and becomes a conditional, a sequence, a literal, or a call.
///
/// It runs as three passes over the datum tree's node array, and none of
/// them is a recursive descent. A @ref smd::cl::foundation::tagged_tree is
/// built children-before-parent, so its node indices are a topological
/// order: ascending index order is a bottom-up catamorphism and descending
/// index order is a top-down propagation. Both are folds — over the tree's
/// own node sequence where a pass needs only the nodes, over an index range
/// where it also has to address a parallel table by node index — which is
/// why no pass needs a stack, a visitor that recurses, or a bound on
/// nesting depth.
///
/// - **Atoms** (ascending, @c traverse over the result applicative). Every
///   datum atom becomes a @ref smd::cl::core::core_leaf, shape preserved.
///   This is where an atom's *denotation* is decided and diagnosed: `NIL`
///   and `T` become constants, a numeric-tower literal becomes an error
///   (decision D19 makes it readable but not yet executable). Because
///   @c traverse visits every leaf, an unreachable atom is diagnosed too,
///   and an atom-level error is reported ahead of any structural one.
/// - **Roles** (descending, total). Each node learns what position it
///   occupies — an expression, quoted data, the head of a form, or
///   unreached — from its parent. Reachability falls out: only the root
///   starts reachable.
/// - **Emission** (ascending, @c fold_left_short over the result effect).
///   Each node emits its core, reading its children's already-emitted core
///   indices out of the fold's accumulator. This is the pass that must stop
///   at the first error rather than visit everything: the core tree is a
///   shared arena, and continuing past a failure would spend its capacity
///   on nodes that are already discarded.
///
/// The atom pass reads a symbol as a variable reference, which is the
/// value-position reading; the role pass is what decides that a given
/// occurrence is a function name or a piece of data instead. That is not a
/// correction — it is the Lisp-2 rule itself. An atom names a symbol; only
/// its position picks a namespace.
namespace smd::cl::elaborator {

/// Default core-tree node capacity for the convenience entry point.
inline constexpr int default_max_nodes = 256;

/// Default per-node subexpression capacity for the convenience entry point.
inline constexpr int default_max_children = 32;

namespace detail {

// -------------------------------------------------------------------
// Pass one: atoms
// -------------------------------------------------------------------

/// The core leaf each datum atom denotes, read in value position.
///
/// A visitor over one node's alternatives, which is the use of
/// @c std::visit the rules permit: it dispatches an atom, it does not drive
/// the recursion.
// 475cde6b-1633-44fe-96c7-02c60f4c9fd7
struct atom_lowering {
    symbol::symbol_id nil_id{}; ///< `NIL`, if the table has interned it.
    symbol::symbol_id t_id{};   ///< `T`, if the table has interned it.

    constexpr auto operator()(reader::datum_fixnum atom) const
        -> foundation::result<core::core_leaf> {
        return core::core_leaf{core::core_fixnum{atom.value}};
    }

    constexpr auto operator()(reader::datum_symbol atom) const
        -> foundation::result<core::core_leaf> {
        if (atom.id == nil_id) {
            return core::core_leaf{core::core_nil{}};
        }
        if (atom.id == t_id) {
            return core::core_leaf{core::core_t{}};
        }
        return core::core_leaf{core::core_variable{atom.id}};
    }

    constexpr auto operator()(reader::datum_keyword atom) const
        -> foundation::result<core::core_leaf> {
        return core::core_leaf{core::core_keyword{atom.id}};
    }

    constexpr auto operator()(reader::datum_character atom) const
        -> foundation::result<core::core_leaf> {
        return core::core_leaf{core::core_character{atom.value}};
    }

    constexpr auto operator()(reader::datum_string const &atom) const
        -> foundation::result<core::core_leaf> {
        if (atom.length > core::max_string_chars) {
            return foundation::parse_error{{}, "string literal too long"};
        }
        core::core_string literal{};
        literal.length = atom.length;
        std::ranges::copy(atom.view(), literal.storage.begin());
        return core::core_leaf{literal};
    }

    constexpr auto operator()(reader::datum_tower const &) const
        -> foundation::result<core::core_leaf> {
        // Decision D19: the tower's syntax lands in the reader ahead of its
        // semantics, so this is "not yet", not "not a number".
        return foundation::parse_error{
            {}, "numeric tower literal is not yet executable"};
    }
};
// 475cde6b-1633-44fe-96c7-02c60f4c9fd7 end

/// The datum tree with its atoms lowered: same shape, same branch tags,
/// @ref core::core_leaf leaves.
template <int MaxNodes, int MaxList>
using lowered_tree =
    foundation::tagged_tree<core::core_leaf, reader::datum_branch, MaxNodes,
                            MaxList>;

/// Returns the id @p name is interned under, or the invalid id if the table
/// has never seen it.
///
/// An operator this returns the invalid id for cannot occur in the form:
/// the reader interns every symbol it reads, so a name absent from the
/// table is a name absent from the source. Recognition therefore needs no
/// interning, and comparison is by id (decision D12), never by string.
template <class SymbolTable>
[[nodiscard]] constexpr auto find_or_invalid(SymbolTable const &symbols,
                                             std::string_view name)
    -> symbol::symbol_id {
    auto const found = symbols.find(name);
    return found ? *found : symbol::symbol_id{};
}

/// Interns @p name, surfacing a full symbol table or name pool as a @ref
/// foundation::parse_error rather than tripping the table's asserts.
template <class SymbolTable>
[[nodiscard]] constexpr auto intern_checked(SymbolTable &symbols,
                                            std::string_view name)
    -> foundation::result<symbol::symbol_id> {
    if (auto const existing = symbols.find(name)) {
        return *existing;
    }
    if (symbols.size() >= symbols.capacity()) {
        return foundation::parse_error{{}, "symbol table full"};
    }
    if (symbols.name_chars_used() + static_cast<int>(name.size()) >
        symbols.name_chars_capacity()) {
        return foundation::parse_error{{}, "symbol name storage full"};
    }
    return symbols.intern(name);
}

/// Lowers every atom of @p form, leftmost bad atom winning.
template <int MaxNodes, int MaxList, class SymbolTable>
[[nodiscard]] constexpr auto
lower_atoms(reader::datum_tree<MaxNodes, MaxList> const &form,
            SymbolTable const &symbols)
    -> foundation::result<lowered_tree<MaxNodes, MaxList>> {
    atom_lowering const denotation{find_or_invalid(symbols, "NIL"),
                                   find_or_invalid(symbols, "T")};
    return foundation::traverse(
        [&denotation](reader::datum_atom const &atom) {
            return std::visit(denotation, atom);
        },
        form);
}

// -------------------------------------------------------------------
// Pass two: roles
// -------------------------------------------------------------------

/// What position a datum node occupies in the form being elaborated.
enum class node_role : unsigned char {
    unreachable,  ///< Not part of the program; emits nothing.
    evaluate,     ///< An expression position.
    quote_datum,  ///< Literal data.
    operator_name ///< The head of a compound form, consumed by its parent.
};

/// Which form a compound datum in @ref node_role::evaluate denotes.
enum class form_kind : unsigned char {
    call,        ///< An ordinary function call.
    conditional, ///< `if`.
    sequence,    ///< `progn`.
    quotation    ///< `quote`.
};

/// What one pass has decided about one node, for the next pass to use.
struct node_plan {
    node_role role = node_role::unreachable; ///< Its position.
    form_kind form = form_kind::call;        ///< Meaningful when compound.

    // HIDDEN FRIEND
    friend constexpr auto operator==(node_plan, node_plan) -> bool = default;
};

/// The special operators this elaborator recognizes, resolved to ids once
/// per elaboration so that recognition is id comparison (decision D12).
struct operator_ids {
    symbol::symbol_id quote{};       ///< `QUOTE`.
    symbol::symbol_id conditional{}; ///< `IF`.
    symbol::symbol_id sequence{};    ///< `PROGN`.
};

/// Resolves the recognized special operators against @p symbols.
template <class SymbolTable>
[[nodiscard]] constexpr auto resolve_operators(SymbolTable const &symbols)
    -> operator_ids {
    return operator_ids{find_or_invalid(symbols, "QUOTE"),
                        find_or_invalid(symbols, "IF"),
                        find_or_invalid(symbols, "PROGN")};
}

/// Returns the symbol at the head of @p children, or the invalid id when
/// the head is not a symbol — a nested form, or `NIL` or `T`, both of which
/// the atom pass has already turned into constants.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
head_symbol(lowered_tree<MaxNodes, MaxList> const &tree,
            foundation::static_vector<int, MaxList> const &children)
    -> symbol::symbol_id {
    if (children.empty() || !tree.is_leaf(children[0])) {
        return symbol::symbol_id{};
    }
    auto const *named =
        std::get_if<core::core_variable>(&tree.leaf(children[0]));
    return named ? named->id : symbol::symbol_id{};
}

/// Classifies a compound form by the symbol at its head.
[[nodiscard]] constexpr auto classify_head(symbol::symbol_id head,
                                           operator_ids const &operators)
    -> form_kind {
    if (!head.valid()) {
        return form_kind::call;
    }
    if (head == operators.quote) {
        return form_kind::quotation;
    }
    if (head == operators.conditional) {
        return form_kind::conditional;
    }
    if (head == operators.sequence) {
        return form_kind::sequence;
    }
    return form_kind::call;
}

/// Propagates @p index's own role to its children, and records which form
/// it is if it is a compound one.
template <int MaxNodes, int MaxList>
constexpr auto
plan_children(lowered_tree<MaxNodes, MaxList> const &tree,
              operator_ids const &operators,
              foundation::static_vector<node_plan, MaxNodes> &plans, int index)
    -> void {
    if (tree.is_leaf(index)) {
        return;
    }
    auto const &branch = tree.branch(index);
    auto const give = [&plans](int child, node_role role) {
        plans[child].role = role;
    };
    switch (plans[index].role) {
    case node_role::unreachable:
    case node_role::operator_name:
        // Nothing below an unreached node; and a compound in head position
        // is diagnosed rather than descended into.
        return;
    case node_role::quote_datum:
        std::ranges::for_each(branch.children, [&give](int child) {
            give(child, node_role::quote_datum);
        });
        return;
    case node_role::evaluate:
        break;
    }
    if (branch.tag == reader::datum_branch::quote) {
        plans[index].form = form_kind::quotation;
        std::ranges::for_each(branch.children, [&give](int child) {
            give(child, node_role::quote_datum);
        });
        return;
    }
    if (branch.tag != reader::datum_branch::list || branch.children.empty()) {
        // `#(...)`, `#'f` and the backquote family have no evaluated
        // subforms yet, and `()` is the constant NIL: the emission pass
        // reports or constructs, and neither needs a planned child.
        return;
    }
    plans[index].form =
        classify_head(head_symbol(tree, branch.children), operators);
    give(branch.children[0], node_role::operator_name);
    auto const argument_role = plans[index].form == form_kind::quotation
                                   ? node_role::quote_datum
                                   : node_role::evaluate;
    std::ranges::for_each(
        branch.children | std::views::drop(1),
        [&give, argument_role](int child) { give(child, argument_role); });
}

// dc3ee031-e752-4420-a3e8-610face05838
/// Assigns every node its role, top down in one descending pass.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
plan_nodes(lowered_tree<MaxNodes, MaxList> const &tree,
           operator_ids const &operators)
    -> foundation::static_vector<node_plan, MaxNodes> {
    auto plans = foundation::static_vector<node_plan, MaxNodes>::filled(
        tree.size(), node_plan{});
    if (tree.root() < 0) {
        return plans;
    }
    plans[tree.root()].role = node_role::evaluate;
    // Every parent's index exceeds its children's, so descending index
    // order visits each node after whichever node gave it its role.
    std::ranges::for_each(
        std::views::iota(0, tree.size()) | std::views::reverse,
        [&](int index) { plan_children(tree, operators, plans, index); });
    return plans;
}
// dc3ee031-e752-4420-a3e8-610face05838 end

// -------------------------------------------------------------------
// Pass three: emission
// -------------------------------------------------------------------

/// Everything one elaboration's emission pass shares: the planned source,
/// the core arena being filled, and the symbol table that quoted
/// quote-family data is materialized into.
template <int MaxNodes, int MaxChildren, int DatumNodes, int DatumList,
          class SymbolTable>
struct emit_context {
    /// The atom-lowered source tree.
    using source_tree = lowered_tree<DatumNodes, DatumList>;
    /// The core tree being built.
    using out_tree = core::core_tree<MaxNodes, MaxChildren>;
    /// A source branch's child-index list.
    using source_children = typename source_tree::child_list;
    /// A core branch's child-index list.
    using out_children = typename out_tree::child_list;
    /// Source node index to emitted core node index, -1 where nothing was
    /// emitted. This is the emission fold's accumulator.
    using index_map = foundation::static_vector<int, DatumNodes>;

    source_tree const &source; ///< What is being lowered.
    foundation::static_vector<node_plan, DatumNodes> const &plans; ///< Roles.
    SymbolTable &symbols; ///< Receives quoted `QUOTE` and `FUNCTION`.
    out_tree &out;        ///< Receives every core node emitted.
};

/// Appends a core leaf, surfacing a full tree as a @ref
/// foundation::parse_error rather than tripping the capacity assert.
template <class Ctx>
[[nodiscard]] constexpr auto add_leaf_checked(Ctx &ctx, core::core_leaf leaf)
    -> foundation::result<int> {
    if (ctx.out.size() >= ctx.out.capacity()) {
        return foundation::parse_error{{}, "core tree full"};
    }
    return ctx.out.add_leaf(std::move(leaf));
}

/// Appends a core branch, surfacing a full tree as a @ref
/// foundation::parse_error.
template <class Ctx>
[[nodiscard]] constexpr auto
add_branch_checked(Ctx &ctx, core::core_tag tag,
                   typename Ctx::out_children children)
    -> foundation::result<int> {
    if (ctx.out.size() >= ctx.out.capacity()) {
        return foundation::parse_error{{}, "core tree full"};
    }
    return ctx.out.add_branch(tag, std::move(children));
}

/// Emits one hermetic pair.
template <class Ctx>
[[nodiscard]] constexpr auto make_pair(Ctx &ctx, int car, int cdr)
    -> foundation::result<int> {
    typename Ctx::out_children pair;
    pair.push_back(car);
    pair.push_back(cdr);
    return add_branch_checked(ctx, core::core_tag{core::core_cons{}},
                              std::move(pair));
}

/// Collects the already-emitted core indices of @p children past the first
/// @p skip of them.
template <class OutChildren, class IndexMap, class SourceChildren>
[[nodiscard]] constexpr auto
collect_emitted(IndexMap const &map, SourceChildren const &children, int skip)
    -> foundation::result<OutChildren> {
    return foundation::fold_left_short(
        children | std::views::drop(skip), OutChildren{},
        [&map](OutChildren collected,
               int child) -> foundation::result<OutChildren> {
            if (collected.size() >= collected.capacity()) {
                return foundation::parse_error{{},
                                               "too many subforms in one form"};
            }
            collected.push_back(map[child]);
            return collected;
        });
}

/// The leaf a quoted atom denotes: a symbol becomes data rather than a
/// variable reference, and every other atom already denotes itself.
[[nodiscard]] constexpr auto quoted_image(core::core_leaf const &leaf)
    -> core::core_leaf {
    if (auto const *named = std::get_if<core::core_variable>(&leaf)) {
        return core::core_leaf{core::core_quoted_symbol{named->id}};
    }
    return leaf;
}

// f1cd911b-1555-4d82-a97f-2044cd35cbf4
/// Emits a proper list of @p elements as hermetic pairs, right to left from
/// a fresh `NIL`. An empty element list is that `NIL`.
template <class Ctx>
[[nodiscard]] constexpr auto
emit_quoted_list(Ctx &ctx, typename Ctx::index_map const &map,
                 typename Ctx::source_children const &elements)
    -> foundation::result<int> {
    return foundation::and_then(
        add_leaf_checked(ctx, core::core_leaf{core::core_nil{}}),
        [&ctx, &map, &elements](int tail) {
            return foundation::fold_left_short(
                elements | std::views::reverse, tail,
                [&ctx, &map](int cdr, int element) {
                    return make_pair(ctx, map[element], cdr);
                });
        });
}
// f1cd911b-1555-4d82-a97f-2044cd35cbf4 end

/// Emits `'x` or `#'x` appearing inside quoted data as the two-element list
/// it denotes.
///
/// ANSI specifies both representations — `'x` is `(quote x)` (2.4.6) and
/// `#'x` is `(function x)` (2.4.8.2) — so materializing them is reading the
/// standard, not inventing a convention. The operator has to be interned
/// here because the reader never spelled it: `'x` is a branch tag, not a
/// token, so nothing put `QUOTE` in the table.
template <class Ctx>
[[nodiscard]] constexpr auto
emit_quoted_operator(Ctx &ctx, typename Ctx::index_map const &map,
                     typename Ctx::source_children const &children,
                     std::string_view name) -> foundation::result<int> {
    return foundation::and_then(
        intern_checked(ctx.symbols, name),
        [&ctx, &map, &children](symbol::symbol_id operator_id) {
            return foundation::and_then(
                add_leaf_checked(ctx, core::core_leaf{core::core_quoted_symbol{
                                          operator_id}}),
                [&ctx, &map, &children](int head) {
                    return foundation::and_then(
                        add_leaf_checked(ctx,
                                         core::core_leaf{core::core_nil{}}),
                        [&ctx, &map, &children, head](int tail) {
                            return foundation::and_then(
                                make_pair(ctx, map[children[0]], tail),
                                [&ctx, head](int rest) {
                                    return make_pair(ctx, head, rest);
                                });
                        });
                });
        });
}

template <class Ctx>
[[nodiscard]] constexpr auto emit_quoted(Ctx &ctx,
                                         typename Ctx::index_map const &map,
                                         int index) -> foundation::result<int> {
    if (ctx.source.is_leaf(index)) {
        return add_leaf_checked(ctx, quoted_image(ctx.source.leaf(index)));
    }
    auto const &branch = ctx.source.branch(index);
    switch (branch.tag) {
    case reader::datum_branch::list:
        return emit_quoted_list(ctx, map, branch.children);
    case reader::datum_branch::quote:
        return emit_quoted_operator(ctx, map, branch.children, "QUOTE");
    case reader::datum_branch::function:
        return emit_quoted_operator(ctx, map, branch.children, "FUNCTION");
    case reader::datum_branch::vector:
        return foundation::parse_error{
            {}, "vector literals are not yet supported as data"};
    case reader::datum_branch::backquote:
    case reader::datum_branch::unquote:
    case reader::datum_branch::unquote_splice:
        // The representation of a backquote form is implementation-defined
        // (ANSI 2.4.6.1), so it is the macroexpander's to choose, not this
        // pass's to guess.
        return foundation::parse_error{
            {}, "backquote is not yet supported as data"};
    }
    return foundation::parse_error{{}, "unsupported quoted datum"};
}

/// Emits `(if test then)` or `(if test then else)`.
template <class Ctx>
[[nodiscard]] constexpr auto
emit_conditional(Ctx &ctx, typename Ctx::index_map const &map,
                 typename Ctx::source_children const &children)
    -> foundation::result<int> {
    if (children.size() < 3 || children.size() > 4) {
        return foundation::parse_error{
            {}, "if takes a test, a then form, and an optional else form"};
    }
    return foundation::and_then(
        collect_emitted<typename Ctx::out_children>(map, children, 1),
        [&ctx](typename Ctx::out_children arms) {
            return add_branch_checked(ctx, core::core_tag{core::core_if{}},
                                      std::move(arms));
        });
}

/// Emits `(progn form...)`; the empty sequence is `NIL`.
template <class Ctx>
[[nodiscard]] constexpr auto
emit_sequence(Ctx &ctx, typename Ctx::index_map const &map,
              typename Ctx::source_children const &children)
    -> foundation::result<int> {
    if (children.size() == 1) {
        return add_leaf_checked(ctx, core::core_leaf{core::core_nil{}});
    }
    return foundation::and_then(
        collect_emitted<typename Ctx::out_children>(map, children, 1),
        [&ctx](typename Ctx::out_children body) {
            return add_branch_checked(ctx, core::core_tag{core::core_progn{}},
                                      std::move(body));
        });
}

/// Emits an ordinary call. The callee is a symbol resolved in the function
/// namespace, so it travels in the tag rather than as a child (Lisp-2).
template <class Ctx>
[[nodiscard]] constexpr auto
emit_call(Ctx &ctx, typename Ctx::index_map const &map,
          typename Ctx::source_children const &children)
    -> foundation::result<int> {
    auto const callee = head_symbol(ctx.source, children);
    if (!callee.valid()) {
        return foundation::parse_error{
            {}, "the head of a form must name a function"};
    }
    return foundation::and_then(
        collect_emitted<typename Ctx::out_children>(map, children, 1),
        [&ctx, callee](typename Ctx::out_children arguments) {
            return add_branch_checked(ctx,
                                      core::core_tag{core::core_call{callee}},
                                      std::move(arguments));
        });
}

/// Emits a parenthesized form in an expression position.
template <class Ctx>
[[nodiscard]] constexpr auto
emit_compound(Ctx &ctx, typename Ctx::index_map const &map, int index,
              typename Ctx::source_children const &children)
    -> foundation::result<int> {
    if (children.empty()) {
        return add_leaf_checked(ctx, core::core_leaf{core::core_nil{}});
    }
    switch (ctx.plans[index].form) {
    case form_kind::quotation:
        if (children.size() != 2) {
            return foundation::parse_error{{},
                                           "quote takes exactly one argument"};
        }
        // The quoted datum is the whole of the form's core; `quote` itself
        // emits nothing (decision D18: no node kind for what a node kind
        // would only pass through).
        return map[children[1]];
    case form_kind::conditional:
        return emit_conditional(ctx, map, children);
    case form_kind::sequence:
        return emit_sequence(ctx, map, children);
    case form_kind::call:
        return emit_call(ctx, map, children);
    }
    return foundation::parse_error{{}, "unsupported form"};
}

template <class Ctx>
[[nodiscard]] constexpr auto
emit_evaluated(Ctx &ctx, typename Ctx::index_map const &map, int index)
    -> foundation::result<int> {
    if (ctx.source.is_leaf(index)) {
        return add_leaf_checked(ctx, ctx.source.leaf(index));
    }
    auto const &branch = ctx.source.branch(index);
    switch (branch.tag) {
    case reader::datum_branch::list:
        return emit_compound(ctx, map, index, branch.children);
    case reader::datum_branch::quote:
        return map[branch.children[0]];
    case reader::datum_branch::vector:
        return foundation::parse_error{
            {}, "vector literals are not yet executable"};
    case reader::datum_branch::function:
        return foundation::parse_error{{}, "#'f is not yet executable"};
    case reader::datum_branch::backquote:
    case reader::datum_branch::unquote:
    case reader::datum_branch::unquote_splice:
        return foundation::parse_error{{}, "backquote is not yet executable"};
    }
    return foundation::parse_error{{}, "unsupported form"};
}

/// One step of the emission fold: emits the core for @p index and records
/// where it landed.
template <class Ctx>
[[nodiscard]] constexpr auto emit_node(Ctx &ctx, typename Ctx::index_map map,
                                       int index)
    -> foundation::result<typename Ctx::index_map> {
    using index_map = typename Ctx::index_map;
    auto record =
        [map, index](int emitted) mutable -> foundation::result<index_map> {
        map[index] = emitted;
        return map;
    };
    switch (ctx.plans[index].role) {
    case node_role::unreachable:
    case node_role::operator_name:
        return map;
    case node_role::evaluate:
        return foundation::and_then(emit_evaluated(ctx, map, index), record);
    case node_role::quote_datum:
        return foundation::and_then(emit_quoted(ctx, map, index), record);
    }
    return map;
}

/// A map in which nothing has been emitted yet.
template <class IndexMap>
[[nodiscard]] constexpr auto unmapped(int count) -> IndexMap {
    return IndexMap::filled(count, -1);
}

} // namespace detail

/// Elaborates one datum @p form into the core tree it means, interning into
/// @p symbols the quote-family operators that quoted data needs and the
/// reader never spelled.
///
/// Recognizes `quote` (spelled either way), `if` and `progn`; every other
/// compound form in an expression position is a call, with the callee
/// resolved in the function namespace (Lisp-2). Self-evaluating atoms are
/// fixnums, characters, strings, keywords, `NIL` and `T`; a numeric-tower
/// literal is readable but not yet executable (decision D19), as are
/// vectors, `#'f` and the backquote family.
///
/// @tparam MaxNodes    Core-tree node capacity.
/// @tparam MaxChildren Maximum subexpressions per core node.
/// @tparam DatumNodes  The form's node capacity (deduced).
/// @tparam DatumList   The form's per-list capacity (deduced).
/// @tparam SymbolTable The symbol table instantiation; needs @c find,
///                     @c intern, and the capacity observers of
///                     @c symbol::symbol_table.
/// @param  form    The datum to lower; must have a root.
/// @param  symbols The table @p form's names were interned into.
/// @return The core tree, or the leftmost failure.
// 53eabd7c-d0c2-4f0e-a63b-3260a0151044
template <int MaxNodes = default_max_nodes,
          int MaxChildren = default_max_children, int DatumNodes, int DatumList,
          class SymbolTable>
[[nodiscard]] constexpr auto
elaborate(reader::datum_tree<DatumNodes, DatumList> const &form,
          SymbolTable &symbols)
    -> foundation::result<core::core_tree<MaxNodes, MaxChildren>> {
    static_assert(MaxChildren >= 3,
                  "a core node must hold an if's three arms and a pair's two "
                  "halves");
    using out_tree = core::core_tree<MaxNodes, MaxChildren>;
    using context = detail::emit_context<MaxNodes, MaxChildren, DatumNodes,
                                         DatumList, SymbolTable>;
    using index_map = typename context::index_map;
    return foundation::and_then(
        detail::lower_atoms(form, symbols),
        [&symbols](detail::lowered_tree<DatumNodes, DatumList> const &lowered)
            -> foundation::result<out_tree> {
            if (lowered.root() < 0) {
                return foundation::parse_error{{}, "form has no root"};
            }
            auto const plans =
                detail::plan_nodes(lowered, detail::resolve_operators(symbols));
            out_tree out;
            context ctx{lowered, plans, symbols, out};
            return foundation::and_then(
                foundation::fold_left_short(
                    std::views::iota(0, lowered.size()),
                    detail::unmapped<index_map>(lowered.size()),
                    [&ctx](index_map map, int index) {
                        return detail::emit_node(ctx, std::move(map), index);
                    }),
                [&out, root = lowered.root()](
                    index_map const &emitted) -> foundation::result<out_tree> {
                    out.set_root(emitted[root]);
                    return out;
                });
        });
}
// 53eabd7c-d0c2-4f0e-a63b-3260a0151044 end

} // namespace smd::cl::elaborator

#endif
