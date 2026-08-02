**DRAFT &#x2014; pending author revision**

<div class="abstract" id="org509fe8e">
<p>
Step R3 built the rebuild's reader, and it reads considerably more Common Lisp than the rebuild can evaluate.
Decision D19 says reader syntax leads semantics, so strings, characters, <code>#(...)</code> vectors and every numeric tower spelling land at once, and a literal with no machine representation yet is carried as its spelling.
Every character decision is a table lookup, which is the claim that <code>set-macro-character</code> later widens an enum instead of forcing a rewrite.
The datum tree and the core AST are two instantiations of one new container, so Foldable and Traversable are written once, against the container, before either tree exists.
The container is the fixed point of its own base functor, a <code>static_assert</code> says so, and none of the recursion schemes over it recurses.
And the frozen <code>smdlisp</code> reader is the oracle for everything it covers, except in the three places where it is wrong.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 25 - Interning Symbols, and Seven Divergences That Were One ←](phase-25-symbols.md)

</nav>


# Everything the reader decides, it looks up

Phase 16's reader answered a readtable's worth of questions without having a readtable. The character classifications were functions, and the dispatch was a switch on `'`, `` ` ``, `,` and `#` written out by hand. That is how you write it when you don't know `set-macro-character` is coming. D19 says the aim is all of ANSI Common Lisp, and the readtable is exactly the part of the standard where the reader stops being a fixed program and becomes one the user can edit while it runs (Steele, Guy L., 1990).

So R3 puts the table in first. `readtable.hpp` is three parallel arrays over the 7-bit character range: a `syntax_type` per ANSI 2.1.4, a `macro_kind` naming which reader macro a character dispatches to, and a `sharpsign_kind` for the sub-dispatch after `#`. The standard readtable is built by a `consteval` function, which is the honest signature for table generation; it means nothing at run time. The reader's entry into all of it is one switch:

```cpp
/// Reads one datum node: skips intertoken space, then dispatches on the
/// current character through the readtable (decision D19: this lookup —
/// not character tests — is the reader's spine).
template <class Ctx>
[[nodiscard]] constexpr auto read_node(cursor cur, Ctx &ctx)
    -> foundation::result<parse_state<int>> {
    cur = skip_intertoken_space(cur, ctx.table);
    if (cur.empty()) {
        return foundation::parse_error{cur.position(),
                                       "unexpected end of input"};
    }
    auto const where = cur.position();
    switch (ctx.table.macro_of(cur.peek())) {
    case macro_kind::none:
        return read_token_datum(cur, ctx);
    case macro_kind::left_paren:
        return read_delimited(cur.bump(), ctx, datum_branch::list, where);
    case macro_kind::right_paren:
        return foundation::parse_error{where, "unexpected ')'"};
    case macro_kind::single_quote:
        return read_wrapped(cur.bump(), ctx, datum_branch::quote, where);
    case macro_kind::backquote:
        return read_wrapped(cur.bump(), ctx, datum_branch::backquote, where);
    case macro_kind::comma: {
        cursor const after = cur.bump();
        if (!after.empty() && after.peek() == '@') {
            return read_wrapped(after.bump(), ctx, datum_branch::unquote_splice,
                                where);
        }
        return read_wrapped(after, ctx, datum_branch::unquote, where);
    }
    case macro_kind::double_quote:
        return read_string(cur.bump(), ctx, where);
    case macro_kind::semicolon: // consumed as intertoken space
        break;
    case macro_kind::sharpsign:
        return read_sharpsign(cur, ctx);
    }
    return foundation::parse_error{where, "expected datum"};
}
```

`(` is never compared against `'('`. It's a table entry that says `left_paren`, and the table says so because `make_standard_readtable` put it there. The one bare character comparison left in the function is the `@` after a comma, and that one is correct: ANSI makes `,@` the comma macro reading its own next character, not a readtable entry of its own. The claim D19 is making is that `set-macro-character` then arrives as one more alternative in `macro_kind` carrying a user function, with the dispatch site unmoved. I can't check that claim yet, and won't be able to until `*readtable*` is a real variable that a program can bind. What I can check is that today's dispatch already reads a table it could be handed a different copy of.


# Syntax ahead of semantics

D19's other consequence is that an atom kind may be readable before it is executable, so full ANSI atom syntax lands now, ahead of the evaluation it needs:

