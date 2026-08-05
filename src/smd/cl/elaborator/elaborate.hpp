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
#include <smd/cl/symbol/symbol_table.hpp>

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
/// index order is a top-down propagation. Both are folds over
/// @c std::views::iota, which is why no pass needs a stack, a visitor that
/// recurses, or a bound on nesting depth.
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

/// Interning that diagnoses a full table rather than asserting.
///
/// It lived here until the evaluator wanted a third copy of it; it is now
/// @ref symbol::intern_checked. The calls below would find it by
/// argument-dependent lookup anyway — a table is a @c symbol type — so this
/// declaration is here to say where it went, and it is also why the two
/// definitions could not have coexisted.
using symbol::intern_checked;

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
    quotation,   ///< `quote`.
    definition,  ///< `defun`.
    named_block, ///< `block`.
    block_exit,  ///< `return-from`.
    protection   ///< `unwind-protect`.
};

// 20b081ea-5806-495c-8476-d89fee98acdc
/// How a form reads the children after its head: how many of them are
/// *names* rather than subforms, and what every later child is.
///
/// This is the whole of what the new special operators add to the role pass.
/// `defun` reads two names — the function's, and its lambda list — and
/// `block` and `return-from` read one; a name is left @ref
/// node_role::unreachable so that no pass treats it as an expression, and
/// the emission pass reads it straight out of the source tree. Nothing here
/// bounds nesting or needs a stack, because the pass is still one descending
/// sweep of the node array.
struct child_roles {
    int names = 0; ///< Children after the head that are names, not subforms.
    node_role rest =
        node_role::evaluate; ///< What every child past the names is.

    // HIDDEN FRIEND
    friend constexpr auto operator==(child_roles, child_roles)
        -> bool = default;
};

/// Returns how @p form reads its children.
[[nodiscard]] constexpr auto roles_of(form_kind form) -> child_roles {
    switch (form) {
    case form_kind::quotation:
        return child_roles{0, node_role::quote_datum};
    case form_kind::definition:
        return child_roles{2, node_role::evaluate};
    case form_kind::named_block:
    case form_kind::block_exit:
        return child_roles{1, node_role::evaluate};
    case form_kind::call:
    case form_kind::conditional:
    case form_kind::sequence:
    case form_kind::protection:
        break;
    }
    return child_roles{0, node_role::evaluate};
}
// 20b081ea-5806-495c-8476-d89fee98acdc end

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
    symbol::symbol_id definition{};  ///< `DEFUN`.
    symbol::symbol_id named_block{}; ///< `BLOCK`.
    symbol::symbol_id block_exit{};  ///< `RETURN-FROM`.
    symbol::symbol_id protection{};  ///< `UNWIND-PROTECT`.
};

