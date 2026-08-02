**DRAFT &#x2014; pending author revision**

<div class="abstract" id="orgac9e59e">
<p>
Step R1 builds <code>src/smd/cl/foundation/</code>, the substrate the rebuilt Common Lisp front end sits on.
It starts as the reviewed union of two independent copies of the same code: <code>smdscheme</code>'s, and the one in the sibling repository <code>compile-time-forth</code>.
The plan predicted the copies had not diverged, and they had not; for five of the files the entire difference is the include guard, the namespace, and a comment saying the file was adapted by copy.
Three things are new and were in neither copy: a left fold that stops on the first failure, which <code>std::ranges::fold_left</code> can not do; topological folds over columns whose elements name only earlier positions; and the Foldable and Traversable typeclasses (monoid instance objects and all) that <code>docs/CODING_RULES.md</code> has required all along and nothing had ever implemented.
The substrate doesn't have any behaviour of its own to show off, so the law tests are the step's evidence.
One of them failed only at <code>-O0</code>, and the fix that matters is process.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 23 - Why Rebuild Rather Than Refactor ←](phase-23-why-rebuild.md)

</nav>


# A merge that was mostly a rename

The step brief asked for a "reviewed union" of the two foundations, and R0's reading said to expect almost nothing to review. That held up. Diffing `src/smd/smdscheme/foundation/` against `src/smd/forth/foundation/` gives 7 to 15 changed lines per file, and for `functor`, `applicative`, `alternative`, `result` and `source_pos` the whole of that difference is three things: the include guard, the namespace, and a comment recording that the file was adapted by copy.

Three genuine differences survived. `compile-time-forth` had grown a `capacity()` on `static_vector` and a better default capacity on `tree_arena`, and both came across. The third is `parse_error`'s equality, where the two copies drifted in opposite useful directions: the scheme copy compares message pointers first and takes a match as a match, the forth copy handles a null message. The union keeps both, and the hand-rolled character loop each of them carried is now a `std::string_view` comparison. `static_vector`'s equality got the same treatment and is `std::equal` now.

Two independent copies of a component, with no semantic divergence between them, is the plan's stated signal that something wants to be a library (`docs/cl-rebuild-plan.md` §5). The signal is a good deal more convincing after you do the merge and find there was nothing to merge.

`result` is the clearest case of it.

```cpp
template <class T>
class result {
  public:
    /// The carried success type, named for effect-generic code such as
    /// @c traverse instances.
    using value_type = T;

    /// Constructs a successful result holding @p value.
    constexpr result(T value);

    /// Constructs a failed result holding @p error.
    constexpr result(parse_error error);

    /// Returns true if this result holds a value rather than an error.
    [[nodiscard]] constexpr auto has_value() const -> bool;

    /// Returns the contained success value.
    /// @pre has_value() == true
    [[nodiscard]] constexpr auto value() const -> T const &;

    /// Returns the contained error.
    /// @pre has_value() == false
    [[nodiscard]] constexpr auto error() const
        -> foundation::parse_error const &;

    // HIDDEN FRIEND
    friend constexpr auto operator==(result const &lhs, result const &rhs)
        -> bool = default;

  private:
    std::variant<T, foundation::parse_error> data_;
};
```

The only additions to what both copies already had are the `value_type` alias and the defaulted equality, and both are there because the typeclass instances and their law tests need them. There is also a comment about a step that has not happened. Decision D13 says the evaluator R5 builds needs a third alternative &#x2014; an unwind in flight &#x2014; as a distinct channel beside value and error. It isn't there. What is there is a note telling callers to use `has_value` / `value` / `error` and stay off the `std::variant` underneath, so the alternative set can widen later without breaking any of them. Cheap to write down now, expensive to retrofit, which is the only reason I put it in a header this early.


# A fold that stops

`std::ranges::fold_left` visits every element. That's the whole of its contract; there's no early exit in it and no way to ask for one. The elaborator that R4 will build propagates errors, and an error propagation that keeps going after the first failure is doing work it will throw away, on input a previous step has already reported as bad. So the substrate gets its own fold.

