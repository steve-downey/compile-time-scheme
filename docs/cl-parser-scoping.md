# Scoping note for adopting parser combinators

- **Status:** proposal, awaiting ratification. Nothing here is authoritative yet.
- **Date:** 2026-08-16
- **Precedes:** `docs/cl-language-scoping.md`, whose phase sketch this displaces to second position.
- **Closes on landing:** DIV-0028.

The question that produced this document was narrow: is there a positive reason `cl`'s lexing and parsing are not written with combinators?
The answer is no, and the code says something more interesting than no.

---

# 1. What the code answered

DIV-0028 already recorded that R3's choice was never a deliberate rejection — "it is simply the shape R3's author chose, and no later step revisited it."
Reading the reader rather than the divergence doc sharpens that considerably.

**`cl`'s reader is already written in the combinator paradigm, without its vocabulary.**

`src/smd/cl/reader/cursor.hpp:87` defines its own `parse_state<T>` — `T value; cursor rest;` — which is field-for-field `src/smd/smdscheme/parser/parser.hpp:22`.
Every reader function returns `foundation::result<parse_state<int>>`, which is exactly the combinator layer's `parse_result<int>`.
`read_wrapped` is nested `and_then` continuations, which is monadic bind written out by hand.
R3 re-derived the two central types of the layer it did not adopt, and then hand-inlined its bind.

That is the rebuild's own §1 complaint — one missing abstraction billed repeatedly — recurring inside the tree built to not have it.

And the cost is not only aesthetic. `read_delimited` contains:

```cpp
auto const element = read_node(cur, ctx);
if (!element.has_value()) { return element; }
```

D15 names that construct as the thing recursion schemes exist to replace: "error propagation is a scheme's short-circuiting carrier, never an `if (!r.has_value()) return r;` ladder."
It is a live violation of a ratified rule, in the newest reader in the repository.
This is the actual argument for the change, and it is a correctness argument rather than a taste one: a ladder is correct by vigilance, a carrier is correct by construction.

---

# 2. What adoption actually requires

Three gaps separate the kit's `parser/` from what `cl` needs.
None of them is "combinators are the wrong paradigm." All three are the existing layer being Scheme-shaped.

**Gap 1 — the layer has no bind.**
`src/smd/smdscheme/parser/` offers `pure`, `satisfy`, `char_p`, `map`, `lift2`, `sequence_left`, `sequence_right`, `operator|`, `alt`, `many`, `some`, `optional`, and the `ParserApplicative`/`ParserAlternative` facades.
Grepping the whole directory for `and_then`, `bind` or `flat_map` returns nothing: **it is Applicative and Alternative, and not Monad.**

That sufficed for Scheme because Scheme's reader has no context-dependent syntax; `smdscheme/reader/` is 317 lines across three files and never needs the value of one parse to choose the next.
Common Lisp's reader is context-dependent in at least four places — `#nnR` parses a radix that then determines how the following token is read, `#\name` looks up a character name, sharpsign sub-dispatch selects on the character after `#`, and every datum position dispatches through the readtable.
`read.hpp` uses `and_then` fifteen times.
Applicative cannot express any of them: `lift2` fixes both parsers before either runs.

So adopting combinators here means **adding bind to the layer**, and that is the thing that makes it a general parser kit rather than a Scheme-shaped one.

**Gap 2 — a parser carries no context.**
`parser<F>` wraps `parse_result<T>(cursor)`.
Every `cl` reader function is `(cursor, Ctx&)`, where `Ctx` carries the datum arena, the symbol table and the readtable.
`Ctx` is not even a named concept today — it is duck-typed, reached through `typename Ctx::child_list`.

**Gap 3 — a parser has no effect channel.**
`cl`'s parsers write nodes into a fixed-capacity arena and return `int` indices into it, with a capacity check that can fail.
Gaps 2 and 3 are one gap wearing two hats: the layer needs a state-carrying parser.

One thing that looked like an obstacle and is not: **the readtable.**
Dispatch is `switch (table.macro_of(c))`, and because a datum is an arena index, every branch has the same return type `result<parse_state<int>>`.
Same-typed alternatives are *easier* to combine than `smdscheme`'s heterogeneous `T`.
`set-macro-character` later becomes a table lookup selecting among same-typed parsers, which is the structure D19 asked the reader to have anyway.

