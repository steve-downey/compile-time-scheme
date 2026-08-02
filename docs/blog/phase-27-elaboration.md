**DRAFT &#x2014; pending author revision**

<div class="abstract" id="org37971fe">
<p>
The elaborator turns a datum tree, which is what the source said, into a core tree, which is what the program means.
It recognizes three special operators, which is the least interesting thing about it.
All three of its passes are index loops over <code>std::views::iota</code>, and none of them calls itself.
A <code>tagged_tree</code> is built children before parents, so ascending index order is already a bottom-up fold over the whole tree, and descending index order is already a top-down propagation.
No stack, no recursing visitor, no limit on how deeply a form may nest.
Nothing evaluates the core tree yet.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 26 - Reader and Core AST ←](phase-26-reader-core-ast.md)

</nav>


# What the reader refused to decide

The reader from phase 26 reads `(if (zerop n) 1 nil)` as a list of four things and stops. It doesn't know `if`. Phase 4 made the same split for the Scheme front end and the reason has held up: a reader that recognizes keywords has to change every time the language gains a special operator, and this language is going to gain a lot of them.

What's different in the rebuild is that `nil` and `t` aren't special to the reader either. They're symbols, interned like every other symbol (phase 25), and the elaborator is where they stop being lookups and become constants. So elaboration is where a list stops being a list and becomes a conditional, a sequence, a literal, or a call. Three special operators recognized today; everything else in head position is a function call.


# Index order is the recursion

The claim this step is built on: a recursion over an arena tree does not need recursion.

`foundation::tagged_tree` keeps its nodes in a `static_vector` and hands back an index when you add one. You can only add a node once its children exist, because the node stores their indices. So a parent's index always exceeds every one of its children's, and the node array is a topological sort of the tree. Nobody sorted it. It came out that way.

Which means ascending index order *is* a bottom-up catamorphism (Meijer, Erik and Fokkinga, Maarten and Paterson, Ross, 1991) (every node visited after all of its children), and descending index order is the mirror, every node visited after its parent. Both are folds over `std::views::iota(0, tree.size())`, one of them through `std::views::reverse`. So no pass needs a stack or a visitor that calls itself, and nesting depth stops being a resource. The only capacity that matters is the node count, which is a number the tree already knows.

D15 says traversals are typeclass instances rather than hand-written recursion, and it's the sort of rule that reads as style until you see what it costs to break. Here it's the whole design.


# Pass one: atoms

```cpp
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
```

A visitor over one node's alternatives is the use of `std::visit` the coding rules allow: it dispatches an atom, and something else drives the traversal. Driving is `traverse` over the result applicative (McBride, Conor and Paterson, Ross, 2008), ascending, and that `traverse` is the entire error propagation. Shape preserved, branch tags untouched, every leaf replaced by what it denotes.

Two decisions live in that struct. `NIL` and `T` become constants instead of variable references, by id comparison against what the table interned, never by string comparison (D12). And a numeric tower literal is an error. D19 has the reader accept the full ANSI spelling of every number in the tower before the evaluator can do anything with any of them, so `1/3` reads perfectly well and then elaborates to "not yet executable." The message says "not yet" on purpose; the spelling is fine and the semantics are owed.

A string literal too long for the core's storage is diagnosed rather than truncated, and `core::max_string_chars` is not shared with the reader's constant. A datum you can read is not automatically a program you can hold, and pretending otherwise means silently losing characters somewhere between the two.


# Pass two: roles, and which namespace an atom is in

```cpp
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
```

Each node learns from its parent what position it occupies: an expression, quoted data, the head of a form, or unreached. The pass is total. There's no effect to thread and nothing to short-circuit, so it's a `for_each` rather than a fold with a result in it, and it can't fail.

Reachability falls out of the initialization. Every node starts `unreachable`, only the root is set to `evaluate` before the loop runs, and a node only ever gets a role from its parent. A datum nothing reaches keeps the role it was born with, and the emission pass emits nothing for it. The reachability analysis in this file is an enumerator's default value.

The Lisp-2 point is worth being careful about, because the passes look like a mistake followed by a fix. Pass one reads a symbol as `core_variable`, which is the value-position reading. Pass two is what decides that a given occurrence is a function name, or a piece of quoted data, and rewrites nothing in order to do it; it assigns a role, and pass three is the one that reads it. An atom names a symbol. Only position picks a namespace, and pass one has no position available to look at. The order of the two passes is the order the information arrives in.


# Pass three: emission, and the fold that has to stop

Emission ascends, accumulating a map from source node index to emitted core node index. Because it ascends, every child of the node currently being emitted is already in that map, which is how a node gets its children's core indices without going and looking for them.

Here is the smallest piece of it, the one that turns a quoted list into pairs:

```cpp
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
```

This one is `fold_left_short` and not `traverse`, and the difference is the point of having both. `traverse` visits every leaf, which is what pass one wants, because leaves are independent of each other and an unreachable bad atom is still worth a diagnostic. Emission wants the opposite. The core tree is one shared arena, so a form that fails halfway has already spent capacity on nodes nobody is ever going to look at, and visiting the rest of the tree to collect more diagnostics would spend more of it on the same nothing. `fold_left_short` is the fold that can decline to keep going, which is why it exists (phase 24).

A third shape neither of them covers is a step whose input is the previous step's output, with no structure and no sequence. `and_then` is that shape, and it had been sitting in `reader::detail` since phase 26. The R3 brief expected it to get promoted on the second use and it did, into `foundation`, with monad-law tests. The reader keeps the spelling with a using-declaration for the two call sites that still write `detail::and_then`; inside the namespace, ADL would have found it anyway. The two definitions could not coexist: while both existed, every unqualified call in `reader::detail` was ambiguous.

