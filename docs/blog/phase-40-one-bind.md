**DRAFT &mdash; pending author revision**

<div class="abstract" id="org97ee6f7">
<p>
This step creates <code>smd::kit::parser</code>: a <code>parser&lt;F&gt;</code> over a context threaded through every call.
The type is registered as a Monad instance instead of getting a <code>bind</code> of its own, and <code>read_radix_number</code> is converted as its first client.
That registration, as the brief illustrated it, does not compile.
<code>monad_typeclass</code> is a variable template in <code>smd::kit::foundation</code>, and a specialization has to be declared in a namespace enclosing the primary template's own.
<code>smd::kit::parser</code> sits beside that namespace instead of inside it.
The step checked that against GCC 16 instead of assuming it, and used the shape <code>src/smd/cl/foundation/tagged_tree_instances.hpp</code> already carried for the same reason one level down.
<code>grep 'monad_typeclass&lt;parser'</code>, the check that was supposed to find the registration, finds nothing, and the step wrote down that no valid spelling could match it.
One valid spelling does. It is not the one that landed, and the one that landed had the precedent.
<code>read_radix_number</code> is now <code>bind(token_p, classify_and_add)(cur, ctx)</code>, with the same two diagnostic strings at the same two positions and <code>scan_token</code> untouched.
Of the three primitives the step lands, <code>bind</code> is the only one anything outside the kit's own tests calls.
<code>monad.hpp</code>'s doc comment says an instance that defers its continuation has to supply <code>join</code>, <code>then</code> and <code>apply</code> itself instead of inheriting them.
Its own example of such an instance is "a parser that stores what to do next".
<code>parser_monad_map</code> inherits all three.
The reader's <code>CMakeLists.txt</code> gains <code>kit.parser</code>, a real new dependency, recorded as out of declared scope instead of passing unremarked.
At 1474 seconds it runs a little over twice as long as the header split before it, which the plan calls this phase's floor.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 39 - Eight Headers, and a Comment the Formatter Made False ←](phase-39-eight-headers.md)

</nav>


# The vocabulary the reader was missing

`cl`'s reader was written in the combinator style before anything here called it that. `src/smd/cl/reader/cursor.hpp` defined its own `parse_state<T>` (a value and the cursor after it), and every reader function returns `foundation::result<parse_state<T>>`, which is a parse result under a different name. `read_wrapped` is nested `and_then` continuations. Bind, written out by hand.

The combinator layer this repository already had lived in `src/smd/smdscheme/parser/`, and it was Applicative and Alternative: `pure`, `satisfy`, `char_p`, `map`, `lift2`, `sequence_left`, `sequence_right`, `operator|`, `alt`, `many`, `some`, `optional`. Nothing named `bind`, `and_then` or `flat_map`. Which was enough for Scheme, whose reader never needs the value of one parse to choose the next.

Common Lisp's does, in at least four places. `#nnR` reads a radix that then decides how the following token is classified, and `#\name` looks up a character name. Sharpsign sub-dispatch selects on the character after the `#`, and every datum position dispatches through the readtable. `lift2` fixes both of its parsers before either one runs, so no Applicative composition reaches any of the four. So adopting combinators here means adding `bind`. That is the whole of it.

There's a wrinkle about where the old layer is. `src/smd/smdscheme/` left trunk when the two retired front ends were deleted, so the file this step was told to read for the shape of `map` and `lift2` is reachable only as `git show iteration/smdscheme-final:src/smd/smdscheme/parser/parser.hpp`. The new `parser<F>` is recognizably that file with a context threaded through it, down to the deduction guide and the wording of the class comment. Its `cursor` and `parse_state` came from the other direction, out of `cl`'s live reader, which is why `src/smd/cl/reader/cursor.hpp` is now a twenty-nine-line forwarding shim.

That split turns out to matter later in the same step. `smdscheme`'s `parse_state` was `{ T value; cursor rest; }` and nothing else. `cl`'s carries a defaulted `operator==` as a hidden friend, and the law tests are written as "two parsers are equal when they produce the same `parse_result` for the same input and context." The half of the layer that makes the laws checkable is the half that didn't come from the parser kit.


# An instance, and not a second bind

`src/smd/kit/foundation/monad.hpp` arrived after the combinator plan was first written: a `monad<Impl>` CRTP base, a `monad_typeclass<T>` lookup, a `bind` CPO, with `result<T>` registered against it in `result_instances.hpp`. So the brief is emphatic that `bind` isn't invented here. `parser<F>` becomes a second registered instance in the same shape. The risk being guarded against is someone writing a free `bind(p, f)` that looks like the CPO, because registering an instance looks like more work.

