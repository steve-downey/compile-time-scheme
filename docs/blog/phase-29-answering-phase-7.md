**DRAFT &#x2014; pending author revision**

<div class="abstract" id="orgcd424fb">
<p>
No step of the rebuild landed here.
Phase 28 said evaluation is the one traversal in this pipeline that can't be a fold, and phase 7 of this same series evaluated <code>if</code> inside a fold, selectively, and shipped a post about it.
So the sentence was wrong as written, and re-deriving the true one turned up something underneath it: the tree has been carrying two invariants as if they were one, and the one every recursion scheme depends on was documented everywhere and checked nowhere.
The evaluator didn't change. What changed is an argument, plus three asserts.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 28 - Three Channels, and a Machine Instead of a Fold ←](phase-28-three-channels.md)

</nav>


# The sentence that was wrong

Phase 28's abstract said the evaluator is a small-step machine "because evaluation is the one traversal in this pipeline that can't be a fold", and the section under it argued the point from the shape of a catamorphism: every node visited once, in an order the structure fixes, so there's no way to say *evaluate this arm and not that one*.

That argument is true of the plain scheme and it isn't the whole argument, because this series already solved exactly that problem inside a fold. Phase 7 is called "Mendler Interpretation" and it exists for the `if` case.

The post is still marked draft, so I went back and rewrote the section instead of filing a correction under it. This is what the rewrite is about.


# Phase 7 already ran `if` inside a fold

A Mendler-style fold hands the algebra its own `recurse` function (Mendler, Nax Paul, 1987). The algebra can call it on whichever children it likes, in whatever order, with whatever context, and it can decline to call it at all. So `comp_if` recursed into the condition, then into the arm it chose, and never touched the other one. `comp_lambda` recursed into nothing and captured the environment instead. A body was recursed into once per call, in the environment the call had built, which is where lexical scope came from.

That's selective descent, and it's a fold with the uniformity requirement dropped (Bird, Richard S. and de Moor, Oege, 1997). `smd::fixpoint::mendler_fold` and `mendler_para` are still in the repository, in `src/smd/fixpoint/recursion_schemes.hpp`, and the interpreter built on them is still in `src/smd/smdlisp/sender/sender_eval.hpp`, compiled and tested, with its `core_if` arm intact. "Evaluation can't be a fold" is a claim contradicted by code that compiles in this tree today.


# The knob has nothing to turn

The representation is what ruled the Mendler fold out, and phase 28 now says so in two sentences, so here is the part that doesn't fit in two sentences: the price.

Phase 5 built `Fix<CompF>`, where a child is a `Box` and reaching it is dereferencing a pointer, and the recurse-knob exists to decide when to chase one. Getting the knob back means building that tree again beside the columnar one: a second full spelling of every core node kind, and capacities back inside the types (`comp_lambda<A, MaxList>`, `env<CompT, 16>`). DIV-0016 priced that at a step's worth of work that buys nothing, and D14 is the decision that capacities don't belong in node identity. Those are two of the reasons the rebuild was argued for in the first place, which makes this a nice test of whether the argument holds when it's inconvenient.

There's a second reason, however, and it's the cheaper one to state. Every `recurse` in a Mendler interpreter is a C++ activation record, so under constant evaluation the object program's depth is the compiler's `-fconstexpr-depth`. This tree wants that number to be `limits::frames`, diagnosed like every other capacity.

And the sentence still wasn't right even then, because it says what the machine isn't. Phase 28 carries where that came out (evaluation recurses over the trace, and the tree only seeds it, so the machine is an unfold (Meijer, Erik and Fokkinga, Maarten and Paterson, Ross, 1991) with a name of its own), along with the new rule in `docs/cpp-rules.md` that names it. What's worth adding here is only that the rules hadn't named it because the rules were written about trees, and a document that has been right about trees for four phases is a hard thing to notice the edge of.


# The warning that inverted

`docs/fixpoint-mendler-reference.md` is the design document behind phases 5 through 8, and it has been sitting in `docs/` reading like live guidance for a foundation this tree doesn't have. Two of its sections don't merely fail to apply, they come out backwards.

"Why Environment-Threading Breaks Catamorphisms" lists the two cases as lazy `if` and a body evaluated in the extended environment; both are answered here by `para` plus a machine. "Applicative Parallelism Preserved" is the one phase 28 already takes apart: its prediction that a CPS trampoline linearizes what `Fix<CompF>` preserved is true of a pointer tree and backwards on a column. What that post doesn't say is that the prediction has been sitting in `docs/` for four phases with nothing marking it as spent, which is the part that worries me more than the claim did.

The document gets a status header saying not to plan `src/smd/cl/**` work from it. DIV-0016 gets a dated note recording the prediction as obsolete rather than pending, since the document carrying it doesn't say so itself. And R7's brief gets the decision instead of the inheritance: porting the pivot's Mendler sender interpreter is ruled out, and a `when_all` demonstration, if there's one to make, comes from an argument frame that forks. Scoped honestly, too. Left-to-right argument evaluation is the conforming order as soon as an argument can have effects (ANSI 3.1.2.1.2.3) (Steele, Guy L., 1990), so whatever R7 shows is either about the effect-free fragment or about visible graph structure, and never about evaluation order.