The same rule has an unpaid bill going the other way. `intern_checked` is written twice now, once in `reader::detail` and once in `elaborator::detail`, and I left the second copy rather than grow `foundation/` in a step that didn't own it. Two copies was the signal in phase 24. R5 needs interning too, so R5 decides.

The whole elaborator is those three tools composed:

```cpp
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
```

Lower the atoms, plan the roles, fold the emission, set the root.


# One new tag, and the reason D18 allows it

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

/// Hermetic pair construction, used only to build quoted list data;
/// children are the car and the cdr.
///
/// Decision D18 says lower onto existing forms before adding a tag, and the
/// existing form here would be @ref core_call to `cons` — which is wrong,
/// not merely inelegant. ANSI quoted data is a literal, not a call, so
/// `'(1 2)` must not depend on what the function slot of `CONS` holds; a
/// program that redefines `cons` still gets pairs from its quoted lists.
/// That is a semantics no existing tag expresses, which is exactly D18's
/// stated exception.
struct core_cons {
    // HIDDEN FRIEND
    friend constexpr auto operator==(core_cons, core_cons) -> bool = default;
};

/// A core-tree branch tag: what a compound expression is. Step R4 (the
/// elaborator) grows this variant as it lowers more special operators;
/// decision D18 keeps that growth slow — prefer lowering onto existing
/// forms over new tags.
using core_tag = std::variant<core_call, core_if, core_progn, core_cons>;
```

D18 says lower a new operator onto forms the core already has before adding a node kind, and the pivot showed it works in the step that had the most reason to break it. The obvious lowering for `'(1 2)` is a call to `cons`. It's wrong, and not in a matter-of-taste way. ANSI quoted data is a literal, so what `'(1 2)` yields must not depend on what the function slot of `CONS` holds; a program that redefines `cons` still gets pairs out of its quoted lists. No existing tag expresses that, which is the exception D18 states for itself. One tag, `core_cons`, and the emission pass builds quoted lists out of it right to left from a fresh `NIL`.

The leaf variant grew too: `core_character`, `core_string` owning its own characters, and `core_quoted_symbol`. Eight leaves and four tags now, where phase 26 had five and three. `core_quoted_symbol` is a separate alternative from `core_variable` for the same reason pass two exists: the same interned id will reach the evaluator as a value in one position and as a lookup in the other.

`''x` and `'#'f` get a representation because ANSI fixes one: 2.4.6 makes `'x` mean `(quote x)` and 2.4.8.2 makes `#'x` mean `(function x)` (Steele, Guy L., 1990), so the elaborator materializes the two-element lists. It has to intern `QUOTE` and `FUNCTION` itself, because the reader never spelled either one; `'x` is a branch tag, not a token, so nothing put those names in the table. That is why `elaborate` takes the symbol table by non-`const` reference, which looked like a wart until I found the reason for it. Backquote gets no representation at all. 2.4.6.1 makes it implementation-defined, so it's the macroexpander's choice to make, whenever there's a macroexpander.


# What it doesn't do

Elaborated scope today is literals, variables, keywords, calls, `if`, `progn`, `quote` in both spellings, and `()` and `(progn)` as `NIL`. Everything outside that comes back as an error with a sentence saying which thing it was: vectors, `#'f`, the backquote family, the tower. Nothing is dropped quietly, and capacity isn't asserted either: a form too big for the tree it was handed to fill returns a `parse_error`, because an assert is not an acceptable answer to a program that's merely large.

The gap I'd close first is the one I can't close from here. The datum tree carries no source positions, so every elaborator-level diagnostic gets a default `source_pos`. Reader errors keep their real ones, which means a failure's position currently tells you which stage failed and nothing more useful than that. Fixing it means putting a position or a span on `tagged_tree` nodes, and that's reader and foundation work, not elaborator work. Worth knowing before writing an evaluator, since its diagnostics would inherit the same hole.

I also can't check any of this against a real Common Lisp yet. There's no implementation installed in this environment, so R3's reader syntax and R4's elaboration decisions both come from reading the standard and get pinned by tests I wrote. That's the weakest evidence in the project right now, and D16 says so: conformance is supposed to be measured against external authority, and a differential run against SBCL is still owed.


# The merge criterion

`(if (zerop n) 1 nil)` elaborates to the shape `core/ast.test.cpp` builds by hand, rebuilt in `elaborate.test.cpp` with the ids the reader actually interned: same nodes in the same order, compared with `==` inside a `static_assert`. That's the entire claim this step makes. Nothing runs the core tree; the evaluator is R5 and it doesn't exist. What I have is a data structure and a test saying it has the shape I meant it to have.

The next step's question is whether a CPS evaluator over the same tree can stay in the same shape &#x2014; three index folds, no recursive descent, because the tree only *looks* recursive. I don't see why it couldn't. That isn't the same as knowing.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 26 - Reader and Core AST](phase-26-reader-core-ast.md)

</nav>


# References

McBride, Conor and Paterson, Ross (2008). *Applicative Programming with Effects*, Journal of Functional Programming.

Meijer, Erik and Fokkinga, Maarten and Paterson, Ross (1991). *Functional Programming with Bananas, Lenses, Envelopes and Barbed Wire*.

Steele, Guy L. (1990). *Common Lisp the Language*, Digital Press.