```cpp
/// Monad @c Impl for @ref parser.
///
/// @c bind runs @p p; only on success does it run @p g on @p p's value and
/// resume from @p p's rest cursor, over the same threaded context. Written
/// in terms of @ref foundation::result's own registered Monad instance one
/// level down, rather than re-deriving the success/failure branch by hand:
/// @c p(cur, ctx) is already a @c foundation::result, so sequencing it
/// through @c foundation::bind is what skips @p g on failure.
struct parser_monad_impl {
    template <class T>
    constexpr auto pure(this auto &&, T value) {
        return smd::kit::parser::pure(std::move(value));
    }

    template <class F, class G>
    constexpr auto bind(this auto &&, parser<F> p, G g) {
        return parser{[p = std::move(p),
                       g = std::move(g)](cursor cur, parse_context auto &ctx) {
            return foundation::bind(p(cur, ctx), [&g, &ctx](auto const &state) {
                return g(state.value)(state.rest, ctx);
            });
        }};
    }
};

/// Monad instance map for @ref parser.
struct parser_monad_map : foundation::monad<parser_monad_impl> {
    using parser_monad_impl::bind;
    using parser_monad_impl::pure;
};
```

Success and failure branches aren't written out a second time. `p(cur, ctx)` is already a `foundation::result`, so sequencing it through `foundation::bind` is what skips `g` on a failed parse. The parser's own `bind` is that one call plus the threading. Its inner lambda captures `&g` and `&ctx` by reference, which is safe for exactly one reason: `result`'s bind is strict and runs its function before returning. The outer lambda, the one that gets stored, takes `p` and `g` by value and `ctx` as a parameter.


# The namespace it could not be declared in

The brief's illustration of the registration ends like this, inside `namespace smd::kit::parser`, where the rest of the sketch sits:

```cpp
template <class F>
inline constexpr auto foundation::monad_typeclass<parser<F>> = parser_monad_map{};
```

It does not compile. A variable template's partial specialization has to be declared in a namespace that encloses the primary template's own. `monad_typeclass` is declared in `smd::kit::foundation`, and `smd::kit::parser` is that namespace's sibling. Neither one encloses the other. Qualification doesn't help, because the problem isn't finding the name. `result<T>` never showed this, because `result` lives inside `smd::kit::foundation` with the typeclass it registers against.

The step confirmed that against GCC 16 directly instead of inferring it, and the handoff to the next one says so in those words. It also found the shape already in the tree: `src/smd/cl/foundation/tagged_tree_instances.hpp` registers a `cl` type against a kit-owned typeclass and has the same sibling problem one level down. What landed is that shape.

```cpp
namespace smd::kit::foundation {

/// Registers the Monad instance for @ref smd::kit::parser::parser.
template <class F>
inline constexpr auto monad_typeclass<smd::kit::parser::parser<F>> =
    smd::kit::parser::parser_monad_map{};

} // namespace smd::kit::foundation
```

Note where the anchor stops. `606983ed` closes at the end of `parser_monad_map`, before the closing brace of `namespace smd::kit::parser`, so the block above is outside it. The architecture section transcludes the part that composes, and explains the part that doesn't in prose underneath. A reader of that section sees the `Impl` and the instance map as live code, and the declaration the whole step turned on as a description of one. Which is the wrong way round.


# A check with nothing to match

The step's spot-check block is six greps, and the first is `grep -n 'monad_typeclass<parser' src/smd/kit/parser/parser_instances.hpp`, with the instruction "the first must find the registration." It finds nothing. What landed spells the type fully qualified, so the character after `<` is an `s`.

The step recorded that instead of quietly working around it. Its metrics row ends with a note saying the check "cannot match a syntactically valid registration for this cross-namespace case", and that the substance was verified instead with `grep monad_typeclass<` and a passing build.

The reasoning there goes one spelling too far. Inside `namespace smd::kit::foundation` the name `parser` is found by ordinary lookup in the common enclosing `smd::kit`, so

```cpp
template <class F>
inline constexpr auto monad_typeclass<parser::parser<F>> = parser::parser_monad_map{};
```

declares the same specialization, compiles, and matches the grep. GCC 16 accepts it against this step's own headers. Declare both spellings in one translation unit and you get a conflicting-declaration error, which is the compiler saying they name the same specialization. So the check is unmatchable against the registration that landed, not against every registration that could have. And the one that landed is the one with a precedent in the tree.

The failure mode is the opposite of the one phase 38 spent a section on. There a glob matched nothing, printed nothing, exited 0, and read exactly like a pass. Here a check told to find something found nothing, in front of whoever was watching the terminal, which is presumably why this one got written down.

