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