```cpp
template <std::ranges::input_range Range, class Acc, class F>
    requires short_circuit_effect<std::remove_cvref_t<std::invoke_result_t<
                 F &, Acc, std::ranges::range_reference_t<Range const>>>> &&
             std::constructible_from<
                 std::remove_cvref_t<std::invoke_result_t<
                     F &, Acc, std::ranges::range_reference_t<Range const>>>,
                 Acc>
constexpr auto fold_left_short(Range const &range, Acc init, F f) {
    using effect_type = std::remove_cvref_t<std::invoke_result_t<
        F &, Acc, std::ranges::range_reference_t<Range const>>>;
    for (auto const &element : range) { // substrate generic algorithm
        auto step = std::invoke(f, std::move(init), element);
        if (!step.has_value()) {
            return step;
        }
        init = step.value();
    }
    return effect_type{std::move(init)};
}
```

`short_circuit_effect` is the constraint on the step function's return: anything you can ask `has_value()`, and get a `value()` or an `error()` out of. `result` models it and `int` doesn't, and the test asserts both of those at compile time instead of leaving it as a comment. This is the piece decision D15 rests on, and it belongs in the substrate rather than in any one front end, because both front ends will want it.


# Two folds that disagree, on purpose

`traverse` over the `result` applicative also propagates errors, and it does something different. It visits all three elements of a three-element vector even when the first one fails, and returns the leftmost error. `fold_left_short` visits two of the three and returns the second one's. Neither of them is the other's optimization.

Both behaviours are pinned by tests, on both sides, each with a comment pointing at the other file. `CHECK(visited == 3)` in `static_vector_instances.test.cpp` and `CHECK(visited == 2)` in `fold_left_short.test.cpp`. The difference is contract, not style, so it gets tested like one.

And it is not an accident of the instance. An applicative `apply` takes two operands that have already been evaluated (McBride, Conor and Paterson, Ross, 2008); by the time it can see that one of them failed, the work that produced the other one is finished. An applicative traversal can discard after an error. It can not prevent it. Short-circuiting has to live in the fold, because the fold is the thing that gets to decide whether to call `f` again.


# A fold that is the recursion

`topo_fold.hpp` was in neither copy either, and nothing else in this step calls it; its tests build their columns by hand. The whole contract is one precondition: every position an element names is less than its own.

```cpp
template <class A, int Capacity, std::ranges::input_range Layers, class Alg>
    requires std::convertible_to<
        std::invoke_result_t<Alg &, static_vector<A, Capacity> const &,
                             std::ranges::range_reference_t<Layers const>>,
        A>
[[nodiscard]] constexpr auto fold_up(Layers const &layers, Alg alg)
    -> static_vector<A, Capacity> {
    static_vector<A, Capacity> done;
    for (auto const &layer : layers) { // substrate generic algorithm
        done.push_back(std::invoke(alg, done, layer));
    }
    return done;
}
```

A column that satisfies the precondition is a recursive structure that has already been flattened, and folding it in index order is the recursion. By the time the algebra runs for an element, every child the element names has a finished value in `done`, so the lookup is random access into the fold's own accumulator; the accumulator is the lookup table. Where an element keeps the positions it names is the caller's business; the algebra is handed the column so far and the element, and does its own looking. There's no recursive call in any of it, so nothing needs a stack and no input can blow one.

`fold_up_short` is the same fold wearing `fold_left_short`'s early exit: the algebra returns a `result`, the first failure comes straight back, and no later element is visited. `fold_down` is the dual. An ascending fold computes synthesized values, children before parents; a descending fold computes inherited values, parents before children, reading a parent column from the far end back toward the front. Attribute grammars have had names for that pair since Knuth; the substrate spells them as two loops.

`static_vector` grew `value_type`, `filled`, and `append_range` alongside, so a column can be built the same way the folds consume one; `filled` exists because `fold_down` materializes its whole result column before filling it in from the back.

A tree could store itself as such a column, and a tree that did would never need a visitor to walk it. But nothing here does; the substrate is committing to the shape before anything asks for it.


# Rules that had been written down and never followed