/// Resolves the recognized special operators against @p symbols.
template <class SymbolTable>
[[nodiscard]] constexpr auto resolve_operators(SymbolTable const &symbols)
    -> operator_ids {
    return operator_ids{find_or_invalid(symbols, "QUOTE"),
                        find_or_invalid(symbols, "IF"),
                        find_or_invalid(symbols, "PROGN"),
                        find_or_invalid(symbols, "DEFUN"),
                        find_or_invalid(symbols, "BLOCK"),
                        find_or_invalid(symbols, "RETURN-FROM"),
                        find_or_invalid(symbols, "UNWIND-PROTECT")};
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
    if (head == operators.definition) {
        return form_kind::definition;
    }
    if (head == operators.named_block) {
        return form_kind::named_block;
    }
    if (head == operators.block_exit) {
        return form_kind::block_exit;
    }
    if (head == operators.protection) {
        return form_kind::protection;
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
    // A name stays unreachable — the default — so nothing downstream treats
    // it as an expression; the emission pass reads it from the source tree.
    auto const shape = roles_of(plans[index].form);
    std::ranges::for_each(
        branch.children | std::views::drop(1 + shape.names),
        [&give, shape](int child) { give(child, shape.rest); });
}

// dc3ee031-e752-4420-a3e8-610face05838
/// Assigns every node its role, top down in one descending pass.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
plan_nodes(lowered_tree<MaxNodes, MaxList> const &tree,
           operator_ids const &operators)
    -> foundation::static_vector<node_plan, MaxNodes> {
    foundation::static_vector<node_plan, MaxNodes> plans;
    std::ranges::for_each(std::views::iota(0, tree.size()),
                          [&plans](int) { plans.push_back(node_plan{}); });
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

/// Returns the symbol the source leaf at @p index names, or the invalid id
/// if that node is not a symbol.
///
/// The atom pass read every symbol in value position as a
/// @ref core::core_variable, so this is where a *name* — a block's, a
/// function's, a parameter's — is recovered. `NIL` and `T` became constants
/// there and so are not names here, which is why `(block nil ...)` is
/// diagnosed rather than accepted.
template <class Ctx>
[[nodiscard]] constexpr auto name_at(Ctx const &ctx, int index)
    -> symbol::symbol_id {
    if (!ctx.source.is_leaf(index)) {
        return symbol::symbol_id{};
    }
    auto const *named =
        std::get_if<core::core_variable>(&ctx.source.leaf(index));
    return named ? named->id : symbol::symbol_id{};
}

/// A lambda list's parameter names.
using parameter_list =
    foundation::static_vector<symbol::symbol_id, core::max_lambda_params>;

/// Reads the lambda list at @p index as parameter names.
///
/// Required-parameters only: a lambda list keyword is a symbol like any
/// other, so it would otherwise be bound as an ordinary parameter, which is
/// the kind of silence decision D19 rules out.
template <class Ctx>
[[nodiscard]] constexpr auto read_lambda_list(Ctx const &ctx, int index)
    -> foundation::result<parameter_list> {
    if (ctx.source.is_leaf(index) ||
        ctx.source.branch(index).tag != reader::datum_branch::list) {
        return foundation::parse_error{
            {}, "a lambda list must be a list of parameter names"};
    }
    return foundation::fold_left_short(
        ctx.source.branch(index).children, parameter_list{},
        [&ctx](parameter_list params,
               int child) -> foundation::result<parameter_list> {
            auto const name = name_at(ctx, child);
            if (!name.valid()) {
                return foundation::parse_error{{},
                                               "a parameter must be a symbol"};
            }
            if (ctx.symbols.name(name).starts_with('&')) {
                return foundation::parse_error{
                    {}, "lambda list keywords are not yet supported"};
            }
            if (params.size() >= params.capacity()) {
                return foundation::parse_error{{}, "too many parameters"};
            }
            params.push_back(name);
            return params;
        });
}

/// Emits a branch with exactly one child.
template <class Ctx>
[[nodiscard]] constexpr auto emit_wrapper(Ctx &ctx, core::core_tag tag,
                                          int child)
    -> foundation::result<int> {
    typename Ctx::out_children one;
    one.push_back(child);
    return add_branch_checked(ctx, std::move(tag), std::move(one));
}

/// Emits `(block name form...)`.
template <class Ctx>
[[nodiscard]] constexpr auto
emit_named_block(Ctx &ctx, typename Ctx::index_map const &map,
                 typename Ctx::source_children const &children, int skip,
                 symbol::symbol_id name) -> foundation::result<int> {
    return foundation::and_then(
        collect_emitted<typename Ctx::out_children>(map, children, skip),
        [&ctx, name](typename Ctx::out_children body) {
            return add_branch_checked(
                ctx, core::core_tag{core::core_block{name}}, std::move(body));
        });
}

// d398d3f2-8862-4663-9e03-28262372feae
/// Emits `(defun name (parameter...) form...)`.
///
/// Three nodes, and the nesting is the whole of what `defun` means: the body
/// goes inside a @ref core::core_block named for the function, which is the
/// implicit block ANSI 3.1.2.1.2.2 requires and which is what makes
/// `return-from` usable in a function body without a `block` of its own; the
/// block goes inside a @ref core::core_lambda, which binds the parameters;
/// and the lambda goes inside a @ref core::core_defun, which puts it in the
/// symbol's function slot.
///
/// Nothing here captures the function under its own name, and that is the
/// point. The recursive call in the body is an ordinary
/// @ref core::core_call, which the machine resolves through the function
/// slot at the moment of the call — so a body that names itself finds the
/// definition being installed. That is DIV-0009 closed.
template <class Ctx>
[[nodiscard]] constexpr auto
emit_definition(Ctx &ctx, typename Ctx::index_map const &map,
                typename Ctx::source_children const &children)
    -> foundation::result<int> {
    if (children.size() < 3) {
        return foundation::parse_error{
            {}, "defun takes a name, a lambda list, and a body"};
    }
    auto const name = name_at(ctx, children[1]);
    if (!name.valid()) {
        return foundation::parse_error{{}, "a function name must be a symbol"};
    }
    return foundation::and_then(
        read_lambda_list(ctx, children[2]),
        [&ctx, &map, &children, name](parameter_list const &params) {
            return foundation::and_then(
                emit_named_block(ctx, map, children, 3, name),
                [&ctx, &params, name](int body) {
                    return foundation::and_then(
                        emit_wrapper(ctx,
                                     core::core_tag{core::core_lambda{params}},
                                     body),
                        [&ctx, name](int function) {
                            return emit_wrapper(
                                ctx, core::core_tag{core::core_defun{name}},
                                function);
                        });
                });
        });
}
// d398d3f2-8862-4663-9e03-28262372feae end

/// Emits `(block name form...)`, the source form.
template <class Ctx>
[[nodiscard]] constexpr auto
emit_block(Ctx &ctx, typename Ctx::index_map const &map,
           typename Ctx::source_children const &children)
    -> foundation::result<int> {
    if (children.size() < 2) {
        return foundation::parse_error{{}, "block takes a name and a body"};
    }
    auto const name = name_at(ctx, children[1]);
    if (!name.valid()) {
        return foundation::parse_error{{}, "a block name must be a symbol"};
    }
    return emit_named_block(ctx, map, children, 2, name);
}

/// Emits `(return-from name)` or `(return-from name value)`.
template <class Ctx>
[[nodiscard]] constexpr auto
emit_block_exit(Ctx &ctx, typename Ctx::index_map const &map,
                typename Ctx::source_children const &children)
    -> foundation::result<int> {
    if (children.size() < 2 || children.size() > 3) {
        return foundation::parse_error{
            {}, "return-from takes a block name and an optional value"};
    }
    auto const name = name_at(ctx, children[1]);
    if (!name.valid()) {
        return foundation::parse_error{{}, "a block name must be a symbol"};
    }
    return foundation::and_then(
        collect_emitted<typename Ctx::out_children>(map, children, 2),
        [&ctx, name](typename Ctx::out_children carried) {
            return add_branch_checked(
                ctx, core::core_tag{core::core_return_from{name}},
                std::move(carried));
        });
}

/// Emits `(unwind-protect protected cleanup...)`.
template <class Ctx>
[[nodiscard]] constexpr auto
emit_protection(Ctx &ctx, typename Ctx::index_map const &map,
                typename Ctx::source_children const &children)
    -> foundation::result<int> {
    if (children.size() < 2) {
        // ANSI's grammar is `unwind-protect protected-form cleanup-form*`:
        // no cleanup at all is legal, no protected form is not.
        return foundation::parse_error{
            {}, "unwind-protect takes a protected form and cleanup forms"};
    }
    return foundation::and_then(
        collect_emitted<typename Ctx::out_children>(map, children, 1),
        [&ctx](typename Ctx::out_children forms) {
            return add_branch_checked(
                ctx, core::core_tag{core::core_unwind_protect{}},
                std::move(forms));
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
    case form_kind::definition:
        return emit_definition(ctx, map, children);
    case form_kind::named_block:
        return emit_block(ctx, map, children);
    case form_kind::block_exit:
        return emit_block_exit(ctx, map, children);
    case form_kind::protection:
        return emit_protection(ctx, map, children);
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
    IndexMap map;
    std::ranges::for_each(std::views::iota(0, count),
                          [&map](int) { map.push_back(-1); });
    return map;
}

} // namespace detail

/// Elaborates one datum @p form into @p out, which may already hold earlier
/// forms, and returns the index of the node that is this form's root.
///
/// The root is *returned* rather than set, because a program is a sequence
/// of top-level forms sharing one arena: whoever reads them decides what
/// joins them, and a closure made by an earlier form must still be able to
/// name a node an equally long-lived tree owns. @ref elaborate is this with
/// one form and the root set.
///
/// @tparam MaxNodes    Core-tree node capacity (deduced from @p out).
/// @tparam MaxChildren Maximum subexpressions per core node (deduced).
/// @tparam DatumNodes  The form's node capacity (deduced).
/// @tparam DatumList   The form's per-list capacity (deduced).
/// @tparam SymbolTable The symbol table instantiation; needs @c find,
///                     @c intern, and the capacity observers of
///                     @c symbol::symbol_table.
/// @param  form    The datum to lower; must have a root.
/// @param  symbols The table @p form's names were interned into.
/// @param  out     The arena to emit into.
/// @return The emitted form's root index in @p out, or the leftmost failure.
// 53eabd7c-d0c2-4f0e-a63b-3260a0151044
template <int MaxNodes, int MaxChildren, int DatumNodes, int DatumList,
          class SymbolTable>
[[nodiscard]] constexpr auto
elaborate_into(reader::datum_tree<DatumNodes, DatumList> const &form,
               SymbolTable &symbols,
               core::core_tree<MaxNodes, MaxChildren> &out)
    -> foundation::result<int> {
    static_assert(MaxChildren >= 3,
                  "a core node must hold an if's three arms and a pair's two "
                  "halves");
    using context = detail::emit_context<MaxNodes, MaxChildren, DatumNodes,
                                         DatumList, SymbolTable>;
    using index_map = typename context::index_map;
    return foundation::and_then(
        detail::lower_atoms(form, symbols),
        [&symbols,
         &out](detail::lowered_tree<DatumNodes, DatumList> const &lowered)
            -> foundation::result<int> {
            if (lowered.root() < 0) {
                return foundation::parse_error{{}, "form has no root"};
            }
            auto const plans =
                detail::plan_nodes(lowered, detail::resolve_operators(symbols));
            context ctx{lowered, plans, symbols, out};
            return foundation::and_then(
                foundation::fold_left_short(
                    std::views::iota(0, lowered.size()),
                    detail::unmapped<index_map>(lowered.size()),
                    [&ctx](index_map map, int index) {
                        return detail::emit_node(ctx, std::move(map), index);
                    }),
                [root = lowered.root()](index_map const &emitted)
                    -> foundation::result<int> { return emitted[root]; });
        });
}

/// Elaborates one datum @p form into the core tree it means, interning into
/// @p symbols the quote-family operators that quoted data needs and the
/// reader never spelled.
///
/// Recognizes `quote` (spelled either way), `if`, `progn`, `defun`, `block`,
/// `return-from` and `unwind-protect`; every other compound form in an
/// expression position is a call, with the callee resolved in the function
/// namespace (Lisp-2). Self-evaluating atoms are fixnums, characters,
/// strings, keywords, `NIL` and `T`; a numeric-tower literal is readable but
/// not yet executable (decision D19), as are vectors, `#'f` and the
/// backquote family.
///
/// @tparam MaxNodes    Core-tree node capacity.
/// @tparam MaxChildren Maximum subexpressions per core node.
/// @tparam DatumNodes  The form's node capacity (deduced).
/// @tparam DatumList   The form's per-list capacity (deduced).
/// @tparam SymbolTable The symbol table instantiation.
/// @param  form    The datum to lower; must have a root.
/// @param  symbols The table @p form's names were interned into.
/// @return The core tree, or the leftmost failure.
template <int MaxNodes = default_max_nodes,
          int MaxChildren = default_max_children, int DatumNodes, int DatumList,
          class SymbolTable>
[[nodiscard]] constexpr auto
elaborate(reader::datum_tree<DatumNodes, DatumList> const &form,
          SymbolTable &symbols)
    -> foundation::result<core::core_tree<MaxNodes, MaxChildren>> {
    using out_tree = core::core_tree<MaxNodes, MaxChildren>;
    out_tree out;
    return foundation::and_then(
        elaborate_into(form, symbols, out),
        [&out](int root) -> foundation::result<out_tree> {
            out.set_root(root);
            return out;
        });
}
// 53eabd7c-d0c2-4f0e-a63b-3260a0151044 end

} // namespace smd::cl::elaborator

#endif