The sixth check has the quieter problem. `grep -n 'and_then' src/smd/cl/reader/detail/sharpsign.hpp` is described as showing `read_radix_number` no longer reaching for `and_then` while "`read_sharpsign`, still unconverted, may." It returns nothing at all. `read_sharpsign` never had an `and_then`: it computes the infix argument, then switches on `ctx.table.sharp_of` with an early return in every arm.


# The first client

`read_radix_number` is the right first consumer for two reasons. The radix it parses from the prefix decides how the following token is classified, which is the shape `lift2` can't express. And it calls nothing recursive. Converting it needs one `cl`-level parser that lifts the untouched `scan_token` into the layer.

```cpp
/// The whole-token scan, lifted into the parser layer. @c scan_token itself
/// is untouched -- DIV-0003 holds because a whole token is classified at
/// once, and this only lifts that function's existing result into
/// @c parser<F>'s vocabulary rather than replacing it.
inline constexpr auto token_p =
    smd::kit::parser::parser{[](cursor cur, reader_context auto &ctx) {
        return scan_token(cur, ctx.table);
    }};
```

The function's body becomes `bind(token_p, classify_and_add)(cur, ctx)`. What changed underneath that is the capture list. Before, the continuation was `[&]`, and it pulled in `ctx`, `radix`, `where` and the scanned token together. Now `radix` and `where` go in by value, the scanned token is copied, and `ctx` arrives as a parameter of the parser that comes back. The scoping note ratified before any of this started says a parser carries its context by construction rather than by capture. Here that's a property of the code instead of a line in a decision record.

Cursor arithmetic went the same way. Where the old code closed with `parse_state<int>{id, token.rest}`, reaching back into the token it had captured, the converted one closes with `parse_state<int>{id, rest}`, the cursor `bind` handed it. Same cursor. One fewer thing to get wrong by hand.

Two smaller things fell out of the conversion. `scan_token`'s call site left `sharpsign.hpp` and reappeared inside `token_p` in `read_context.hpp`. The reader still calls it exactly twice, so the spot check that greps `detail/*.hpp` sees the count it saw before. And an `and_then` that had nothing to do with parsers (the one sequencing `add_leaf_checked`'s `result<int>`) became a `bind`. Both names reach `result`'s registered instance, so that line means what it meant before it was retyped.


# Three primitives, one caller

The commit message says "pure, map and bind and nothing else, because read\_radix\_number is what needed them." `read_radix_number` needed `bind`.

`pure` has a structural caller: `monad<Impl>` requires an `Impl` to supply it, so `parser_monad_impl::pure` has to forward to something. `map` has none. Across the whole tree at this tag, every call to `map` on a parser is in `parser.test.cpp`, proving the Functor laws it exists to satisfy. `parser_like` has no caller at all. The handoff to the next step says so without being asked: "nothing constrains anything with it, so it is currently decoration, not a caught mistake", and adds that `reader_context` constrains one function and hasn't yet caught a real argument-order bug either.

`parse_context` is honest about naming a role rather than constraining a shape. Its test file checks nothing but membership: `no_context`, a small example struct and `int` all model it, and `cursor` in three spellings does not.

The rule the plan keeps stating is that a primitive lands with its first consumer and never ahead of one. An abstraction that lands five steps before its first client is five steps of guessing that nothing checks. One of the three cleared that bar. The brief is careful about why `map` isn't registered as a `functor_typeclass` instance. And the architecture section marks the primitive set provisional, in bold. Nothing here is being smuggled. It is still two thirds of a set arriving on a guess.

What is not a guess is the laws.

```cpp
// --- Monad laws, over no_context. -------------------------------------

static_assert([] {
    no_context ctx{};
    return same_parse(bind(pure(5), double_p), double_p(5), "a", ctx);
}());

static_assert([] {
    // Right identity must also hold on the failing branch.
    no_context ctx{};
    auto const pure_p = [](int n) { return pure(n); };
    return same_parse(bind(char_val, pure_p), char_val, "a", ctx) &&
           same_parse(bind(char_val, pure_p), char_val, "", ctx);
}());

static_assert([] {
    no_context ctx{};
    auto const lhs = bind(bind(char_val, double_p), succ_p);
    auto const rhs =
        bind(char_val, [](int n) { return bind(double_p(n), succ_p); });
    return same_parse(lhs, rhs, "a", ctx) && same_parse(lhs, rhs, "", ctx);
}());

// --- The same three laws, over a context the parsers actually read. -------

static_assert([] {
    scale_context ctx{3};
    return same_parse(bind(pure(5), double_p), double_p(5), "a", ctx);
}());

static_assert([] {
    scale_context ctx{3};
    auto const pure_p = [](int n) { return pure(n); };
    return same_parse(bind(scaled_char, pure_p), scaled_char, "a", ctx) &&
           same_parse(bind(scaled_char, pure_p), scaled_char, "", ctx);
}());

static_assert([] {
    scale_context ctx{3};
    auto const lhs = bind(bind(scaled_char, double_p), succ_p);
    auto const rhs =
        bind(scaled_char, [](int n) { return bind(double_p(n), succ_p); });
    return same_parse(lhs, rhs, "a", ctx) && same_parse(lhs, rhs, "", ctx);
}());

// --- bind does not invoke its continuation on a failed value. --------------

static_assert([] {
    no_context ctx{};
    bool invoked = false;
    auto const probe = [&invoked](int n) {
        invoked = true;
        return pure(n);
    };
    auto const r = bind(char_val, probe)(cursor{""}, ctx);
    return !invoked && !r.has_value();
}());
```