```cpp
/// A datum-tree leaf: one atom.
using datum_atom = std::variant<datum_fixnum, datum_symbol, datum_keyword,
                                datum_character, datum_string, datum_tower>;

/// A datum-tree branch tag: what a compound datum is.
///
/// The quote family records source reality (@c 'x is not a @c (quote x)
/// list token) and defers all interpretation to later passes, carrying
/// over the old tree's split; @c vector is new with decision D19.
enum class datum_branch : unsigned char {
    list,          ///< @c (a b c); children are the elements.
    vector,        ///< @c #(a b c); children are the elements.
    quote,         ///< @c 'x; one child, the quoted datum.
    function,      ///< @c #'f; one child, the referenced datum.
    backquote,     ///< @c `template; one child, the template.
    unquote,       ///< @c ,x; one child, the unquoted datum.
    unquote_splice ///< @c ,@x; one child, the spliced datum.
};
```

Six atom kinds where the pivot had three, and `datum_tower` is the one that needs explaining. `1/2`, `1.5e3`, `#16rDEADBEEF` and an integer too wide for an `int` all read without complaint, and not one of them can be evaluated, because the rebuild has no ratio, float, or bignum type. The datum carries what the evaluator can't represent: which tower member the spelling denotes, the radix the digits are written in, and the token's own characters in its own storage. Fixnums are the exception, parsed to a value, because the fixnum lane exists. D19 calls the tower a fan-out, with each member's semantics arriving in its own lane gated on the evaluator, and this is the shape that makes those lanes possible without going back to the source text.

I want to be plain that nothing reads a `datum_tower` today. It's a spelling in a box with tests that it is the right spelling.

The pivot's hardest-won reader lesson carries over unchanged. A token is scanned to its end and only then classified, so `1+` is the symbol it is in ANSI CL and not the integer `1` followed by junk. That's DIV-0003, found by a failing `static_assert` back in step L5 and marked accepted-permanent. `number.hpp` restates the rule in its own header comment, and the oracle test below pins `1+` by name so it can't quietly come back.


# One container, two trees

The pivot's datum tree is built over an arena the caller has to keep alive, and only a datum's children live in that arena &#x2014; a root datum comes back by value, in a temporary that dies with the `read_datum` call. That is what the old tree's `core_symbol` documents: a `string_view` into a bare top-level symbol's name dangled the moment the read result went out of scope, AddressSanitizer caught it as a stack-use-after-return, and the fix was to copy the folded name into the core node. R3's replacement is a plain class that owns its own nodes, and it is still a fixpoint; the pivot's tree was one too, because every recursive type is. The difference is that this one is first order, materialized as columns keyed by node index, and this time the compiler checks the claim.

The layer comes first.

```cpp
/// One interior layer of a tagged tree: a tag value describing what the
/// branch is, and its children — whatever a child currently is.
///
/// @tparam Tag         The branch description type.
/// @tparam R           The recursive position: what occupies a child
///                     slot. `int` for a stored tree (a child is an
///                     index), an algebra's carrier mid-fold (a child is
///                     its already-computed result).
/// @tparam MaxChildren Maximum number of children per branch.
template <class Tag, class R, int MaxChildren>
struct branch_f {
    Tag tag{};                                ///< What this branch is.
    static_vector<R, MaxChildren> children{}; ///< The child slots.

    // HIDDEN FRIEND
    friend constexpr auto operator==(branch_f const &, branch_f const &)
        -> bool = default;
};

/// One layer of a tagged tree: a leaf, or a branch over child slots of
/// type @p R. This is the tree's base functor; the tree itself is its
/// fixed point at `R = int`, materialized as a column those ints index.
template <class Leaf, class Tag, class R, int MaxChildren>
using node_f = std::variant<Leaf, branch_f<Tag, R, MaxChildren>>;

/// Maps @p f over a layer's recursive positions — each child slot —
/// leaving the leaf alternative and a branch's tag untouched. This is
/// the base functor's fmap, and it is the whole interface a recursion
/// scheme needs: fold results into the child slots, then hand the layer
/// to an algebra that never chases an index.
template <class F, class Leaf, class Tag, class R, int MaxChildren>
[[nodiscard]] constexpr auto
fmap_children(F &&f, node_f<Leaf, Tag, R, MaxChildren> const &node) {
    using S = std::remove_cvref_t<std::invoke_result_t<F &, R const &>>;
    using out_node = node_f<Leaf, Tag, S, MaxChildren>;
    if (auto const *leaf = std::get_if<Leaf>(&node)) {
        return out_node{*leaf};
    }
    auto const &branch = std::get<branch_f<Tag, R, MaxChildren>>(node);
    branch_f<Tag, S, MaxChildren> mapped{branch.tag, {}};
    mapped.children.append_range(
        branch.children | std::views::transform([&f](R const &child) -> S {
            return std::invoke(f, child);
        }));
    return out_node{std::move(mapped)};
}
```

