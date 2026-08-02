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
#include <smd/cl/foundation/tagged_tree_schemes.hpp>
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
/// It runs as three passes, each a named scheme over the tree, and none
/// of them is a recursive descent. A @ref smd::cl::foundation::tagged_tree
/// stores its own base functor — columns keyed by node index: the nodes,
/// and the @ref smd::cl::foundation::tree_link giving each node its
/// parent and its position among that parent's children — and each pass
/// reads columns and produces one more, so the pipeline is nodes, then
/// plans, then emitted core indices. Construction is children-before-
/// parent, so index order is a topological order: ascending is bottom-up,
/// descending is top-down, and no scheme needs a stack, a visitor that
/// recurses, or a bound on nesting depth.
///
/// What makes all three folds rather than scatters is that every pass
/// writes only the row it is standing on. Reading another row is ordinary
/// random access and happens throughout — a parent's role, a child's
/// emitted index — but no pass reaches over and writes someone else's
/// entry. The link column is what buys that for the role pass; without a
/// way to find its parent, a node can only be told what it is by a parent
/// writing into it.
///
/// - **Atoms** (ascending, @c traverse over the result applicative, one
///   column in and one out). Row-wise: every datum atom becomes a
///   @ref smd::cl::core::core_leaf, shape preserved. This is where an
///   atom's *denotation* is decided and diagnosed: `NIL` and `T` become
///   constants, a numeric-tower literal becomes an error (decision D19
///   makes it readable but not yet executable). Because @c traverse visits
///   every leaf, an unreachable atom is diagnosed too, and an atom-level
///   error is reported ahead of any structural one.
/// - **Roles** (descending: @c scan_down, the inherited-attribute
///   scheme, total). Each node asks its parent what position it occupies
///   — an expression, quoted data, the head of a form, or unreached —
///   and the scheme hands over the parent's finished plan through the
///   link column. Reachability falls out: only the root starts
///   reachable. Descending order is what makes the parent's plan final
///   before any child reads it, since a parent's index always exceeds
///   its children's.
/// - **Emission** (ascending: @c para_short, the paramorphism over the
///   result effect). Each node's carrier is its emitted core index, and
///   each layer arrives with its child slots already holding the
///   children's — emission is an int-to-int algebra, tree indices in,
///   core indices out. It is a paramorphism and not a plain catamorphism
///   for one reason: a call's head is read as syntax, a name and not a
///   subexpression, so the algebra reaches back to the source child
///   list. And it is the short-circuiting form because the core tree is
///   a shared arena: past the first error, continuing would spend
///   capacity on nodes that are already discarded.
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

/// The role @p index plays, read from its parent instead of written by it.
///
/// The parent's finished plan arrives as @ref
/// smd::cl::foundation::scan_down hands it over — this is the inherited
/// half of an attribute grammar, and the scheme owns the ordering
/// argument: construction is children-before-parent, so the descending
/// scan has settled every parent before any child asks. What remains
/// here is only the question itself.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
role_of(lowered_tree<MaxNodes, MaxList> const &tree, int index,
        node_plan const *parent) -> node_role {
    if (index == tree.root()) {
        return node_role::evaluate;
    }
    if (parent == nullptr) {
        // Built but named by no branch, and not the root: unreached.
        return node_role::unreachable;
    }
    auto const link = tree.link(index);
    auto const parent_plan = *parent;
    switch (parent_plan.role) {
    case node_role::unreachable:
    case node_role::operator_name:
        // Nothing below an unreached node; and a compound in head position
        // is diagnosed rather than descended into.
        return node_role::unreachable;
    case node_role::quote_datum:
        return node_role::quote_datum;
    case node_role::evaluate:
        break;
    }
    auto const &parent_branch = tree.branch(link.parent);
    if (parent_branch.tag == reader::datum_branch::quote) {
        return node_role::quote_datum;
    }
    if (parent_branch.tag != reader::datum_branch::list) {
        // `#(...)`, `#'f` and the backquote family have no evaluated
        // subforms yet: the emission pass reports, and reporting needs no
        // planned child.
        return node_role::unreachable;
    }
    if (link.ordinal == 0) {
        return node_role::operator_name;
    }
    return parent_plan.form == form_kind::quotation ? node_role::quote_datum
                                                    : node_role::evaluate;
}

/// Which compound form @p index is, given the @p role it plays.
///
/// Row-wise, unlike @ref role_of: a node's form is decided by the node and
/// its own children's head symbol, never by its parent. Only a node in an
/// expression position has one; everything else keeps the default, which
/// nothing reads.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
form_of(lowered_tree<MaxNodes, MaxList> const &tree,
        operator_ids const &operators, int index, node_role role) -> form_kind {
    if (role != node_role::evaluate || tree.is_leaf(index)) {
        return form_kind::call;
    }
    auto const &branch = tree.branch(index);
    if (branch.tag == reader::datum_branch::quote) {
        return form_kind::quotation;
    }
    if (branch.tag != reader::datum_branch::list || branch.children.empty()) {
        // `()` is the constant NIL, and the rest are diagnosed on emission.
        return form_kind::call;
    }
    return classify_head(head_symbol(tree, branch.children), operators);
}

