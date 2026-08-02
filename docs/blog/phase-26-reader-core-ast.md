**DRAFT &#x2014; pending author revision**

<div class="abstract" id="org6d964b7">
<p>
Step R3 built the rebuild's reader, and it reads considerably more Common Lisp than the rebuild can evaluate.
Decision D19 says reader syntax leads semantics, so strings, characters, <code>#(...)</code> vectors and every numeric tower spelling land at once, and a literal with no machine representation yet is carried as its spelling.
Every character decision is a table lookup, which is the claim that <code>set-macro-character</code> later widens an enum instead of forcing a rewrite.
The datum tree and the core AST are two instantiations of one new container, so Foldable and Traversable are written once, against the container, before either tree exists.
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

The pivot's datum tree is a fixpoint over an arena the caller has to keep alive, and only a datum's children live in that arena &#x2014; a root datum comes back by value, in a temporary that dies with the `read_datum` call. That is what the old tree's `core_symbol` documents: a `string_view` into a bare top-level symbol's name dangled the moment the read result went out of scope, AddressSanitizer caught it as a stack-use-after-return, and the fix was to copy the folded name into the core node. R3's replacement is a plain class that owns its own nodes.

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
    /// One node: a leaf or a branch.
    using node_type = std::variant<Leaf, branch_type>;
    /// A branch's child-index list.
    using child_list = static_vector<int, MaxChildren>;

    constexpr tagged_tree() = default;

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

    // HIDDEN FRIEND
    friend constexpr auto operator==(tagged_tree const &, tagged_tree const &)
        -> bool = default;

  private:
    static_vector<node_type, MaxNodes> nodes_{};
    int root_{-1};
};
```

Children are `int` indices into the tree's own `static_vector`, so a tree is an ordinary value: copyable, comparable with the defaulted `operator==`, usable in constant evaluation, with no external arena to keep alive. Indices carry no capacity, which is D14's rule about capacity staying out of type identity; `MaxNodes` parameterises the tree because the tree is the storage.

The interesting property is in `add_branch`'s precondition. A branch may only name nodes that already exist, so node index order is a topological order with every child before its parent, and a builder that adds children left to right leaves the leaves in left-to-right source order. That one precondition is the whole traversal-order contract:

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
        auto const &tc = applicative_typeclass<effect_type>;
        auto accumulated = tc.pure(out_tree{});
        auto const append_leaf = [](out_tree out, B mapped) {
            out.add_leaf(std::move(mapped));
            return out;
        };
        std::ranges::for_each(std::views::iota(0, tree.size()), [&](int i) {
            if (tree.is_leaf(i)) {
                accumulated = tc.invoke(append_leaf, std::move(accumulated),
                                        std::invoke(f, tree.leaf(i)));
            } else {
                accumulated = tc.invoke(
                    [&branch = tree.branch(i)](out_tree out) {
                        out.add_branch(branch.tag, branch.children);
                        return out;
                    },
                    std::move(accumulated));
            }
        });
        return tc.invoke(
            [root = tree.root()](out_tree out) {
                if (root >= 0) {
                    out.set_root(root);
                }
                return out;
            },
            std::move(accumulated));
    }
};
```

D15 asks that the datum and core trees carry Foldable and Traversable with documented order as part of the instance contract, and that `traverse` over the result applicative **be** the elaborator's error propagation (McBride, Conor and Paterson, Ross, 2008). Because both trees are instantiations of `tagged_tree`, all of the instances (Functor, Foldable, Traversable) are written once against the container and law-tested once, and neither tree has one of its own. D15 is filed as a style rule and is really an architecture one, and this is why: in the pivot, a walk got written wherever a walk was needed, because arena indices plus a wide `std::visit` leave nothing else to write.

`traverse` visits every leaf. Stopping early is the effect's business, and for `result` that means the leftmost error wins after the whole tree has been walked. When later work genuinely must be skipped, R1's `fold_left_short` is the other shape, and the header says which one you're getting.

The reader itself does not traverse anything, and the reason is structural. A parse is an unfold: there's no tree to walk until the reader has built one. So `read.hpp` chains its steps through a small `and_then` over `result`, which is the same rule D15 states (error propagation is not an `if (!r.has_value())` ladder) reaching the one pipeline stage where `traverse` can't be the answer.


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

The direction of the failure is the part I didn't expect. Every one of the constexpr problems in this project so far has been constant evaluation refusing something the runtime path was perfectly happy with. Here the ~static\_assert~s all passed and the Catch2 run failed, because the constant evaluator doesn't lower anything to memchr and the optimizer does. The compile-time twin caught nothing; the runtime witness the project keeps insisting on is the only reason anyone noticed.

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