`node_f` is one layer of tree and no more: a leaf, or a tag over child slots, where a child is whatever `R` says it is. Nothing in the file recurses. `fmap_children` maps a function over the child slots and touches nothing else, and that small function is the entire interface the recursion schemes will need.

The tree ties the knot at `R = int`.

```cpp
template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
class tagged_tree {
  public:
    /// The leaf payload type, named for leaf-generic code.
    using leaf_type = Leaf;
    /// The branch description type.
    using tag_type = Tag;
    /// The interior-node type.
    using branch_type = tree_branch<Tag, MaxChildren>;
    /// One node: a leaf or a branch — one @ref node_f layer at `R = int`.
    /// The static_assert below the class is the witness that the tree is
    /// the base functor's fixed point, materialized as a column.
    using node_type = node_f<Leaf, Tag, int, MaxChildren>;
    /// A branch's child-index list.
    using child_list = static_vector<int, MaxChildren>;

    constexpr tagged_tree() = default;

    /// Builds a tree from a node sequence and a root index, in that order.
    ///
    /// This is the assembly half of every shape-preserving algorithm over a
    /// tree: map @ref nodes to a new node sequence, hand it back here with
    /// the same root, and the result has the same shape by construction —
    /// same node count, same indices, so the copied child lists still name
    /// the nodes they named before. Nothing has to walk an index range to
    /// arrange that.
    ///
    /// @pre the range's length is <= @p MaxNodes
    /// @pre @p root is -1 (no root) or an index into the range
    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_reference_t<R>,
                                     node_type>
    [[nodiscard]] static constexpr auto from_nodes(R &&node_range, int root)
        -> tagged_tree;

    /// Appends a leaf node and returns its index.
    /// @pre size() < capacity()
    constexpr auto add_leaf(Leaf leaf) -> int;

    /// Appends a branch node and returns its index.
    /// @pre size() < capacity()
    /// @pre every index in @p children names an existing node
    constexpr auto add_branch(Tag tag, child_list children) -> int;

    /// Marks the node at @p index as the root of the tree.
    /// @pre 0 <= index < size()
    constexpr auto set_root(int index) -> void;

    /// Returns the root node's index, or -1 if no root has been set.
    [[nodiscard]] constexpr auto root() const -> int;

    /// Returns the number of nodes.
    [[nodiscard]] constexpr auto size() const -> int;

    /// Returns @p MaxNodes, so builders can check for room before an
    /// @ref add_leaf or @ref add_branch that must not overflow.
    [[nodiscard]] constexpr auto capacity() const -> int;

    /// Returns @p MaxChildren, the per-branch child capacity.
    [[nodiscard]] constexpr auto max_children() const -> int;

    /// Returns the nodes in index order.
    ///
    /// This is the sequence an algorithm over a whole tree is written
    /// against, and the reason none of them needs an index range of its
    /// own: construction is children-before-parent, so index order is a
    /// topological order, and a walk in that order is an ascending walk.
    [[nodiscard]] constexpr auto nodes() const -> std::span<node_type const>;

    /// Returns the leaf payloads in index order, as a view over @ref nodes.
    ///
    /// This is the element sequence of the Foldable and Traversable
    /// instances in <smd/cl/foundation/tagged_tree_instances.hpp>: a fold
    /// sees leaves, because branches are structure and not elements. The
    /// documented traversal order is this view's order.
    [[nodiscard]] constexpr auto leaves() const;

    /// Returns the node at @p index.
    /// @pre 0 <= index < size()
    [[nodiscard]] constexpr auto node(int index) const -> node_type const &;

    /// Returns true if the node at @p index is a leaf.
    /// @pre 0 <= index < size()
    [[nodiscard]] constexpr auto is_leaf(int index) const -> bool;

    /// Returns the leaf payload at @p index.
    /// @pre is_leaf(index)
    [[nodiscard]] constexpr auto leaf(int index) const -> Leaf const &;

    /// Returns the branch at @p index.
    /// @pre !is_leaf(index)
    [[nodiscard]] constexpr auto branch(int index) const -> branch_type const &;

    /// Returns where the node at @p index sits beneath its parent.
    /// @pre 0 <= index < size()
    [[nodiscard]] constexpr auto link(int index) const -> tree_link const &;

    /// Returns the parent links in index order, parallel to @ref nodes —
    /// the column to zip against when a pass reads both.
    [[nodiscard]] constexpr auto links() const -> std::span<tree_link const>;

    // HIDDEN FRIEND
    friend constexpr auto operator==(tagged_tree const &, tagged_tree const &)
        -> bool = default;

  private:
    /// Points @p children at @p index, their parent. The only scatter left
    /// in the design, and it is the container's own: it happens once, when
    /// the edge is created, rather than once per pass that needs it.
    constexpr auto link_children(int index, child_list const &children) -> void;

    /// Recomputes every link from the nodes. Used where nodes arrive
    /// already assembled (@ref from_nodes) rather than an edge at a time.
    constexpr auto relink() -> void;

    static_vector<node_type, MaxNodes> nodes_{};
    static_vector<tree_link, MaxNodes> links_{};
    int root_{-1};
};
```