// dc3ee031-e752-4420-a3e8-610face05838
/// Assigns every node its plan: one @ref smd::cl::foundation::scan_down
/// through the link column, the inherited-attribute scheme applied to
/// the attribute it exists for.
///
/// Each step writes one entry — its own — and reads its parent's, so
/// this is a fold over the plan column rather than a scatter into it.
/// The difference is the tree's link column: without it a node cannot
/// find its parent, so the only way to propagate downward is for each
/// parent to write its children's entries.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
plan_nodes(lowered_tree<MaxNodes, MaxList> const &tree,
           operator_ids const &operators)
    -> foundation::static_vector<node_plan, MaxNodes> {
    return foundation::scan_down<node_plan>(
        tree, [&operators](lowered_tree<MaxNodes, MaxList> const &within,
                           int index, node_plan const *parent) {
            auto const role = role_of(within, index, parent);
            return node_plan{role, form_of(within, operators, index, role)};
        });
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
    /// One source node: the emission fold's element, paired with its plan.
    using source_node = typename source_tree::node_type;
    /// A source leaf's payload.
    using source_leaf = typename source_tree::leaf_type;
    /// A source branch.
    using source_branch = typename source_tree::branch_type;
    /// The core tree being built.
    using out_tree = core::core_tree<MaxNodes, MaxChildren>;
    /// A source branch's child-index list.
    using source_children = typename source_tree::child_list;
    /// A core branch's child-index list.
    using out_children = typename out_tree::child_list;
    /// One materialized layer: the source node with its child slots
    /// holding the children's already-emitted core indices (-1 where a
    /// child's role emits nothing). This is @c para_short's gift to the
    /// algebra — the same @c node_f template as @c source_node, at the
    /// same `R = int`, but the ints now index the core tree being built
    /// instead of the source tree being read. Emission is an int-to-int
    /// algebra: tree indices in, core indices out.
    using layer = foundation::node_f<core::core_leaf, reader::datum_branch, int,
                                     DatumList>;

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

/// Collects the already-emitted core indices among @p children — the
/// materialized child slots — past the first @p skip of them.
template <class OutChildren, class EmittedChildren>
[[nodiscard]] constexpr auto collect_emitted(EmittedChildren const &children,
                                             int skip)
    -> foundation::result<OutChildren> {
    return foundation::fold_left_short(
        children | std::views::drop(skip), OutChildren{},
        [](OutChildren collected,
           int child) -> foundation::result<OutChildren> {
            if (collected.size() >= collected.capacity()) {
                return foundation::parse_error{{},
                                               "too many subforms in one form"};
            }
            collected.push_back(child);
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
emit_quoted_list(Ctx &ctx, typename Ctx::source_children const &elements)
    -> foundation::result<int> {
    return foundation::and_then(
        add_leaf_checked(ctx, core::core_leaf{core::core_nil{}}),
        [&ctx, &elements](int tail) {
            return foundation::fold_left_short(
                elements | std::views::reverse, tail,
                [&ctx](int cdr, int element) {
                    return make_pair(ctx, element, cdr);
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
emit_quoted_operator(Ctx &ctx, typename Ctx::source_children const &children,
                     std::string_view name) -> foundation::result<int> {
    return foundation::and_then(
        intern_checked(ctx.symbols, name),
        [&ctx, &children](symbol::symbol_id operator_id) {
            return foundation::and_then(
                add_leaf_checked(ctx, core::core_leaf{core::core_quoted_symbol{
                                          operator_id}}),
                [&ctx, &children](int head) {
                    return foundation::and_then(
                        add_leaf_checked(ctx,
                                         core::core_leaf{core::core_nil{}}),
                        [&ctx, &children, head](int tail) {
                            return foundation::and_then(
                                make_pair(ctx, children[0], tail),
                                [&ctx, head](int rest) {
                                    return make_pair(ctx, head, rest);
                                });
                        });
                });
        });
}

template <class Ctx>
[[nodiscard]] constexpr auto emit_quoted(Ctx &ctx,
                                         typename Ctx::layer const &node)
    -> foundation::result<int> {
    if (auto const *payload = std::get_if<typename Ctx::source_leaf>(&node)) {
        return add_leaf_checked(ctx, quoted_image(*payload));
    }
    auto const &branch = std::get<typename Ctx::source_branch>(node);
    switch (branch.tag) {
    case reader::datum_branch::list:
        return emit_quoted_list(ctx, branch.children);
    case reader::datum_branch::quote:
        return emit_quoted_operator(ctx, branch.children, "QUOTE");
    case reader::datum_branch::function:
        return emit_quoted_operator(ctx, branch.children, "FUNCTION");
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
emit_conditional(Ctx &ctx, typename Ctx::source_children const &children)
    -> foundation::result<int> {
    if (children.size() < 3 || children.size() > 4) {
        return foundation::parse_error{
            {}, "if takes a test, a then form, and an optional else form"};
    }
    return foundation::and_then(
        collect_emitted<typename Ctx::out_children>(children, 1),
        [&ctx](typename Ctx::out_children arms) {
            return add_branch_checked(ctx, core::core_tag{core::core_if{}},
                                      std::move(arms));
        });
}

/// Emits `(progn form...)`; the empty sequence is `NIL`.
template <class Ctx>
[[nodiscard]] constexpr auto
emit_sequence(Ctx &ctx, typename Ctx::source_children const &children)
    -> foundation::result<int> {
    if (children.size() == 1) {
        return add_leaf_checked(ctx, core::core_leaf{core::core_nil{}});
    }
    return foundation::and_then(
        collect_emitted<typename Ctx::out_children>(children, 1),
        [&ctx](typename Ctx::out_children body) {
            return add_branch_checked(ctx, core::core_tag{core::core_progn{}},
                                      std::move(body));
        });
}

/// Emits an ordinary call. The callee is a symbol resolved in the function
/// namespace, so it travels in the tag rather than as a child (Lisp-2).
template <class Ctx>
[[nodiscard]] constexpr auto
emit_call(Ctx &ctx, typename Ctx::source_children const &source_children,
          typename Ctx::source_children const &children)
    -> foundation::result<int> {
    // The callee is read as syntax — a name, not a subexpression — so it
    // comes from the source child list, not the materialized one. This is
    // the read that makes emission a paramorphism rather than a plain
    // catamorphism.
    auto const callee = head_symbol(ctx.source, source_children);
    if (!callee.valid()) {
        return foundation::parse_error{
            {}, "the head of a form must name a function"};
    }
    return foundation::and_then(
        collect_emitted<typename Ctx::out_children>(children, 1),
        [&ctx, callee](typename Ctx::out_children arguments) {
            return add_branch_checked(ctx,
                                      core::core_tag{core::core_call{callee}},
                                      std::move(arguments));
        });
}

/// Emits a parenthesized form in an expression position.
template <class Ctx>
[[nodiscard]] constexpr auto
emit_compound(Ctx &ctx, node_plan const &plan,
              typename Ctx::source_children const &source_children,
              typename Ctx::source_children const &children)
    -> foundation::result<int> {
    if (children.empty()) {
        return add_leaf_checked(ctx, core::core_leaf{core::core_nil{}});
    }
    switch (plan.form) {
    case form_kind::quotation:
        if (children.size() != 2) {
            return foundation::parse_error{{},
                                           "quote takes exactly one argument"};
        }
        // The quoted datum is the whole of the form's core; `quote` itself
        // emits nothing (decision D18: no node kind for what a node kind
        // would only pass through).
        return children[1];
    case form_kind::conditional:
        return emit_conditional(ctx, children);
    case form_kind::sequence:
        return emit_sequence(ctx, children);
    case form_kind::call:
        return emit_call(ctx, source_children, children);
    }
    return foundation::parse_error{{}, "unsupported form"};
}

template <class Ctx>
[[nodiscard]] constexpr auto
emit_evaluated(Ctx &ctx, typename Ctx::source_tree const &tree, int index,
               typename Ctx::layer const &node, node_plan const &plan)
    -> foundation::result<int> {
    if (auto const *payload = std::get_if<typename Ctx::source_leaf>(&node)) {
        return add_leaf_checked(ctx, *payload);
    }
    auto const &branch = std::get<typename Ctx::source_branch>(node);
    switch (branch.tag) {
    case reader::datum_branch::list:
        return emit_compound(ctx, plan, tree.branch(index).children,
                             branch.children);
    case reader::datum_branch::quote:
        return branch.children[0];
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

/// The emission algebra, one node per application: emits the core for
/// the materialized layer @p node, which plays the role its plan gave
/// it, and returns where it landed — that return value is what fills
/// the next layer's child slot.
///
/// Every arm returns exactly one carrier value, including the two that
/// emit nothing (-1), because one-value-per-node is what keeps a child
/// slot meaning "my child's core index" and nothing else. The paramorph
/// carries the column; the algebra never sees it.
template <class Ctx>
[[nodiscard]] constexpr auto
emit_node(Ctx &ctx, typename Ctx::source_tree const &tree, int index,
          typename Ctx::layer const &node) -> foundation::result<int> {
    switch (ctx.plans[index].role) {
    case node_role::unreachable:
    case node_role::operator_name:
        return -1;
    case node_role::evaluate:
        return emit_evaluated(ctx, tree, index, node, ctx.plans[index]);
    case node_role::quote_datum:
        return emit_quoted(ctx, node);
    }
    return -1;
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
            // The paramorphism: bottom-up over the source, each node's
            // carrier its emitted core index, each layer arriving with
            // its child slots already emitted. Its return value is the
            // root's index, which is the one fact the caller needs.
            return foundation::and_then(
                foundation::para_short<int>(
                    lowered,
                    [&ctx](typename context::source_tree const &tree, int index,
                           typename context::layer const &layer) {
                        return detail::emit_node(ctx, tree, index, layer);
                    }),
                [&out](int root) -> foundation::result<out_tree> {
                    out.set_root(root);
                    return out;
                });
        });
}
// 53eabd7c-d0c2-4f0e-a63b-3260a0151044 end

} // namespace smd::cl::elaborator

#endif
