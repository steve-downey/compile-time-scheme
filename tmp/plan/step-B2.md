# Step B2 — `smd::kit::parser`, a Monad instance over a threaded context

## Project context

`compile-time-scheme` is a compile-time compiler proof of concept in C++26 on
GCC16. After Phase A there is one front end, `src/smd/cl/`, over one shared
substrate, `src/smd/kit/`. B1 split `src/smd/cl/reader/read.hpp` into eight
headers under `src/smd/cl/reader/detail/` plus an umbrella.

**Integration branch: `cl-parser-combinators`.**

**Reserved for this step:** blog phase **40**, divergence number **DIV-0036**.

## Phase B's standing constraints

- No observable behaviour change: no new syntax, no changed diagnostic text, no
  different error position.
- DIV-0003 must not regress; `1+` reads as the symbol `1+`.
- `scan_token` and `classify_number` are **out of scope for all of Phase B**. A
  parser may call them; neither may be rewritten. That is what makes DIV-0003
  structurally unbreakable rather than merely tested against.
- Existing reader tests and the conformance corpus pass unchanged. Adding a
  case is fine; **changing an existing expectation is a halt**.

## Why

`cl`'s reader is already written in the combinator paradigm without its
vocabulary. `src/smd/cl/reader/cursor.hpp` defines its own `parse_state<T>` —
`T value; cursor rest;` — and every reader function returns
`foundation::result<parse_state<int>>`, which is a `parse_result<int>` by
another name. `read_wrapped` is nested `and_then` continuations, which is
monadic bind written out by hand. The header uses `and_then` fifteen times.

The combinator layer this repository already had was **Applicative and
Alternative, not Monad**: `pure`, `satisfy`, `char_p`, `map`, `lift2`,
`sequence_left`, `sequence_right`, `operator|`, `alt`, `many`, `some`,
`optional`, and nothing named `bind`, `and_then` or `flat_map`. That sufficed
for Scheme, whose reader never needs the value of one parse to choose the next.
Common Lisp's is context-dependent in at least four places — `#nnR` reads a
radix that then determines how the following token is read, `#\name` looks up a
character name, sharpsign sub-dispatch selects on the character after `#`, and
every datum position dispatches through the readtable. `lift2` fixes both
parsers before either runs, so Applicative cannot express any of them.

So adopting combinators here means **adding bind**, and that is the thing that
makes the layer a general parser kit rather than a Scheme-shaped one.

**`bind` does not have to be invented here — it has to be an instance.**
Since this plan was first written, `src/smd/kit/foundation/monad.hpp` landed
(`ae3c33a`): a `monad<Impl>` CRTP base, a `monad_typeclass<T>` lookup, and a
`bind` CPO, with `result<T>` already registered against it in
`result_instances.hpp`. So `parser<F>` does not get a free-standing
`bind(p, f)` that reimplements the pattern — it gets registered as a second
Monad instance the same way, in a new `parser_instances.hpp`, and every
caller reaches it through the same generic `bind` CPO `result` uses. Section
1 gives the exact shape; "What already exists" below names the file to copy.
The risk this guards against is someone reaching for a second, parallel
`bind` because it looks like less work than registering an instance — that
is exactly the forked abstraction this plan's amendment rule exists to catch
before it happens.

This step builds the minimum that has a consumer today and stops. It does not
build the whole layer. `satisfy`, repetition and choice arrive in B3 and B4,
each with the function that needs it, because an abstraction placed far from
its first real consumer maximises the number of guesses validated too late.

## The context decision, already made — do not re-litigate

**D29 is answered: thread the context.** A parser is
`parse_result<T>(cursor, Ctx&)` over a **named** `Ctx` concept, not
`parse_result<T>(cursor)` with `&ctx` captured in the lambdas.