`docs/CODING_RULES.md` is Tier 1 in this project and has been authoritative all along; phase 23 already walked the gap between what it mandates and what `smdlisp` does. This step is where the mandates stop being aspiration: `fold_map` as the semantic centre, `fold_left` and `fold_right` derived where practical, `traverse` minimal and shape-preserving, instances by variable template, laws before substance.

So `fold_left` is derived, and here is the derivation.

```cpp
template <class F, class Acc, class T>
constexpr auto fold_left(this auto &&self, F &&f, Acc init,
                             T const &container) -> Acc {
        self.fold_map(
            [&](auto const &element) {
                init = f(std::move(init), element);
                return unit{};
            },
            container, unit_monoid);
        return init;
}
```

An accumulator captured by reference, updated inside a `fold_map` over `unit_monoid` (the monoid whose carrier holds nothing and whose combine does nothing). All the `fold_map` contributes is the order the visits happen in, which is exactly what I wanted, because the order is the instance's documented contract and deriving `fold_left` any other way would be restating it. It read like a trick to me the first time. It's the standard derivation.

`fold_right` doesn't get derived. Deriving it from `fold_map` wants either a function-composition monoid or a materialized reversal: the first is not practical under `constexpr` at fixed capacity, and the second needs a capacity the generic base can not know. That's a limit of the generic base, though. For a container that is a range, `std::ranges::fold_right` does the job directly, and an instance with a range in hand is free to say so. In the base it stays an instance primitive, with the reason written in the header instead of living in somebody's head.

It is also the one place the step's merge criterion reads wider than it is. The brief asked for `grep -c "for (int i = 0"` to return zero *outside* the generic algorithms, and it does. The commit message goes further and reports zero everywhere, "including inside the substrate's own generic algorithms, where range-for turned out to be enough". Range-for wasn't quite enough. `static_vector_foldable_impl::fold_right` is a `for (int i = values.size() - 1; i >= 0; --i)`, which that pattern doesn't match and which D15 permits anyway, since raw loops are allowed inside the substrate's own generic algorithms and nowhere else; `fold_down` walks its column the same descending way. The loops the exemption does cover now say so where they stand: each one in `topo_fold.hpp` carries a `// substrate generic algorithm` comment. The count is honest as far as it goes. It counts ascending loops.


# Instances are variable templates

```cpp
/// Registers the Functor instance for @ref static_vector.
template <class T, int Capacity>
inline constexpr auto functor_typeclass<static_vector<T, Capacity>> =
    static_vector_functor_map{};

/// Registers the Foldable instance for @ref static_vector.
template <class T, int Capacity>
inline constexpr auto foldable_typeclass<static_vector<T, Capacity>> =
    static_vector_foldable_map{};

/// Registers the Traversable instance for @ref static_vector.
template <class T, int Capacity>
inline constexpr auto traversable_typeclass<static_vector<T, Capacity>> =
    static_vector_traversable_map{};
```

Three specializations of three variable templates, in a header that exists to hold them. The primary template for each of `functor_typeclass`, `foldable_typeclass` and `traversable_typeclass` is `std::false_type{}`, so calling `fold_map` on a type nobody registered is a compile error at the point of lookup instead of a quiet fall-through to something plausible. The datatype and its adaptation stay separate: `static_vector.hpp` does not know what a functor is, and `static_vector_instances.hpp` is where it gets told.

Traversal order sits at the top of that header, and it belongs to the contract; every instance has to document its own. Index order, first to last, and for `traverse`, effects sequenced in the same order. Anything that derives from `fold_map` inherits it, which is the point of having one centre.


# The laws are the evidence

Nothing in this step computes anything anybody asked for. That's a real problem for a merge criterion: the usual shape of a step here is "here is a program that now runs", and there is no program. What there is instead is laws. Functor identity and composition, the applicative laws, monoid identity and associativity, the traversal identity law stated against an applicative that has no effect, shape preservation, and effect order. Each one is a `constexpr` predicate with a `static_assert` over it and a `TEST_CASE` calling the same function, which is this project's standing way of saying the same thing twice: once to the constant evaluator, once to a runtime witness. The suite runs over a thousand tests at the end of the step.