A tree is two columns keyed by node index. The node column holds one `node_f` layer per node, with child slots that are `int` indices back into the same column, so a tree is an ordinary value: copyable, comparable with the defaulted `operator==`, usable in constant evaluation, with no external arena to keep alive. Indices carry no capacity, which is D14's rule about capacity staying out of type identity; `MaxNodes` parameterises the tree because the tree is the storage. And the fixpoint is not a reading of the design; it's the `static_assert` just below the class, stating that `node_type` and `node_f` with `int` in the recursive position are the same type. The compiler checks that equation on every build, which is more than a comment ever did.

The link column is the transpose of the child lists: each node's parent index and its ordinal among that parent's children, the same edge relation read upward. `add_branch` writes it at the moment the edge is created (the one scatter left in the design, paid once at construction), so a descending pass can ask a node for its parent instead of needing every parent to write into its children.

The other interesting property is in `add_branch`'s precondition. A branch may only name nodes that already exist, so node index order is a topological order with every child before its parent, and a builder that adds children left to right leaves the leaves in left-to-right source order. That one precondition is the whole traversal-order contract:

```cpp
struct tagged_tree_traversable_impl {
    template <class F, class Leaf, class Tag, int MaxNodes, int MaxChildren>
    constexpr auto
    traverse(this auto &&, F &&f,
             tagged_tree<Leaf, Tag, MaxNodes, MaxChildren> const &tree) {
        using effect_type =
            std::remove_cvref_t<std::invoke_result_t<F &, Leaf const &>>;
        using B = typename effect_type::value_type;
        using out_tree = tagged_tree<B, Tag, MaxNodes, MaxChildren>;
        using out_node = typename out_tree::node_type;
        using in_node =
            typename tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::node_type;
        using out_nodes = static_vector<out_node, MaxNodes>;
        auto const &tc = applicative_typeclass<effect_type>;

        // Every node maps to an effect carrying its image: a leaf runs f,
        // a branch is carried by pure, which has no effect of its own. The
        // two arms are uniform, so the walk below is one fold and not a
        // conditional.
        auto const wrap = [](B mapped) { return out_node{std::move(mapped)}; };
        auto const node_effect = [&f, &tc, &wrap](in_node const &node) {
            if (auto const *payload = std::get_if<Leaf>(&node)) {
                return tc.invoke(wrap, std::invoke(f, *payload));
            }
            return tc.pure(
                out_node{std::get<tree_branch<Tag, MaxChildren>>(node)});
        };
        auto const append = [](out_nodes acc, out_node node) {
            acc.push_back(std::move(node));
            return acc;
        };
        // Sequencing the node effects left to right is a left fold in the
        // applicative — the accumulator is an effect, so effect order is
        // fold order and nothing else has to state it.
        auto sequenced = std::ranges::fold_left(
            tree.nodes(), tc.pure(out_nodes{}),
            [&](auto accumulated, in_node const &node) {
                return tc.invoke(append, std::move(accumulated),
                                 node_effect(node));
            });
        return tc.invoke(
            [root = tree.root()](out_nodes sequenced_nodes) {
                return out_tree::from_nodes(sequenced_nodes, root);
            },
            std::move(sequenced));
    }
};
```