The reason is lifetime safety by construction. Combinators reify parsers into
storable values, so a captured `&ctx` can outlive its context in a way the
current per-call `read_node(cur, ctx)` cannot.
`docs/divergences/DIV-0007-closure-program-datum-arena-caller-owned.md` is this
project's own record of that bug class already paid for once — GCC caught a
dangling `string_view` with `accessing 'arena_dr' outside its lifetime`, and R0
classified it a `defect`. Capture is the cheaper option and the un-checkable
one; threading is more churn now and is correct by construction, which is the
criterion the whole change rests on.

`no_context` may exist on its own merits, for a parser that genuinely needs
none. It is **not** a backward-compatibility default and must not be given as a
default template or function argument. There is no backward compatibility to
keep: `cl` is the layer's only client.

## Setup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree add ../step-b2-kit-parser -b step-b2-kit-parser cl-parser-combinators
cd ../step-b2-kit-parser
git submodule update --init --recursive
```

## Verify GREEN baseline

```sh
make test-matrix > /tmp/verify-B2-base.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-B2-base.log
```

Not green ⇒ `blocked-B2.md`.

## What already exists that this step builds on

- `src/smd/cl/reader/cursor.hpp` — `cursor` (immutable, `peek`/`bump`/
  `position`/`remaining`/`empty`), `parse_state<T>`, and `advance_while`. It
  includes only `<smd/cl/foundation/source_pos.hpp>` and two standard headers,
  so it is already free of anything language-specific.
- `src/smd/kit/foundation/result.hpp` — `result<T>` and `and_then`.
  `src/smd/cl/foundation/result.hpp` is an R8 forwarding shim onto it; read
  that file's header comment for the shim pattern this step reuses.
- `src/smd/cl/foundation/parse_error.hpp` — `parse_error{where, message}`.
- `src/smd/cl/reader/detail/sharpsign.hpp` (from B1) — `read_radix_number` and
  `read_sharpsign`.
- `src/smd/cl/reader/token.hpp` — `scan_token(cursor, readtable const&) ->
  result<parse_state<token_text>>`. **Out of scope.** Note its return type: it
  is already a `parse_result<token_text>`, which is why it lifts into the layer
  without being touched.
- The retired layer, for reference only, at
  `git show iteration/smdscheme-final:src/smd/smdscheme/parser/parser.hpp`.
  Read it for the shape of `map`, `lift2` and `operator|`. Do not copy
  `lexeme` from `alt.hpp` — it calls a Scheme-specific `skip_intertoken_space`
  that only skips whitespace and knows nothing about comments.
- `src/smd/kit/foundation/monad.hpp` — `monad<Impl>`, `monad_typeclass<T>`,
  and the `bind`/`join` CPOs. Read its doc comments for what `Impl` must
  provide.
- `src/smd/kit/foundation/result_instances.hpp` — **the pattern to copy, not
  background reading.** `result_monad_impl`/`result_monad_map` is the
  `Impl` struct and instance type; `monad_typeclass<result<T>> =
  result_monad_map{}` is the registration; `and_then` below it is a
  convenience spelling that must live in this file rather than in
  `result.hpp`, because calling it from `result.hpp` would instantiate the
  primary (unspecialized) `monad_typeclass<result<T>>` before the
  specialization is visible everywhere — an IFNDR trap `ae3c33a` hit and
  fixed by co-locating the two. Section 1 sidesteps the same trap for
  `parser<F>` by not writing a convenience spelling at all.
- `src/smd/cl/foundation/monad.hpp` — a pure forwarding shim, nothing more;
  it never had a `cl`-side definition, so there is no implementation
  pattern to find here. It is the eighteenth candidate on
  `docs/backlog/BL-0004-retire-the-cl-foundation-forwarding-shims.md`.
- `docs/compiler_architecture.org` § "The Common Lisp Rebuild: =smd::cl=",
  where B1 recorded the reader's new file layout.

## The change

### 1. `src/smd/kit/parser/`, a new kit module

`src/smd/kit/parser/CMakeLists.txt` defines an `INTERFACE` library
`kit.parser` with `FILE_SET kit_parser_headers`, linking `kit.foundation`, plus
a `kit_parser_test` executable under `if(SCHEMEPOC_ENABLE_TESTING)`. Model it
on `src/smd/kit/foundation/CMakeLists.txt`. Add
`add_subdirectory(parser)` to `src/smd/kit/CMakeLists.txt`.

**Do not add `kit.parser` to the top-level `beman_install_library` export
list.** A2 shrank that list to `smd.fixpoint` alone; exporting the live
trees is `docs/backlog/BL-0005-…`, scheduled after this whole phase.
`kit.parser` builds and is tested via `add_subdirectory` like every other
module here; it is simply not part of the *installed* package yet. If
`CMakeLists.txt`'s `TARGETS` list already names `kit.foundation` or any
`cl.*` target, BL-0005 landed early and out of order — say so in your
handoff rather than adding to it.

**`src/smd/kit/parser/cursor.hpp`** — move `cursor`, `parse_state<T>` and
`advance_while` here as `smd::kit::parser`, and leave
`src/smd/cl/reader/cursor.hpp` as an R8-style forwarding shim: include the kit
header, `using smd::kit::parser::cursor;` and the other two names, and a header
comment saying what moved and why, exactly like
`src/smd/cl/foundation/source_pos.hpp` does. Add the new shim to the list in
`docs/backlog/BL-0004-retire-the-cl-foundation-forwarding-shims.md`, which
tracks these deliberately.

A shim rather than repointing four callers, because D31's acceptance witness is
that the existing tests pass **unchanged**, and `read.test.cpp` and
`cursor.test.cpp` both spell `smd::cl::reader::cursor`. Keeping the name alive
keeps that claim checkable by diff.

**`src/smd/kit/parser/parse_context.hpp`** — the named concept D29 asks for,
and `no_context`. The kit cannot require anything language-specific of a
context, so keep the concept honest about what it is for: it names the
parameter's role and catches an argument-order mistake, rather than pretending
to constrain the type. Something along the lines of "an object type that is not
a `cursor`" is checkable and useful; a vacuous `concept parse_context = true`
is not, and would be worse than no concept at all. `no_context` is an empty
struct modelling it.

**`src/smd/kit/parser/parser.hpp`** — the type, `pure`, and `map` only. No
`bind` here; section below explains why.

```cpp
template <class T>
using parse_result = foundation::result<parse_state<T>>;