Three laws twice over: once against `no_context`, once against a context a test parser actually reads. A law that only holds for the empty context isn't the law. Right identity and associativity get the failing input as well as the succeeding one. Left identity starts from a `pure`, which has no failing input to give it. The last block isn't a law at all. It proves that `bind` doesn't run its continuation on a failed parse, which is what everything else in the file assumes.


# What the base class already said about parsers

`monad.hpp` derives `join`, `then` and `apply` from `bind` and `pure`, and its class comment says what that assumes:

> The derivations assume `bind` invokes its function before returning, which holds for every strict instance and is the same assumption `applicative`'s derivations already make. An instance that defers its continuation instead of running it &mdash; a parser that stores what to do next &mdash; has to supply these operations itself rather than inherit them.

That sentence predates this step. It was written for a hypothetical parser, by whoever added the typeclass. This step built one. `parser_monad_impl::bind` returns a `parser` with the continuation stored inside it, and runs nothing. `parser_monad_map` derives from `monad<parser_monad_impl>`, re-exports `bind` and `pure`, and supplies none of the three.

`join` and `then` survive the inheritance anyway. `join` captures nothing, and `then` captures its second operand by value, with a comment pointing at a dangling-reference bug this project has already hit once. `apply` is the one the warning is about. It captures `self` and its argument operand by reference, and `self` is the `tc_type{}` the CPO builds as a temporary for the duration of the call. Nothing calls `join`, `then` or `apply` on a parser, not in this step and not anywhere in the tree at this tag, so nothing has gone wrong. The instance is one caller away from the thing its own base class told it not to do.


# The numbers

1474 seconds, one attempt, green on both legs of the build matrix, verification 53 seconds against a 114-kilobyte log. Seventeen files, 1004 insertions, 116 deletions. Twenty-one new `ctest` entries across four test files, and twenty-four ~static\_assert~s among them, because the project wants the compile-time contracts and the runtime cases both.

The header split before it came in at 695 seconds, and the plan calls that this phase's floor: what a step costs before it does any thinking. This one is a little over twice that, for a new module, a registration with a namespace problem in it, one converted function and a shim.

Two follow-ups outside the step's own diff. `src/smd/cl/reader/CMakeLists.txt` gained `kit.parser` in `target_link_libraries`, which the shim and `token_p` both need. The metrics row records it as outside the step's declared scope, with a sentence saying why, so a new edge in the build graph doesn't arrive silently. And `make lint` went red on a fresh checkout and green on the rerun, so the formatting the hooks had already applied never reached the commit. `18afb36` is that repair, and it says it is formatting only. Strip all the whitespace out and each of the four files hashes identically before and after. Checkable in about one line, and it holds. It is the third step in this plan to merge that way. A backlog item was filed the same day, to say that a warning in each worker's briefing has not been enough. The failure mode is that lint *passes* on the run the worker looks at.

A slot in the divergence log was reserved for this step before it ran, for whatever deliberate departure from the standard or from the plan it might need. Nothing went into it.

One last count, since the whole step is an argument about how much hand-written bind there was to begin with. The commit message and the brief both say `read.hpp` used `and_then` "fifteen times." The pre-split file has the string on seventeen lines, four of which are the doc comment and the using-declaration, leaving thirteen calls. Same thirteen on the day the plan was committed. Nothing rests on it, since thirteen nested continuations is plenty of argument, but fifteen is neither number.

The kit has a Monad instance, a registration that had to be declared somewhere else, and one caller. Six steps left to find out whether the other two primitives were the right ones.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 39 - Eight Headers, and a Comment the Formatter Made False](phase-39-eight-headers.md)

</nav>


# References