`identity` exists only so that the traversal identity law has something to be stated against: traversing with an effect that does no effect has to be `fmap`. It's a struct with one member and two typeclass instances. That's the cheapest law in the file, and it's the one that would catch a traversal quietly reordering or dropping something.


# Saying it twice wasn't enough

While I was writing the Foldable law tests, one of them failed in a way nothing here had failed before. The derivations are exercised over `pair_box`, a two-element container local to the test file, with the traversal order documented as first, then second. Fold `pair_box<int>{1, 2}` leftward with a step that appends digits, and the documented order gives 12. The `static_assert` said 12. `make test` said 12. The Debug build said 21.

The instance's `fold_map` combined its two images as `m.combine(f(pb.first), f(pb.second))`, and the two calls to `f`, as arguments to a single call, are unsequenced. The derived `fold_left` observes their order, because the derivation works by handing `fold_map` an `f` that updates an accumulator; the order was the one thing `fold_map` was contributing. Constant evaluation runs left to right. The optimizer picks left to right at `-O1` and above, and the default Asan config is `-O3`. So the compile-time twin passed, the default test run passed, and the contract was broken anyway; the only witness that could say so was the one nobody ran. gcc-14 at `-O0` ran the arguments right to left, and out came 21.

The fix in the code is dull. Name the first image, name the second image, combine the named things; the comment over it now says why the order is written in code instead of left to the compiler.

The fix in process is `make test-matrix`. Debug at `-O0`, Asan at `-O3`, and constant evaluation are three witnesses to the same contract, and no one of them subsumes another. I had been reading the config list the natural way, with the sanitizer build as the careful one and everything else as a speed run. It's the other way around. A sanitizer wants the optimizer: undefined behaviour that only comes into existence after inlining and constant folding never gets constructed at `-O0`, so there is nothing for it to trap. What the optimized build can not also show you is unspecified order in its naive form. The project has now been bitten from the end nobody runs, and `docs/verification-matrix.md` records it, with a table of what each witness can not see and a prediction: the mirror-image defect, one only the optimized build can catch, is out there in the constant-folding class, and the matrix exists so a witness is already running when it shows up.

`make test-matrix` is the merge criterion for a step now; plain `make test` stays the inner-loop command, and CI grew a Debug leg. `docs/cpp-rules.md` gets the two rules the bug teaches: a contract is witnessed three ways, and the order of two side-effecting calls is never left to argument evaluation. The second rule is older than C++. I knew it, of course. The build knows it now.


# fold\_fix, deleted

R0 read `foundation::fold_fix` in the old tree and concluded it could not be instantiated at all. Line 50 calls `fmap(tree.inner, lambda)`, while the CPO in `functor.hpp` takes `(function, value)`, so the lookup is `functor_typeclass<lambda>`, which is `std::false_type`, which has no `fmap`. Unqualified `fmap` inside the namespace finds the CPO variable, so ADL never runs and no overload can turn up to rescue it. R0 ran in a container with no gcc-16, so all of that was careful reading and nothing more.

This step ran where a compiler exists, and the compiler never got the chance to disagree. Zero call sites in either front end, a two-line stub for a test file, and a `CMakeLists.txt` listing only the header: nothing has ever instantiated it, which is why nothing ever complained. It doesn't come across into `src/smd/cl/` at all &#x2014; deleted rather than repaired, in the plan's words &#x2014; and `mendler_para` supersedes it. The `smdscheme` copy stays where it is.


# Still open

Re-running DIV-0013 was cheap, and there was a chance it would close a divergence for free. DIV-0013 records a GCC trunk defect where, under `-fsanitize=null`, a null comparison against the address of a subobject of a namespace-scope `constexpr` object does not fold; the divergence doc carries a five-line reproduction. At r16-8246 it fails exactly as recorded. So the divergence stays open.

The substrate compiles, its headers compile on their own, and every law it states holds. None of that says the shape is right. A substrate is judged by what gets built on top of it, and there is nothing on top of it yet. R2 interns symbols; that is the first client, and the first chance to find out.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 23 - Why Rebuild Rather Than Refactor](phase-23-why-rebuild.md)

</nav>


# References

McBride, Conor and Paterson, Ross (2008). *Applicative Programming with Effects*, Journal of Functional Programming.