template <class P, class Ctx>
concept parser_like = requires(P p, cursor c, Ctx &ctx) { p(c, ctx); };

template <class F>
class parser {
  public:
    constexpr explicit parser(F f);
    template <parse_context Ctx>
    constexpr auto operator()(cursor cur, Ctx &ctx) const;
  private:
    F f_;
};
template <class F> parser(F) -> parser<F>;

template <class T> [[nodiscard]] constexpr auto pure(T value);
template <class P, class F> [[nodiscard]] constexpr auto map(P p, F f);
```

`pure` stays a plain, implementor-facing free function, per `monad.hpp`'s own
doc comment: it cannot be dispatched from its argument, so a CPO would have
nothing to key on. `map` stays a plain free function too, deliberately
**not** a registered `functor_typeclass<parser<F>>` instance — nothing
outside this step needs a generic `fmap` over a parser value, and a
typeclass instance with no second caller is the R8/DIV-0028 over-eagerness
this project already learned to avoid. A real second caller is an
amendment; this step is not one.

**`src/smd/kit/parser/parser_instances.hpp`**, new — `bind`, registered as a
Monad instance, in exactly the shape `result_instances.hpp` uses for
`result<T>`:

```cpp
struct parser_monad_impl {
    template <class T>
    constexpr auto pure(this auto &&, T value) { return smd::kit::parser::pure(std::move(value)); }