D15 asks that the datum and core trees carry Foldable and Traversable with documented order as part of the instance contract, and that `traverse` over the result applicative **be** the elaborator's error propagation (McBride, Conor and Paterson, Ross, 2008). Because both trees are instantiations of `tagged_tree`, all of the instances (Functor, Foldable, Traversable) are written once against the container and law-tested once, and neither tree has one of its own. D15 is filed as a style rule and is really an architecture one, and this is why: in the pivot, a walk got written wherever a walk was needed, because arena indices plus a wide `std::visit` leave nothing else to write. Here the instances are ranges pipelines over the tree's own views: `fmap` is `from_nodes` over a `transform` of `nodes()`, the folds are `std::ranges::fold_left` and `fold_right` over `leaves()`, and `traverse` is a left fold in the applicative that reassembles the mapped node sequence through `from_nodes`. Which changes what shape preservation is. It used to be a property you read the code to believe; now a `transform` can not change a sequence's length, `from_nodes` takes the same root, and the mapped tree has the same indices naming the same places by construction.

`traverse` visits every leaf. Stopping early is the effect's business, and for `result` that means the leftmost error wins after the whole tree has been walked. When later work genuinely must be skipped, R1's `fold_left_short` is the other shape, and the header says which one you're getting.


# The schemes don't recurse

```cpp
/// Catamorphism over a @ref tagged_tree: folds the tree bottom-up with a
/// plain F-algebra, and returns the root's value.
///
/// The algebra receives one @ref node_f layer whose child slots hold the
/// children's already-computed results — it never sees an index, never
/// looks anything up, and cannot ask how deep it is. Construction is
/// children-before-parent, so ascending index order visits every child
/// first and the whole catamorphism is one linear pass: no stack, no
/// recursion, no bound on nesting depth.
///
/// @tparam A   The algebra's carrier (explicit).
/// @tparam Alg Callable: `A(node_f<Leaf, Tag, A, MaxChildren> const&)`.
/// @pre the tree has a root
template <class A, class Alg, class Leaf, class Tag, int MaxNodes,
          int MaxChildren>
    requires std::convertible_to<
        std::invoke_result_t<Alg &, node_f<Leaf, Tag, A, MaxChildren> const &>,
        A>
[[nodiscard]] constexpr auto
cata(tagged_tree<Leaf, Tag, MaxNodes, MaxChildren> const &tree, Alg alg) -> A {
    assert(tree.root() >= 0);
    auto const done = fold_up<A, MaxNodes>(
        tree.nodes(),
        [&alg](static_vector<A, MaxNodes> const &results,
               auto const &node) -> A {
            return std::invoke(alg, detail::materialize(results, node));
        });
    return done[tree.root()];
}
```

`cata` is R1's `fold_up` pointed at the node column, with `fmap_children` swapping each child index for that child's already-computed result before the algebra looks. The algebra receives one layer whose child slots hold results; it never sees an index, never looks anything up, and can't ask how deep it is. Children-before-parent construction means ascending index order visits every child first, so the whole catamorphism is one linear pass: no stack, no recursion, no bound on nesting depth. `para` is the same fold handing the algebra the tree and the node's own index besides, for the algebra that must read the node itself, which is what DIV-0016's driver needed. Both come in short-circuiting forms over `result`, and `scan_down` is `fold_down` aimed through the link column, one value per node from its parent's.

There is no Mendler variant, on purpose. Mendler's knob is control over when the recursive results get demanded, and over a stored column the children's results are computed by position before the parent is visited. The knob has nothing left to turn.

The law that says the knot is tied is reflection: `cata` with the algebra that rebuilds each layer gives back the tree it was handed. And a pinned test keeps the typeclass instances out of this header: on a `from_nodes` tree whose child lists run against index order, the order the structure dictates and the index order the instances promise disagree, so `fold_map` is not re-expressed through `cata`, and the test is there so nobody unifies them helpfully.

What the header does not contain is an anamorphism, and that's the reader's doing. A parse is an unfold: the reader builds, children first, exactly the column every fold above consumes. It is the only producer in the tree, and a generic with one caller is that caller written sideways; so the sentence stays prose, and the schemes stay folds. `read.hpp` chains its steps through a small `and_then` over `result`, which is the same rule D15 states (error propagation is not an `if (!r.has_value())` ladder) reaching the one pipeline stage where `traverse` can't be the answer.