# One invariant was two

Reading all of that back against the code is where the hole turned up.

The claim I'd been repeating is that a `tagged_tree` is built children-before-parent, so index order is topological order, so every pass is a linear fold. Two claims, wearing one coat.

```cpp
/// Two invariants govern this type. They are not the same invariant, and
/// only the first is the container's to keep.
///
/// **I1 — children before parent. Guaranteed here.** A branch may only
/// name nodes that already exist, so every child's index is strictly less
/// than its parent's, and node index order is therefore *a* topological
/// order. @ref add_branch establishes it, @ref from_nodes requires it,
/// and @ref is_children_before_parent is the predicate both are stated in
/// terms of. I1 is the whole of what the recursion schemes in
/// <smd/cl/foundation/tagged_tree_schemes.hpp> need, and it is enough for
/// them: an algebra receives its children through the branch's own child
/// list and never sees an index, so a scheme's answer is a function of
/// the tree's *shape* and not of its layout.
///
/// **I2 — leaves in source order. Not guaranteed here.** I1 admits many
/// layouts of one shape, since any topological order satisfies it, and
/// the element-wise operations are not indifferent to which one they got.
/// Foldable and Traversable visit leaves in ascending index order
/// (<smd/cl/foundation/tagged_tree_instances.hpp>), so `fold_map` under a
/// non-commutative monoid, and the leftmost error of a `traverse`, are
/// answers about the layout as much as about the tree. Index order is
/// left-to-right source order exactly when the builder adds each node's
/// children left to right and the parent after them — which the reader
/// and the elaborator do, and which this class can not check, because
/// nothing in a finished tree records how it was built.
///
/// The consequence to keep in view: a builder that keeps I1 and drops I2
/// — one emitting by level, or hash-consing shared subtrees — leaves
/// every scheme's answer unchanged and silently changes which atom
/// `elaborator::lower_atoms` calls the leftmost bad one. That is a
/// diagnostic moving without a scheme moving, so if I2 ever has to hold
/// for a new builder, test it at that builder.
```

I1 is the container's and is guaranteed. I2 holds because the reader and the elaborator add each node's children left to right and the parent after them, and no type can check it, because nothing in a finished tree records how it was built.

The schemes need I1 alone:

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
/// This needs I1 (@ref tagged_tree) and nothing else, and in particular
/// needs no law of the algebra — not commutativity, not associativity.
/// The children reach the algebra through the branch's own child list, in
/// that list's order, so two layouts of one shape give one answer, and
/// which topological order the builder chose is invisible here. That is
/// not true of the Foldable and Traversable instances, whose element
/// order *is* index order; see I2.
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

The algebra receives its children through the branch's own child list, in that list's order, and never sees an index at all. Two layouts of one shape give one answer. They also need no law of the algebra, neither associativity nor commutativity, which is stronger than I'd been assuming and is the half that matters.

The Foldable and Traversable instances are not in that position. They visit leaves in ascending index order, so `fold_map` under a monoid that doesn't commute, and the leftmost error of a `traverse` over the `result` applicative (McBride, Conor and Paterson, Ross, 2008), are answers about the layout as much as about the tree. R3 had already pinned the consequence in a test called `cata_and_fold_map_disagree_on_permuted_trees`: one shape laid out two ways, the fold through the child lists says 12 and the fold through the index order says 21, and both are correct. What was missing was the invariant having a name, so that a pass could say which one it was standing on.

What moves is a diagnostic. No scheme's answer moves at all. A future builder that keeps I1 and drops I2 &#x2014; one emitting by level, or hash-consing shared subtrees &#x2014; leaves every scheme's result unchanged and silently changes which atom `lower_atoms` reports as the leftmost bad one. D15 gets a dated scope note for this: its claim that index order makes every pass a linear fold is too broad by one pass, and it's the pass that carries the diagnostics. The decision is unchanged. What was wrong is the reasoning underneath it, which licensed three passes with an argument that reaches two.


# The precondition nothing checked

`from_nodes` carried no children-before-parent precondition at all, neither documented nor checked. `add_branch` documented one and didn't check it either. So the invariant every scheme depends on was written in prose in half a dozen comments and enforced in none, and a synthesized node sequence holding a forward reference ran into the link column at the wrong offset, or off the end of it, inside `link_children` instead of failing as a precondition.

```cpp
    tagged_tree built;
    built.nodes_.append_range(std::forward<R>(node_range));
    // Checked before relink rather than after: link_children would index
    // links_ through a forward reference, so a violation is out of bounds
    // there and merely wrong here.
    assert(built.is_children_before_parent());
    built.relink();
    if (root >= 0) {
        built.set_root(root);
    }
    return built;
}

template <class Leaf, class Tag, int MaxNodes, int MaxChildren>
constexpr auto
tagged_tree<Leaf, Tag, MaxNodes, MaxChildren>::is_children_before_parent() const
    -> bool {
    return std::ranges::all_of(
        std::views::enumerate(nodes()), [](auto const &entry) {
            auto const &[index, node] = entry;
            auto const *branch = std::get_if<branch_type>(&node);
            return branch == nullptr ||
                   std::ranges::all_of(branch->children, [index](int child) {
                       return child >= 0 && child < static_cast<int>(index);
                   });
        });
}
```