---

# 3. Proposed decision records

Numbering continues `docs/cl-language-scoping.md`'s proposed D22–D26, which are themselves unratified; if those are renumbered these move with them.

**D27 — `cl`'s reader adopts the combinator style, and the kit's `parser/` is generalized to carry it.**
The motive is correct-by-construction error propagation (§1), not reuse and not kit tidiness.
Closing DIV-0028 and making `smd::kit::parser` a real module with a real client are consequences, not reasons — R8 already showed that extracting for its own sake produces a module with no callers.

**D28 — The parser layer becomes Monadic, not merely Applicative.**
Bind is added alongside the existing Applicative and Alternative operations, with the monad laws tested as laws, per `docs/CODING_RULES.md`'s "law-focused tests before performance tests."
Applicative composition stays the default and bind is used only where the grammar is genuinely context-dependent, because `lift2` says something stronger about a parser than `and_then` does and that information should not be discarded where it holds.

**D29 — A parser carries its context by construction, not by capture.**
A parser becomes `parse_result<T>(cursor, Ctx&)` over a named `Ctx` concept, rather than `parse_result<T>(cursor)` with `&ctx` smuggled into every lambda's capture list.

*This is the one proposed decision with a real alternative, and it should be answered deliberately rather than nodded through.*
Capture is less invasive — no combinator signature changes, and the existing `smdscheme` and `forth` clients keep compiling untouched.
It is also exactly the un-checkable version: nothing in a type says which parsers touch the arena, a capture-by-reference outliving its context is a lifetime bug the compiler will not catch, and DIV-0007 is this project's own record of what that class of bug costs.
Threading it is more churn now and is the correct-by-construction option, which is the criterion D27 rests on.

**D30 — Compile-time cost is measured and recorded, not gated.**
Recorded on the owner's direction, 2026-08-16: the restrictions of constexpr are valuable here as a correctness discipline and as a demonstration of what is possible, not as a product feature — "having a full CL at compile time is almost beside the point."
A large constant-factor increase in compile time is therefore an acceptable price for structure that is correct by construction, and cost does not veto D27.
The step still measures and records before-and-after numbers — translation-unit wall time and `-fconstexpr-ops-limit` headroom, both legs of the matrix — because evidence-before-claim is house style and "we measured it, it cost this much, we accepted it" is a far better record than silence.
Only a qualitative failure counts against: an ops limit that cannot be raised, or a test matrix that becomes unusable in practice.

**D31 — The rewrite changes no observable behaviour.**
No new syntax lands in the same step as the rewrite.
The acceptance witness is the existing evidence, unmodified: 1317 lines of reader tests across seven files, `reader/oracle_compare.test.cpp`'s SBCL differential, and R6's conformance corpus, all passing unchanged.
DIV-0003 (whole-token atom classification, so `1+` reads as a symbol) is correct ANSI behaviour and must not regress — the plan names it because it is the one behaviour a naive combinator translation would most plausibly break, by classifying character-at-a-time.

---

# 4. The phase sketch

A P-series, preceding the C-series of `docs/cl-language-scoping.md`.
Not a plan; shown so the decisions can be seen to have consequences.

The shape is narrow, then wide, then narrow.

**Corrected 2026-08-16, against the execution plan in `tmp/plan/`.**
The sketch this section first carried was wrong in five ways that only became visible once the steps were written against the code, and the list below is the corrected one.
§4's original P1 created `src/smd/kit/parser/` while its P10 claimed to "extract" it, which is a contradiction; the kit is built by copy in P1–P2 and P10 closes DIV-0028 by recording the client relationship rather than performing a move.
Two of the corrections are load-bearing and are given their own paragraphs below.