    template <class F, class G>
    constexpr auto bind(this auto &&, parser<F> p, G g) {
        return parser{[p = std::move(p), g = std::move(g)](cursor cur, parse_context auto &ctx) {
            return foundation::bind(p(cur, ctx), [&](parse_state<auto> const &state) {
                return g(state.value)(state.rest, ctx);
            });
        }};
    }
};

struct parser_monad_map : foundation::monad<parser_monad_impl> {
    using parser_monad_impl::bind;
    using parser_monad_impl::pure;
};

template <class F>
inline constexpr auto foundation::monad_typeclass<parser<F>> = parser_monad_map{};
```

(This is the shape, not a literal transcription — the inner `foundation::bind`
call is over `foundation::result<parse_state<T>>`, already a registered
Monad instance, so the parser's own `bind` is implemented *in terms of* the
result instance one level down. Work out the exact template parameters;
`parse_state<auto>` above is illustrative, not valid C++.) There is no
parser-domain-specific spelling analogous to `and_then` — every caller,
inside this codebase or out, reaches `bind` through the same CPO `result`
uses, qualified or brought into scope with a `using` declaration (part 3
below, and the law-test note just after this, both need this and say why).

Follow the declarations-before-definitions and out-of-line-and-fully-qualified
rules in `docs/cpp-rules.md`. Everything `constexpr`.

**`src/smd/kit/parser/parser.test.cpp`**, **`parser_instances.test.cpp`**,
and `parse_context.test.cpp` and `cursor.test.cpp` — double-include,
bootstrap test, then **law tests first**, per `docs/CODING_RULES.md`'s "add
law-focused tests before performance tests" and `docs/cpp-rules.md`'s "law
tests … come before any other substantive test":

- Functor: `map(p, id) == p`; `map(map(p, f), g) == map(p, compose(g, f))`.
- Monad left identity: `bind(pure(a), f) == f(a)`.
- Monad right identity: `bind(m, [](auto x){ return pure(x); }) == m`.
- Monad associativity: `bind(bind(m, f), g) == bind(m, [](auto x){ return bind(f(x), g); })`.

`bind` above is `smd::kit::foundation::bind`, the CPO — bring it into scope
in `parser_instances.test.cpp` with `using smd::kit::foundation::bind;`.
**A CPO is an object, not a function template, so ADL does not find it** —
unlike a function template, which an unqualified call finds via the
namespace of its argument's type. The call must be qualified or the name
brought in by a `using` declaration, every place `bind` is called on a
`parser<F>`. `ae3c33a`'s own commit message names this fact for `result`'s
`and_then`; it is more exposed here because `parser` has no pre-existing
domain name like `and_then` to hide the CPO behind. Part 3 below hits the
same requirement for the reader's own call sites.

Two parsers are equal when they produce the same `parse_result` — same value,
same rest cursor, same error and error position — for the same input and
context. Write that comparison once as a helper and use it in every law.
Exercise the laws with a non-trivial context, not only with `no_context`; a law
that only holds for the empty context is not the law.

`static_assert` every one of them. These are `constexpr` contracts and the
project wants compile-time tests alongside the runtime ones, both.

### 2. `reader_context`, the `cl` side of the concept

`Ctx` in the reader is duck-typed today, reached through
`typename Ctx::child_list`. Give it a name in
`src/smd/cl/reader/detail/read_context.hpp`: a `reader_context` concept that
refines `kit::parser::parse_context` and requires the members the reader
actually uses — `ctx.tree`, `ctx.symbols`, `ctx.table`, and the nested
`child_list`. Then constrain `read_radix_number` with it in part 3.

Do **not** constrain the other ten functions here. Each converting step
constrains the ones it converts, so a wrong guess about the concept surfaces
against one function rather than eleven.

### 3. The first real consumer: `read_radix_number`

It is in `src/smd/cl/reader/detail/sharpsign.hpp` and it is the right first
consumer because it is genuinely context-dependent — the radix parsed from the
prefix determines how the following token is classified, which is the exact
thing Applicative cannot express — and because it calls nothing recursive.

Its current shape is `and_then(scan_token(cur, ctx.table), λ)` where λ
classifies and appends. Convert it to run through `bind`. You will need a small
`cl`-level parser that lifts `scan_token` into the layer:

```cpp
/// The whole-token scan, as a parser. scan_token itself is untouched --
/// DIV-0003 holds because a whole token is classified at once, and this
/// lifts that function rather than replacing it.
inline constexpr auto token_p = parser{
    [](cursor cur, reader_context auto &ctx) { return scan_token(cur, ctx.table); }};