The check runs before `relink`, so the diagnosis lands at the mistake instead of two operations later. A hand-assembled sequence is the only way in: `add_branch` can't name a node that doesn't exist yet, and a caller mapping `nodes()` to a new sequence keeps the indices and so keeps I1 for free, which is what the Traversable instance does on its way back through `from_nodes`.

`fold_down` had half the same hole one layer down. The precondition was written; nothing checked it. There it's checkable where it's needed, since a parent column is a range of positions and the fold can see what it requires:

```cpp
template <class A, int Capacity, std::ranges::random_access_range Parents,
          class Step>
    requires std::integral<std::remove_cvref_t<
                 std::ranges::range_reference_t<Parents const>>> &&
             std::convertible_to<std::invoke_result_t<Step &, A const *, int>,
                                 A>
[[nodiscard]] constexpr auto fold_down(Parents const &parents, Step step)
    -> static_vector<A, Capacity> {
    int const count = static_cast<int>(std::ranges::distance(parents));
    assert(std::ranges::all_of(
        std::views::enumerate(parents), [](auto const &entry) {
            auto const &[index, parent] = entry;
            return static_cast<int>(parent) < 0 ||
                   static_cast<int>(parent) > static_cast<int>(index);
        }));
    auto done = static_vector<A, Capacity>::filled(count, A{});
    for (int index = count - 1; index >= 0; --index) { // substrate generic
                                                       // algorithm
        int const parent = static_cast<int>(parents[index]);
        done[index] =
            std::invoke(step, parent < 0 ? nullptr : &done[parent], index);
    }
    return done;
}
```

`fold_up` can't have the same treatment. It's handed layers whose child positions only the algebra knows how to find, so its precondition can not be stated in terms of anything it holds, and the tree that satisfies it carries the check instead.

An `assert` in a `constexpr` function is a better tool here than it looks. NDEBUG shows up only in Release and RelWithDebInfo, and a step merges on `make test-matrix`, which runs Debug and Asan, so these are live in both configurations that matter. And a failed assertion under constant evaluation isn't an abort at run time. It's a translation that doesn't happen.


# What it doesn't buy

No program behaves differently, and no test that existed before this phase changed its answer.

The negative direction stays untestable by design. A precondition violation is undefined behaviour, so what the tests pin is the predicate and the positive direction: a built tree satisfies I1 and so does its round trip through `from_nodes`, a synthesized sequence that satisfies it is accepted and links correctly, and two layouts of one shape both satisfy it, compare unequal, and hand back their leaves in different orders. Nothing pins what happens to a caller who ignores the precondition, and nothing should.

I2 is enforced nowhere and can't be. What the header leaves behind is an instruction (if I2 ever has to hold for a new builder, test it at that builder), and an instruction is not a check. That's the part I'd expect to get bitten by, because the failure mode is a diagnostic moving quietly while every test stays green.

The check isn't free either: `is_children_before_parent` is linear in the edges and runs on every `from_nodes` call, which is every `traverse` over a tree, in the two configurations that have asserts turned on. Nothing in this project is anywhere near the size where I'd notice, and I'm recording it so that a later me who does notice knows where to look.


# Two backlog items moved

BL-0001, the elaborator as a fold over a fix algebra, closes. It was the last surviving aside of a plan file deleted back in July, and R1 deleted the `fold_fix` it was written about while R3 shipped the named schemes that replaced it and R4 built the elaborator out of them.

BL-0003 opens. Phase 28 called the implicit-block elision obvious and undone, and rethinking it against the substrate R1 through R4 built moves it out of the evaluator entirely: "does this subtree contain a `return-from` naming N" is a synthesized attribute, so it's a `cata` in the elaborator, and where the answer is no, emission just doesn't build the `core_block` node. That is what stands between an `enter_closure` that pushes no frame and something that deserves to be called tail calls. It's still not scheduled, because nothing has forced it, and because eliding a block changes which programs fit inside a frame budget, which is a thing to do once the conformance corpus is there to notice.

None of this was found by a test. It was found by writing a sentence down and then reading the repository to see whether it was still true, which is a method that scales exactly as far as my patience does. R6 is next, and it's the step that finally asks somebody other than me whether the answers are right.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 28 - Three Channels, and a Machine Instead of a Fold](phase-28-three-channels.md)

</nav>


# References

Bird, Richard S. and de Moor, Oege (1997). *The Algebra of Programming*, Prentice Hall.

McBride, Conor and Paterson, Ross (2008). *Applicative Programming with Effects*, Journal of Functional Programming.

Meijer, Erik and Fokkinga, Maarten and Paterson, Ross (1991). *Functional Programming with Bananas, Lenses, Envelopes and Barbed Wire*.

Mendler, Nax Paul (1987). *Recursive Types and Type Constraints in Second-Order Lambda Calculus*.

Steele, Guy L. (1990). *Common Lisp the Language*, Digital Press.