**P1 — kit parser core (serial).** `parse_context`/`no_context`, cursor, `parse_state`/`parse_result`, `parser<F>` over `(cursor, Ctx&)`, the applicative primitives, `empty`, and **`bind`**, with Functor, Applicative and Monad law tests.
**P2 — kit choice and facade (serial).** `alt`/`many`/`some`/`optional`, `parser_ops` with a Monad tier, Alternative law tests, and the `alt` diagnostics assessment §5 asks for.
**P3 — `cl` retires its duplicates (serial).** `reader/cursor.hpp` becomes an R8-style shim; duck-typed `Ctx` becomes the named `reader_context` concept; the D30 "before" measurement is taken here.
**P4 — split `read.hpp` (serial, mechanical, zero behavioural diff).** Eleven headers plus an umbrella, so the lanes are file-disjoint.
**P5 — intertoken space and comments (serial).** Not a lane; see below.
**P6–P8 — the lanes (fan-out).** `token` (DIV-0003) · `text` (strings, characters) · `forms` (lists, quote).
**P9 — sharpsign, radix and top-level dispatch (serial).** `#nnR` and `read_node`.
**P10 — integration (serial).** D30 "after", DIV-0028 closed, architecture section, project checklist.

**There is no numbers lane, and that is the plan's best correction.**
`classify_number` is a whole-token classifier rather than a parser — it *is* the mechanism by which DIV-0003 holds, because classifying a whole token is what makes `1+` read as a symbol.
A numbers lane would therefore be either empty or a standing invitation to break the one behaviour §5 names as most at risk.
`scan_token` and `classify_number` are declared out of scope for the entire series, which makes DIV-0003 structurally unbreakable instead of merely tested against.
That is the same move as D29 and for the same reason, and it is worth more than the parallelism it costs.

**Intertoken space is serial because the collision is in the signatures, not the files.**
`read_delimited` calls `skip_intertoken_space` on every iteration.
Converting the callee while a peer lane rewrites the caller is not a merge conflict that a split can prevent, so P5 lands before the fan-out opens.

Every step ships a post (D20). Post numbering continues from 32; phases 33–42 and divergence numbers DIV-0029 through DIV-0033 are reserved per step up front, so concurrent lanes cannot both take the next number.

---

# 5. Risks

- **DIV-0003 regression** is the likeliest real defect, for the reason D31 gives. The token lane should be the one with the most oracle attention.
- **Diagnostic quality.** Hand-rolled parsers produce targeted messages (`"expected ')'"`); combinator `alt` chains drift toward "expected one of". `alt.hpp`'s existing behaviour has not been assessed against `read.hpp`'s current messages, and it should be, in P1 rather than at P10.
- **P4 is boring and load-bearing.** A mechanical split that quietly changes behaviour is the worst outcome available, because every later lane inherits it. It merges on the matrix with a zero-diff behavioural claim.
- **Two specific D31 traps, both found while writing the steps rather than while writing this note, and both cases of the idiomatic combinator being a *better parser* than the one it replaces — which D31 forbids.**
  `skip_block_comment` currently lets an unterminated `#|` consume to end of input and leaves the caller to report; the natural recursive combinator fails at the comment instead.
  And the kit's `many<Capacity>` stops at capacity and *succeeds*, where `read_delimited` diagnoses `"too many elements"` — a stock `many` would silently truncate an over-long list, then fail later at the wrong position with the wrong message.
  Neither is a bug being preserved; both are behaviour the corpus and the oracle pin, and D31 says the series does not get to improve them in passing.
- **DIV-0027** (no source positions on diagnostics) sits adjacent to all of this. It is *not* in scope — D31 forbids behaviour change — but P1's signatures should not make it harder to add later.

---

# 6. Consequence for the language scoping note

`docs/cl-language-scoping.md` (PR #48) says in §2.2 that no lane should rewrite the reader, and its §2.3 opens the fan-out only after the spine.
Both need amending if D27 is ratified: the reader rewrite becomes a phase in its own right, and it comes first.
That is a correction to an unratified proposal rather than to a landed decision, so it is a second commit on the same branch rather than a divergence record.

---

# 7. What happens next

1. **This document, ratified** — D27 through D31, with D29 given a deliberate answer.
2. **`tmp/plan/`**, the execution fan-out for P1–P10, produced alongside this note so the shape can be judged with it rather than after it. Cleared agents execute in sequence with disk handoff, per `AGENTS.md`'s step-brief contract; each step's merge criterion is `make test-matrix`, both legs.
3. **`docs/cl-language-plan.md`** and the C-series after, unchanged in substance but second in order.
