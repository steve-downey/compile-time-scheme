# Handoff to B5

## Exact spellings, as B4 left them

`src/smd/kit/parser/choice.hpp`: `operator|(parser<PA>, parser<PB>)`, `alt` (named
alias), `optional(P)`. Both operands of `operator|` must already be `parser<F>`
values (wrap a raw lambda in `parser{...}` first, as `char_p`/`satisfy` do).

`src/smd/kit/parser/repeat.hpp`: `skip_many` (B3, unchanged) plus B4's
`many_until(P step)`. `step` returns `foundation::result<parse_state<std::optional<T>>>`:
`std::nullopt` ends the repetition successfully at that step's own rest cursor;
any other failure propagates verbatim; a value is otherwise **discarded** by
`many_until` itself.

## `many_until` does not take a capacity or a message — decide this deliberately for `read_string`

This is the fact most likely to surprise you. Unlike the retired iteration's
`many<Capacity>`, B4's `many_until` owns no container and no capacity: it is
control-flow only ("keep asking until told to stop or handed a failure"), the
same division of labour `skip_many` has with its own inner parser. The caller's
`step` closure owns the accumulator, the capacity check, and the overflow
message/position — see `read_delimited` in `forms.hpp` for the pattern (a
`static_vector`-shaped local, captured by reference, pushed to inside `step`
after a `size() >= capacity()` check).

Why it's shaped this way, so you don't have to re-derive it: `read_delimited`
needs its "stop" condition (closing delimiter) checked *ahead of* "keep going"
(one element) every turn, on an *already-skipped* cursor, and needs the
overflow position to be that same post-skip position. Both come out exact only
when the same closure that has the skipped cursor in scope also owns the
capacity check. Hoisting collection into a generic `many_bounded<Capacity>`-
shaped kit primitive was considered and rejected for exactly this reason — see
`docs/compiler_architecture.org` § "B4: choice, and `read_delimited`'s last raw
loop becomes a repetition" for the full argument, and the two failure-position
traps (intertoken skip inside vs. outside the choice) it walks through.
`read_string`'s `"string too long"` check is the same shape (position at the
opening quote, not at the overflowing character, per your own step file) — read
that section before assuming `many_until` alone gets you there; you will likely
want the same "step owns the check" pattern, not a kit change.

## `operator|` on a double failure: the second alternative's error survives

Tries `pa`; if `pa` fails at the exact position it started from, tries `pb`
from that same cursor; otherwise `pa`'s result (success or a committed
failure) stands. On a double failure, `pb`'s error — whatever it is — is what
survives, never a synthesized "expected one of ...". This is now a tested law
(`choice.test.cpp`), not only documented. If you use `|` for `read_string`'s
"escape, or any non-quote character" alternative, put whichever message you
want to survive last in the chain.

## `choice.hpp` has no production caller yet

B4 built `operator|`/`alt`/`optional` with law tests and landed the diagnostics
assessment `docs/cl-parser-scoping.md` § 5 asks for, but did **not** wire it
into `read_delimited` — the intertoken-skip/position interaction made that
unsafe without either duplicating the skip on both sides or hoisting it out
(both break a pinned diagnostic position; see the architecture-doc section
above). `read_string`'s two-alternative body (escape vs. ordinary character)
does not have `read_delimited`'s skip-then-choose shape, so `|` may compose
cleanly there where it didn't for B4 — you'd be its first real consumer.

## The zero-consumption guard

`many_until` guards a step that succeeds without advancing the cursor the same
way `skip_many` does (B3): stops there rather than looping forever. No known
caller's step actually has this shape (every branch of `read_delimited`'s own
step consumes at least one character on success) — decide explicitly whether
`read_string`'s repeated character-or-escape step can ever succeed at
zero-width, and say so in your own handoff, the same question B4 was asked and
answered for its own step.

## `reader_context` unchanged

Still `smd::kit::parser::parse_context` plus `.tree`/`.symbols`/`.table`/
`::child_list`. Four functions now constrain with it (`read_radix_number`,
the two skippers indirectly via `readtable const`, `read_delimited`); nothing
needed adding for B4's step.

## Addendum, typeclass-resync, 2026-09-10

Everything B4 wrote above still holds — `operator|`, `many_until`, and the
zero-consumption guard are unchanged. This addendum records only what the
out-of-band resync moved under you.

### The branch now carries `main` up to `d6ae364`

`cl-parser-combinators` had branched at `6099a84`; `main`'s typeclass pickup
merged cleanly, and the branch head is now the `--no-ff` merge `d8f2965`.

Your call sites are unchanged: `bind` and `pure` are spelled exactly as B2–B4
left them, and `src/smd/cl/reader/detail/read_context.hpp` still carries
`using smd::kit::foundation::bind;`. What changed is the registration, and
only there. The lookup variables took the plain names and the CRTP bases the
descriptive ones:

- `monad_typeclass<T>` → `monad<T>`, and likewise `functor`, `applicative`,
  `alternative`, `traversable`, `foldable`.
- `foundation::monad<Impl>` (the base) → `foundation::derive_monad<Impl>`.

So `src/smd/kit/parser/parser_instances.hpp` now reads
`struct parser_monad_map : foundation::derive_monad<parser_monad_impl>` and
`inline constexpr auto monad<smd::kit::parser::parser<F>> = ...`. If you add a
typeclass registration in B5, that is the shape to copy.

### The position on `derive_monad`'s deferral caveat

`derive_monad` now documents that its derivations assume `bind` invokes its
function before returning, and that an instance which defers its continuation
must supply those operations itself. Measured against `parser<F>`, exactly one
does: `fmap` derives as `bind(ma, [&](a){ return pure(f(a)); })`, holding the
callable by reference, while `parser`'s `bind` stores the continuation in the
parser it returns — a stack-use-after-return under Asan, and a lifetime error
in constant evaluation. `parser_monad_impl` therefore supplies `fmap` natively,
forwarding to the existing `parser::map`.

For you this means: `bind`, `pure` and now `fmap` are safe; `join`, `then` and
`kleisli` are inherited, correct, and reachable through their operation objects
if B5 wants them — `then` in particular is a real sequencing combinator you do
not have to write. `apply` is absent from overload resolution, and `parser<F>`
satisfies neither `monad_impl` nor `monad_object`, both because
`element_type_t<parser<F>>` does not exist: `parser<F>::operator()` is a
template over the threaded context, so the parsed value type is not a property
of the parser type. That is recorded as a deliberate position in
`docs/compiler_architecture.org` § B2, not left as a silent fact. Do not try to
"fix" it by giving `parser` a `value_type`.

`functor<parser<F>>` is still unregistered, so the `fmap` CPO still cannot
reach a parser and `parser.hpp`'s note on `map` stands. If B5 wants a
registered Functor, that is a decision to make and record, not a leftover.

### Your Tier-1 read is not the one B4 did

`docs/cpp-rules.md` gained a "Typeclasses" section and `docs/CODING_RULES.md` a
"Typeclass Design" section in this merge. Read them; they are short, and they
bind the shape of any instance you add.

### New ctest baseline

378 per leg, Debug and Asan, up from 360. Fifteen of those arrived with the
merge (`src/smd/kit/foundation/`), three are the `ParserInstancesTest - Fmap*`
cases this step added. None disappeared, and the reader and conformance suites
are untouched. Do not read the rise as your own step's doing.

### The WIP commit on `step-b5-text`

`78cc244` on `step-b5-text` predates this merge and knows nothing about the
rename. Merge or rebase it forward onto `cl-parser-combinators` before building
on it, and expect the typeclass spellings in anything it touches to be stale.