```

Put it wherever it reads best among B1's headers — probably
`detail/read_context.hpp`, since B5, B6 and B7 will all want it — and say in
your handoff where you put it, because three later steps depend on finding it.
Put `using smd::kit::foundation::bind;` in the same header, next to the
existing `using foundation::and_then;` this reader already carries, so every
converting step after this one writes `bind(token_p, …)` unqualified instead
of rediscovering the CPO-is-an-object qualification rule for itself.

The converted function must return the same `result<parse_state<int>>`, produce
the same values, and produce the **same diagnostic strings at the same
positions**: `"expected a rational after radix prefix"` in two places. The
existing tests in `read.test.cpp` pin those exactly.

### 4. Anchors and the architecture doc

Anchor `bind` and its law tests, and `token_p`. Add a section to
`docs/compiler_architecture.org` for `smd::kit::parser` recording: that the
layer is Monad and not merely Applicative and why; that the context is threaded
rather than captured, with the DIV-0007 reason; and — **marked provisional** —
that the primitive set is `pure`/`map`/`bind` only because that is what
`read_radix_number` needed, and that a consumer needing `satisfy`, choice or
repetition is what justifies adding it. An abstraction nobody dared touch and
one nobody needed to touch look identical from outside, and saying which this
is now is the cheapest thing in the plan that prevents a later step forking it.

## Declared file scope

```
src/smd/kit/parser/CMakeLists.txt            (new)
src/smd/kit/parser/cursor.hpp                (new; moved from cl/reader)
src/smd/kit/parser/cursor.test.cpp           (new)
src/smd/kit/parser/parse_context.hpp         (new)
src/smd/kit/parser/parse_context.test.cpp    (new)
src/smd/kit/parser/parser.hpp                (new)
src/smd/kit/parser/parser.test.cpp           (new)
src/smd/kit/parser/parser_instances.hpp      (new; the Monad registration)
src/smd/kit/parser/parser_instances.test.cpp (new; the law tests)
src/smd/kit/CMakeLists.txt                   (add_subdirectory)
src/smd/cl/reader/cursor.hpp                 (becomes a forwarding shim)
src/smd/cl/reader/detail/read_context.hpp    (reader_context concept; token_p; using bind)
src/smd/cl/reader/detail/sharpsign.hpp       (read_radix_number only)
docs/backlog/BL-0004-...md                   (one shim added to the list)
docs/compiler_architecture.org               (new section)
```

`CMakeLists.txt` (the top-level export list) is **not** in scope — see the
note in section 1 above; exporting `kit.parser` is `docs/backlog/BL-0005-…`,
not this step. `src/smd/cl/reader/cursor.test.cpp` is **not** in scope
either: the shim keeps the name alive precisely so it does not have to
change.

## Verify GREEN after

```sh
make test-matrix > /tmp/verify-B2-after.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-B2-after.log
wc -c /tmp/verify-B2-after.log
make compile-headers
make lint
./scripts/verify-transclusions.sh
```

Count above baseline — you added law tests and removed none. Both legs green.

## Spot checks

```sh
git diff --name-only cl-parser-combinators..HEAD | grep 'reader/.*\.test\.cpp$'
```

Must return nothing: no reader test changed.

```sh
grep -n 'monad_typeclass<parser' src/smd/kit/parser/parser_instances.hpp
grep -n 'constexpr auto bind' src/smd/kit/parser/parser.hpp
grep -c 'static_assert' src/smd/kit/parser/parser_instances.test.cpp
grep -n 'no_context' src/smd/kit/parser/*.hpp | grep -i 'default\|= no_context'
grep -n 'scan_token' src/smd/cl/reader/detail/*.hpp
grep -n 'and_then' src/smd/cl/reader/detail/sharpsign.hpp
```

The first must find the registration. The second must find **nothing** —
`bind` is not a free function in `parser.hpp`; if it is, the instance was
forked instead of registered. The fourth must return nothing — `no_context`
is never a default. The fifth still shows `scan_token` called, unmodified.
The sixth shows `read_radix_number` no longer reaching for `and_then`
directly; `read_sharpsign`, still unconverted, may.

## Commit and merge back

```sh
git add -A
git commit -F - <<'EOF'
kit: a parser as a Monad instance, and its first Common Lisp client

cl's reader was already written in the combinator paradigm without the
vocabulary: its own parse_state, every function returning a
result<parse_state<T>>, and read_wrapped built from nested and_then --
monadic bind written out by hand, fifteen times across one header.

The layer this repository had was Applicative and Alternative and had
nothing named bind. That was enough for Scheme, whose reader never needs
the value of one parse to choose the next. Common Lisp's does, in at
least four places, and lift2 fixes both parsers before either runs. So
the interesting part of adopting combinators here is adding bind.

kit::foundation grew a Monad typeclass since this plan was first
written (ae3c33a), with result already registered against it. So
bind is not invented here as a free function on parser<F>; parser
becomes a second registered instance, in parser_instances.hpp,
alongside result's in the same shape -- an Impl struct supplying
bind and pure, a monad_typeclass<parser<F>> specialization, and every
caller reaching it through the same generic bind CPO result already
uses. Forking a second bind that happened to look like the CPO would
have been worse than not adding one.

The context is threaded, not captured. Combinators reify parsers into
values a captured &ctx can outlive, and DIV-0007 is this project's
record of paying for that class of bug once already. no_context exists
but is never a default argument -- there is no compatibility to keep,
because cl is the only client.

pure, map and bind and nothing else, because read_radix_number is what
needed them and it is converted here rather than five steps later. The
radix parsed from a prefix decides how the next token is classified,
which is precisely the shape Applicative cannot express, so the first
consumer is one that would have caught a wrong guess immediately.
scan_token is untouched and stays that way for the whole series.
EOF

git checkout cl-parser-combinators
git merge --no-ff step-b2-kit-parser
```

## Record measurements

```sh
cat >> /home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/metrics.jsonl <<EOF
{"step":"B2","lane":null,"outcome":"green","wall_seconds":<measured>,"attempts":<n>,"verify":{"command":"make test-matrix + compile-headers","exit_code":0,"wall_seconds":<measured>,"log_bytes":$(wc -c < /tmp/verify-B2-after.log),"summary_lines_read":<n>},"diff":{"files_changed":<n>,"insertions":<n>,"deletions":<n>},"out_of_scope":[],"note":""}
EOF
```

## Cleanup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree remove ../step-b2-kit-parser
```

Mark B2 done in
`/home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/checklist.md`.

## Handoff

Read `tmp/plan/step-B3.md`, then write `tmp/plan/handoff-B3.md` (≤ ~150 lines).
B3 adds repetition and converts the intertoken-space skippers. Tell it: where
`token_p` landed; the exact spelling of `bind`, `parser_like` and
`parse_context`; whether the concept caught anything real or is currently
decoration; how you compared two parsers for the law tests, since B3 needs the
same helper; and any compile-time cost you noticed, because decision D30 asks
for before-and-after numbers and B8 collects them.