# A core AST with nothing to build it

The other instantiation is the elaborated core, and at R3 it is very small.

```cpp
/// A VARIABLE-namespace reference: a symbol used as an expression,
/// resolved among variable bindings, never among function bindings
/// (Lisp-2). Carries the interned @ref symbol::symbol_id (decision D12),
/// so a compiled program never holds a view into reader storage — the
/// dangling-name failure the old tree's @c core_symbol documents cannot
/// arise.
struct core_variable {
    symbol::symbol_id id{}; ///< The referenced symbol.

    // HIDDEN FRIEND
    friend constexpr auto operator==(core_variable, core_variable)
        -> bool = default;
};

/// A self-evaluating keyword literal.
struct core_keyword {
    symbol::symbol_id id{}; ///< The interned keyword.

    // HIDDEN FRIEND
    friend constexpr auto operator==(core_keyword, core_keyword)
        -> bool = default;
};

/// The self-evaluating `nil` constant: the sole false value and the empty
/// list. Distinct from @ref core_variable so a bare `NIL` elaborates to a
/// constant, not a variable lookup.
struct core_nil {
    // HIDDEN FRIEND
    friend constexpr auto operator==(core_nil, core_nil) -> bool = default;
};

/// The canonical `t` (true) constant, distinct from @ref core_variable
/// for the same reason as @ref core_nil.
struct core_t {
    // HIDDEN FRIEND
    friend constexpr auto operator==(core_t, core_t) -> bool = default;
};

/// A core-tree leaf: one expression that has no subexpressions.
using core_leaf =
    std::variant<core_fixnum, core_variable, core_keyword, core_nil, core_t>;
```

Five leaves. `core_variable` is a VARIABLE-namespace reference carrying an interned `symbol_id` from R2 (decision D12), so a compiled program never holds a view into reader storage and the old tree's dangling-name failure has nowhere to happen. `nil` and `t` are constants in their own right, which is Phase 17's argument arriving as two empty structs.

```cpp
/// A FUNCTION-namespace call (Lisp-2): the callee is a symbol resolved in
/// the function namespace, not a subexpression, so it lives in the tag;
/// the children are the argument expressions, left to right.
struct core_call {
    symbol::symbol_id callee{}; ///< The called function's symbol.

    // HIDDEN FRIEND
    friend constexpr auto operator==(core_call, core_call) -> bool = default;
};

/// A two- or three-armed conditional; children are test, then, and
/// (optionally) else.
struct core_if {
    // HIDDEN FRIEND
    friend constexpr auto operator==(core_if, core_if) -> bool = default;
};

/// A sequence evaluated left to right for the last child's value;
/// children are the body forms.
struct core_progn {
    // HIDDEN FRIEND
    friend constexpr auto operator==(core_progn, core_progn) -> bool = default;
};

/// A core-tree branch tag: what a compound expression is. Step R4 (the
/// elaborator) grows this variant as it lowers more special operators;
/// decision D18 keeps that growth slow — prefer lowering onto existing
/// forms over new tags.
using core_tag = std::variant<core_call, core_if, core_progn>;
```

Three branch tags: a call, a conditional, a sequence. A `core_call`'s callee is a symbol in the tag rather than a child, because in a Lisp-2 the callee is resolved in the function namespace and isn't a subexpression at all. D18 says a new special operator should be lowered onto forms the core already has unless it needs semantics none of them express, and the pivot's evidence is step L20 adding multiple values with no new node kind whatsoever. This list will grow &#x2014; there's no lambda here, no quoted data, no `setq` &#x2014; and which of those need a tag and which lower onto `core_call` is a question I get to find out.

The honest status of this file is that nothing produces one of these trees. `ast.test.cpp` builds the core for `(if (zerop n) 1 nil)` by hand, children-first, and checks that it folds, maps, and traverses with the leftmost bad leaf's error winning. That is a shape with law tests and no elaborator.


# The old reader is the oracle, except where it's wrong

D16 says correctness evidence comes from external authority rather than from tests written against this implementation's behaviour, and R3 has two authorities available: the standard, and the pivot's reader, which is frozen and never edited for exactly this reason.

`oracle_compare.test.cpp` runs shared syntax through both readers and requires agreement. Fixnums including leading whitespace and a preceding comment, symbols folded to uppercase, keywords, lists, the quote family down to `,@`, and five inputs both readers have to reject. `1+` and `2buffer` are in the symbol list by name, so DIV-0003 can't regress silently. The keyword comparison is the one that needed care: the oracle stores a keyword's name colon-stripped, the rebuild interns it colon-kept so that a keyword and a like-named symbol are distinct table entries, so the test compares `name(id).substr(1)` against the oracle's name and says in a comment why: the representation changed, the behaviour didn't.

Three cases are excluded. `+9` is the fixnum 9 in ANSI; the pivot's integer test allows a leading `-` and nothing else, so it reads the symbol `+9`. `10.` is the fixnum 10 by 2.3.1's trailing decimal point; the pivot reads a symbol there too. And a token longer than the pivot's fixed name storage was silently split into two tokens, where `scan_token` now reports "token too long".

Where the oracle disagrees with the standard, the standard wins and the case leaves the comparison. The over-long token is the same call made where the standard is silent: splitting a token in half and carrying on is a wrong answer, and a diagnosed limit is not.

None of that is visible in the test file. An exclusion is an absence: the reasons live in the step brief, and a reader of `oracle_compare.test.cpp` cold sees only a list that happens not to contain `+9`. D16 is what makes that a rule: a test may pin a scope decision, and a test must never pin a defect. Writing `agree_on_symbol("+9")` would have encoded somebody's bug into the rebuild's evidence, which is worse than having no test for `+9` at all; and there is one, in `read.test.cpp`, checking the ANSI answer against the standard directly. The third authority would be a differential check against SBCL, and in order to run that I need sbcl installed, which it isn't. So the new syntax (strings, characters, vectors, tower spellings) currently has the specification and my reading of it, and nothing else.


# The compiler bug, and which half of it broke

`number.hpp` has to find a `/` in a token to recognise a ratio, and an exponent marker to recognise a float. Spelled the obvious way, `text.find('/')` on a `string_view` that has been offset into a string literal, that returns the wrong answer at -O1 and above on GCC trunk r16-8246. `find` lowers to memchr, and the constant-folding of the memchr call reports the offset in the **original** literal instead of the offset view, so a `remove_prefix(1)` upstream is enough to be off by one.

The direction of the failure is the part I didn't expect. Every constant-evaluation problem in this project so far has been the evaluator refusing something the runtime path was perfectly happy with. Here the `static_assert~s all passed and the Catch2 run failed, because the constant evaluator doesn't lower anything to memchr and the optimizer does. The compile-time twin caught nothing; the runtime witness the project keeps insisting on is the only reason anyone noticed. The matrix from phase 24 already named the optimizer a witness of its own, and predicted this class; this is the first failure only that witness saw, the mirror of the ~pair_box` ordering bug that only `-O0` caught. `docs/verification-matrix.md` gets the second half of its table filled in.

The workaround is to spell the search as a predicate, which isn't lowered to memchr and folds correctly:

```cpp
auto const found =
    std::ranges::find_if(text, [target](char c) { return c == target; });
```

Two functions, `find_char` and `find_exponent_marker`, both carrying a comment saying why they aren't `find`, because there is no way to read that code cold and guess. It's flagged in the R4 brief for a toolchain divergence record, alongside DIV-0013's note that the older constant-evaluation bug still reproduces at the same revision.


# Where that leaves it

The merge criteria are the ordinary ones (compile, test, lint, headers compiling standalone), plus the project's standing rule that anything meaningfully constant-evaluable is stated twice, once as a `static_assert` over a constant evaluation and again as a runtime `TEST_CASE`. `read.test.cpp` has twenty-four of the former, covering source ordering, interning stability across reads, capacity exhaustion reported as a `parse_error` and not an assert, and error positions tracking line numbers. Reading the same name twice anywhere in an input yields the same `symbol_id`, which is R2's whole point, at its first caller.

The elaborator is R4. It's the first pass in the rebuild that consumes a tree instead of producing one, so it's also the first real test of whether D15's `traverse`-over-`result` is the error propagation it claims to be, or whether I find myself writing the ladder anyway and calling it a special case.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 25 - Interning Symbols, and Seven Divergences That Were One](phase-25-symbols.md)

</nav>


# References

McBride, Conor and Paterson, Ross (2008). *Applicative Programming with Effects*, Journal of Functional Programming.

Steele, Guy L. (1990). *Common Lisp the Language*, Digital Press.
