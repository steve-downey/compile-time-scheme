# Common Lisp rebuild plan

Successor to `docs/cl-pivot-plan.md`.
The pivot plan is not superseded as history: steps L0–L23 landed and `src/smd/smdlisp/**` is the working front end this plan replaces.
What is superseded is its decision set, extended here by D11–D17, and its step list, replaced here by phases R0–R8.

Decision numbering continues `docs/cl-pivot-plan.md`'s, which owns D1–D10 in this repository.
`compile-time-forth` numbers its own decisions independently; a D-number is only meaningful together with the repository it was recorded in.

---

# 1. Why rebuild rather than refactor

`smdlisp` works.
Its architecture is emergent rather than designed: it was built by TDD small-step refinement under ground rules fixed before the destination was known, and the destination is now known.

Twenty divergence docs record the consequences honestly, one at a time.
Read together, seven of them are the same missing abstraction billed seven times (§3).

Three things make this the moment.

**The freeze that shaped the design is obsolete.**
D1 froze `src/smd/smdscheme/**` because published blog phases transclude its code by UUID anchor and an in-place pivot would silently rewrite them.
That was true under worktree-path transclusion.
It stopped being true on 2026-07-25, when `docs/epistolary-pinning-plan.md` moved posts onto `orgit-file:` links pinned to `blog/phase-NN` tags, resolved with `git show TAG:PATH` and enforced by `scripts/verify-transclusions.sh`.
DIV-0004's own supersede note says so.
A refactor can no longer reach a published post, and the freeze is now blocking four recorded compromises (DIV-0011, DIV-0012, DIV-0016, DIV-0021) for a reason that no longer exists.

**The test suite has become a ratchet rather than a safety net.**
Roughly 26 tests pin *divergent* behaviour.
DIV-0021 and DIV-0018 state outright that they exist so that closing the underlying bug forces someone to delete a test.
That is a reasonable local discipline and a bad global one: editing such a suite in place creates pressure to relax tests and document new bugs, rather than to ask whether the facility under test has the right shape at all.

**The intended style is already written down and was never adopted.**
`docs/CODING_RULES.md` is Tier 1 and authoritative.
It mandates `fold_map` as the semantic centre with `fold_left`/`fold_right` derived, `traverse` as the minimal Traversable operation preserving shape, typeclass instances selected by variable template, and law-focused tests before performance tests.
`smdlisp` has 79 raw index loops across eight headers, 23-arm `std::visit` switches, no Foldable or Traversable instance for either AST, one test file mentioning laws, and a `foundation::fold_fix` with no caller, no test, and an inverted `fmap` argument order.
(Corrected 2026-08-02, during the phase-23 blog review. As first written this read "43 raw index loops across five headers" and "26-arm"; neither reproduced. `grep -rh 'for (int ' src/smd/smdlisp --include=*.hpp` gives 79 across eight headers, and the core variant in `elaborator/elaborated_core.hpp` has 23 alternatives, not 26. The argument is unaffected and the loop figure is worse than claimed; the numbers are corrected because a plan that cites evidence should cite evidence that reproduces.)

The gap between `docs/CODING_RULES.md` and `src/smd/smdlisp/**` is this plan's specification.
That brief is better than "make it nicer" and it is already ratified.

---

# 2. Decision records

Overturning one requires a divergence doc and orchestrator sign-off, as with D1–D10.

**D11 — The `smdscheme` freeze is retired.**
D1's semantic freeze on `src/smd/smdscheme/**` is lifted.
Its rationale was superseded by per-post tag pinning; the anchor rules themselves are unchanged (do not delete or nest an existing anchor pair, new anchors must be real `uuidgen` output).
`smdscheme` is still not a dumping ground: it remains the Scheme front end, and the rebuild happens in a new tree.
What changes is that a fix to shared `foundation` code is now an ordinary change rather than a divergence-doc event.
DIV-0012 and DIV-0016 close as a consequence.

**D12 — Symbols are interned objects with slots.**
A symbol is an entry in a symbol table, referred to by a stable id, carrying a name, a value slot, a function slot, and a macro slot.
Identity is id comparison, not string comparison.
This replaces `closure::symbol`, which is a `std::string_view` wrapper.
It is the keystone decision: it closes or downgrades DIV-0002, DIV-0006, DIV-0007, DIV-0009, DIV-0010 and DIV-0014, and it makes a compiled program self-contained because names stop being views into the datum arena.

**D13 — The evaluator result has three channels.**
A result is a value, a diagnosed error, or an unwind in flight, as distinct alternatives of one type.
Control flow does not travel in the error channel.

This is not a new idea in this project, and the rationale is not restated here: read `docs/compiler_architecture.org` § "A sender has three completion channels, and that is a semantic resource".
That section establishes that four things travel the closure backends' one wire — a value, a diagnosed error, and two kinds of unwind — that telling them apart "needs an out-of-band discriminator", and that the discriminator is a pair of sentinel `parse_error` messages compared by pointer identity.
It also records that the sender backend already **deletes** that machinery rather than porting it: value to `set_value`, error to `set_error`, unwind to `set_stopped`, so "nothing is multiplexed, so nothing needs discriminating".
D13 therefore generalises the backend that got this right to the two that did not.

Two consequences that section names, which R5 must plan for rather than discover.

The property `unwind-protect` currently gets for free must be **reconstructed**.
Under direct and CPS evaluation, "run the cleanup on every exit path" works because the four outcomes are already merged into one value before `unwind-protect` sees them, so a single unconditional statement suffices.
Split the channels and that is no longer true; the architecture doc's answer is a receiver whose three completion functions funnel into one, which makes cleanup-on-all-channels a structural property rather than a checklist item.

The split is not uniformly better.
Restoring a dynamic binding stays in ordinary C++ control flow around the recursive call, because it brackets a callee's whole extent rather than a single completion.
Splitting is better exactly where the thing being expressed is a completion.

**D14 — Capacity is not part of type identity.**
No runtime value type may be parameterised on a container's capacity.
Today `value` contains `closure` contains `env<Core, MaxBindings>`, so a value's type depends on environment capacity, which is why `static_assert(MaxBindings == 16, ...)` appears three times in `closure/cps_code.hpp`.
Capacities are properties of storage, not of the types stored.
This unblocks DIV-0019 and DIV-0020, and it is the precondition for BL-0002.

**D15 — Traversals are typeclass instances, not hand-written recursion.**
The datum and core trees carry Foldable and Traversable instances, with documented traversal order as part of the instance contract, per `docs/CODING_RULES.md`.
`traverse` over the result applicative *is* the elaborator's error propagation.
Raw index loops are permitted only inside the substrate's own generic algorithms.
This is a style rule that is really an architecture rule: the loop count is a consequence of arena indices plus wide `std::visit`, and it does not fall without changing that shape.

**D16 — Conformance is measured against external authority.**
Adopted from `compile-time-forth`'s D14 and D23.
Correctness evidence comes from a corpus derived from the standard and from a differential oracle running a reference implementation, not from tests written against this implementation's behaviour.
A test may pin a `scope-decision` divergence.
A test must never pin a `defect` (§3).

**D17 — Backends follow the language, not the reverse.**
One evaluator is authoritative.
Additional backends are demonstrations and may lag.
The object language is never held back to keep backends at parity, which is what produced DIV-0017 and DIV-0018.
DIV-0015 (the sender backend cannot carry compile-time evidence) is accepted-permanent and is a reason to decouple, not to synchronise.

A statement from `docs/cl-limitations.md` carries forward verbatim into the rebuild's own documentation, because it is easy to get wrong and consequential when it is: **do not describe all three backends as compile-time evaluators — only the closure backends are.**
Beman Execution's `connect`/`start`/`sync_wait` are not constant-evaluable, so a sender graph cannot produce a compile-time value at all.

**D18 — Prefer lowering to core forms over new node kinds.**
A new special operator is implemented by lowering it onto forms the core already has, unless it needs semantics none of them express.
The pivot proved this works, in the step that had most reason to add nodes.
Step L20 added multiple values with **no** `core_values` node: `values` is an ordinary builtin, so `#'values`, `(funcall #'values 1 2)` and `(apply #'values '(1 2))` all work without further effort.
`multiple-value-bind` lowers to a `core_lambda` application, which is how it inherits implicit progn, arity checking, closure capture and the special-variable dynamic-binding rule "without restating any of them" (DIV-0020).
This is the standing mitigation for node-kind growth, and it is why the backend-multiplication cost stayed manageable across three evaluators.

**D19 — Full ANSI Common Lisp is the aim.**
Recorded 2026-08-01, resolving the first open question of §8.
D10's out-of-scope list carries into the rebuild as a staging roadmap, not a fence: every entry is "not yet", none is "never".
The pivot proved the pipeline on a deliberately small core; the rebuild does not re-prove it — it aims at the language.
Three consequences bind the phases.

Reader syntax may lead semantics.
An atom kind may be readable before it is executable, so the reader implements full ANSI atom syntax from R3 — strings, characters, the numeric tower's spellings, `#(...)` vectors — and the datum model carries what cannot yet be evaluated: a tower literal whose machine representation does not exist yet is still a valid datum, carried as its spelling (digits and radix).

The numeric tower is a fan-out.
Syntax lands at once in R3; semantic support arrives per tower member — fixnums first, then floats, ratios, bignums — in parallel lanes gated on the evaluator, each lane with its own conformance evidence.

The reader is readtable-shaped from the start.
Character-dispatch-driven, so `*readtable*` and `set-macro-character` are a later exposure of structure that already exists rather than a rewrite.

Conditions and restarts enter at R5 on D13's channels, which were designed for them; CLOS enters through a static-dispatch subset before the full protocol; `loop` and `format` are library-shaped work once the substrate is right.

**D20 — Every phase ships a post, and its author is not its implementer.**
Recorded 2026-08-02, repairing an omission rather than changing a policy.

The pivot made a blog post a named deliverable of the step it describes.
This plan's phase list dropped it, and R0–R4 landed without one; §7 restores it and states the authorship rule that was previously assumed rather than written down.

A post is drafted by a fresh agent reading the *merged work* — the step's diff, its commit message, its retired step brief — and never the conversation that produced it.
There are two reasons, and the second is the load-bearing one.

The implementing agent is at its most expensive exactly when the step ends, and the post is the cheapest deliverable to move off that context.

More importantly, `docs/cl-pivot-plan.md` already records what a post is: "a real-time, non-omniscient diary entry."
An agent that has just spent a step's worth of context on the work knows every path it did not take, and writes omnisciently about a result that was not obvious at the time.
An agent reading the merge sees what the work actually shows, which is what a reader of the post will see.
Authorship distance is therefore a property of the genre, not a cost optimisation that happens to be convenient.

The draft is then reviewed by a second, clean agent running the `voice` skill, seeing only the draft and the diff.
That rule is already in force for commit and PR messages (`AGENTS.md`); a post is longer prose with more room to drift, so it is not the place to relax it.

Three agents, none reading another's conversation: implement, draft, review.

---

# 3. What the divergences actually say

Seven divergences are one absence — symbols are strings, not objects.

| Divergence | Symptom | Closed by D12 |
|---|---|---|
| DIV-0009 | recursive `defun` fails with `undefined function` | yes, function slot resolved at call time |
| DIV-0014 | dynamically binding an unbound special is an error | yes, the value slot is the cell |
| DIV-0010 | `defmacro` is scoped to one top-level form | yes, macro slot in a global table |
| DIV-0006 | `%OR-TEMP` reserved names instead of `gensym` | yes, `gensym` is an uninterned symbol |
| DIV-0002 | `eq` and `eql` are the same comparison | yes, `eq` becomes id identity |
| DIV-0007 | the datum arena must outlive the program | yes, names become ids and nothing dangles |
| DIV-0001 | no package system | partly, a package is a symbol table |

DIV-0009 is the headline.
A Lisp in which `(defun len (l) (if (null l) 0 (+ 1 (len (cdr l)))))` fails is not yet a Lisp, and step L14's block tests were rewritten to avoid recursion because of it.

The remaining divergences fall to D13 (DIV-0011, DIV-0021), D14 (DIV-0019, DIV-0020), D11 (DIV-0012, DIV-0016) and D17 (DIV-0017, DIV-0018).

One correction to a claim it would be easy to overstate.
The three-backend structure looks expensive — `eval_direct` at 1084 lines, `cps_code` at 1068, `sender_eval` at 781, each an exhaustive visit over 23 core node kinds — but the pivot managed it better than those numbers suggest.
`docs/cl-limitations.md` records that as of step L23, multiple values is the **only** place the three backends disagree on the object language, and D18 above is how that was achieved.
So D17 does not rest on parity being unattainable.
It rests on parity being a tax that should be paid deliberately, on a backend with genuinely different evidence to offer.

## Where the divergences are recorded

Three documents describe the same set of divergences and answer different questions.
They are kept separate on purpose; do not merge them.

- `docs/cl-limitations.md` — thematic and user-facing.
  Organised by what broke rather than by when it was decided, because "a reader debugging a program cares what broke".
  This is the behavioural baseline the rebuild must beat.
- `docs/divergences/README.md` — classification and engineering triage.
  Answers one question the thematic view does not: may a test pin this?
- `docs/compiler_architecture.org` — the design record.
  Why the pipeline has the shape it has, by UUID anchor, and the living document that rolls forward.
- `docs/divergences/DIV-*.md` — the primary sources, append-only.
  The other three summarise; none replaces them.

---

# 4. Test architecture

This is the load-bearing decision, and the pattern is taken from `compile-time-forth` rather than invented.

That project tests against external authority: `src/smd/forth/conformance/ttester_corpus.hpp` is adapted from John Hayes' TESTER.FR and Anton Ertl's ttester.fs, the standard's own corpus; `conformance/gforth_diff.{hpp,cpp}` runs the same programs through the image and through real gforth, diffs stacks and output, and records the oracle version so a reported divergence is attributable to a specific build.
Both are ratified there as decision records D14 and D23.

The property that matters: you cannot relax a test that came from the standard's corpus, and you cannot argue with an oracle.
That is a structural answer to the ratchet problem, not a discipline that has to be maintained by willpower.

Four tiers, in the order they are added:

1. **Law tests.**
   Functor, Applicative and Traversable laws for every instance, per `docs/CODING_RULES.md`'s "add law-focused tests before performance tests".
   These are the only honest evidence that the recursion schemes are right.
2. **Conformance corpus.**
   A subset of Paul Dietz's `ansi-test` suite covering the implemented operator set, grown as the language grows.
   *Availability and licence terms are unverified and must be checked in phase R6 before this is relied on.*
3. **Differential oracle.**
   SBCL, with ECL or CLISP as fallback, mirroring `gforth_diff`: same source through both pipelines, diff values and output, pin the oracle version.
   Verified 2026-08-01: no implementation is installed in the dev environment, but sbcl (apt candidate 2:2.6.0-1), ecl and clisp are all one `apt install` away; the pinned oracle version is whatever is installed when the harness lands.
   The harness itself is R6's deliverable, but informal hand-run differential checks are available from R2 onward, and should be preferred over tier 4 wherever the old tree pins a `defect` the rebuild fixes — for those behaviours the old tree is a wrong oracle by design.
4. **The old tree as a behavioural oracle.**
   `src/smd/smdlisp/**` keeps building and running and is **never edited**.
   A disagreement between old and new is evidence to be explained, not a merge conflict to be resolved.

---

# 5. The kit

Deferred deliberately.
The seams are already visible from two real forks, and the third instance is what makes them trustworthy.

**Copied with essentially zero drift**, and therefore the kit's actual contents: `foundation/` — `static_vector`, `result`, `parse_error`, `source_pos`, `source_span`, `arena_box`, `functor`, `applicative`, `alternative` — plus `parser/` — `cursor`, `parser`, `alt`, `parser_ops`.
Diffing the `smdscheme` and `forth` copies gives 7–15 changed lines per file, and for `functor`, `applicative`, `alternative`, `result` and `source_pos` that difference is exactly the include guard, the namespace, and the "adapted by copy" comment.
Two independent copies, no semantic divergence, is the signal that something wants to be a library.

**Shared as pattern rather than code**: the sender lowering backend; the conformance and differential-oracle harness; the governance stack (`AGENTS.md`'s three-tier reading contract, `docs/CODING_RULES.md`, `docs/codestyle.org`, step briefs, divergence docs, blog pinning); the build infrastructure.

**Not shared, and this is the important part**: `compile-time-forth` has no AST, no `fix`, and no elaborator.
It has `machine/` (VM, dictionary, data space, stacks, cell) and `interpreter/`.
So the shared substrate is *not* a compiler construction kit in the AST sense.
It is a constexpr computation substrate, plus parser combinators, plus a lowering pattern, plus a conformance-harness pattern.
AST and recursion-scheme machinery is shared only among Lisp-family front ends and belongs in a second, narrower layer above the substrate.

Extraction happens in R8, once Common Lisp is the third working client.
Until then the substrate is developed in place with kit-shaped seams, meaning it stays free of language-specific types — which is already true of `foundation/`.

---

# 6. Phases

Each phase is independently mergeable.
No time estimates: this is hobby work, and getting it right is the point.

Every phase carries a blog post as a deliverable of the phase, per D20, numbered continuing the existing series (0–22 are the Scheme pipeline and the Common Lisp pivot).
The post is not written by the agent that did the work, and the phase is not finished when the code merges — it is finished when the post lands.
§7 has the mechanics.

**R0 — decisions.**
This document, `docs/divergences/README.md`, the classification of every existing divergence, and the checklist entries.
Docs only, no code.

**R1 — substrate.**
New tree `src/smd/cl/`, sibling to `smdlisp`.
Its `foundation/` begins as the reviewed union of the `smdscheme` and `forth` copies, plus the short-circuiting fold that `std::ranges::fold_left` does not provide, plus the typeclass instances the rules require, with law tests.
`foundation::fold_fix` is deleted rather than repaired: it has no caller and no test, and `mendler_para` supersedes it (DIV-0016).

**R2 — symbols.**
The interned symbol table of D12.
The keystone; everything downstream depends on it.

**R3 — reader and core AST.**
The reader's staging carries over; its scope does not (D19, 2026-08-01).
The reader is readtable-shaped — character-dispatch-driven — and implements full ANSI atom syntax: strings, characters, the numeric tower's spellings, `#(...)` vectors, alongside the pivot's symbols, fixnums and lists.
Tower atoms beyond fixnums are readable but not yet executable; the datum carries their spelling per D19.
DIV-0003 (whole-token classification, so `1+` reads as the symbol it is) is correct ANSI behaviour, accepted-permanent, and must not be regressed.
The `smdlisp` reader is the behavioural oracle only for the syntax the pivot implemented; new atom kinds take their tests from the specification and the informal SBCL differential check (§4).
The core AST gets Foldable and Traversable instances from the start rather than retrofitted.

**R4 — elaborator as traversal.**
`traverse` over the result applicative, replacing the `if (!r.has_value()) return r;` ladder.
Mendler paramorphism where an algebra needs the whole node, which DIV-0016 already established is the common case.

**R5 — one evaluator, three channels.**
CPS backend on D13's result type.
Recursive `defun` works at the end of this phase or the phase is not done.

**R6 — conformance.**
`ansi-test` subset and the SBCL differential harness, modelled directly on `src/smd/forth/conformance/`.
Verify `ansi-test` availability, licence terms and its post-D10 usable subset first; the oracle side is already verified (§4, 2026-08-01 note).

Scope this honestly at the start of the phase rather than discovering it midway.
`ansi-test` leans heavily on strings, characters and the numeric tower, and D10 puts all three out of scope.
The directly usable subset may be small enough that the corpus tier degrades to cases hand-derived from the specification, with the differential oracle carrying most of the weight.
That is the expected outcome rather than a fallback: `compile-time-forth` hit the same wall and its `ttester_corpus.hpp` is "adapted (not copied verbatim)" from ttester.fs for exactly this reason.

**R7 — sender backend.**
Only once the object language is settled, per D17.

**R8 — extract the kit.**
Once Common Lisp is the third working client.

## Posts

| Phase | Post | Subject |
|---|---|---|
| R0 | 23 | why rebuild rather than refactor; what twenty divergences turned out to say |
| R1 | 24 | the substrate, and what two independent copies of a file tell you |
| R2 | 25 | symbols as interned objects with slots; the keystone (D12) |
| R3 | 26 | a readtable-shaped reader, and one arena tree with instances from the start |
| R4 | 27 | elaboration as three index folds; why a catamorphism needs no stack |
| R5 | 28 | one evaluator, three channels; recursive `defun` at last |
| R6 | 29 | conformance against external authority |
| R7 | 30 | the sender backend, second time around |
| R8 | 31 | extracting the kit |

Titles and framing belong to whoever drafts each post; the subjects above are the phase's own claim on a reader's attention, not a brief.

---

# 7. The blog

Each phase's post is a diary entry about that phase, transcluding live code by UUID anchor rather than quoting it.
The pinning machinery is `docs/epistolary-pinning-plan.md`, the post-to-tag mapping is `docs/blog/pins.md`, and neither is restated here.
What follows is only the per-phase sequence, which D20 fixes.

**1. The implementing step lands its own UUID anchors.**
An anchored region must exist at the commit its post pins to, and the post pins to the step's merge, so the anchors have to arrive with the code.
The implementer is also the one who knows which regions are the step's centre.
Anchors are real `uuidgen` output; an existing pair is never deleted or nested; the step brief records which regions got them.
A step that lands no anchors leaves its post nothing to transclude, which is how R1–R4 ended up needing the backfill in `docs/blog-backfill-plan.md`.

**2. The orchestrator tags the `--no-ff` merge, before dispatching anyone.**

```sh
git tag -a blog/phase-NN -m "Phase NN: <title> (step RN)" <merge-sha>
git push origin blog/phase-NN
```

The tag must exist first, because the drafting agent writes `orgit-file:` links naming it.
A pin may move only for an anchor absent at the pin, and only before the post is published; once the prose is out, moving the tag is what pinning exists to prevent.

**3. A fresh agent drafts the post.**
Its reading list is the merged work — `git show <merge-sha>`, the step's commit message, the retired step brief, `docs/blog/` for the house format, and the preceding post for continuity — plus the `voice` skill.
It does not read the implementing agent's conversation, and there is no handoff between them (D20).
It writes `docs/blog/phase-NN-slug.org`, adds the index entry, and leaves `make blog-md` and `scripts/verify-transclusions.sh` green.

**4. A second, clean agent reviews the draft with the `voice` skill.**
It sees the draft and the diff, and nothing else — the same rule `AGENTS.md` sets for commit and PR messages.

**5. The orchestrator lands the post and records the pin** in `docs/blog/pins.md`.

The phase is done at step 5, not at the merge.

---

# 8. Open questions

Recorded rather than answered, to be settled in the phase that reaches them.

- **Whether D10's out-of-scope list carries over unchanged.**
  Step L24 consolidated it in one place for the first time: no strings, characters, floats, ratios, bignums, vectors, CLOS, conditions and restarts, `format`, `loop`, `setf` expanders beyond `setq`, readtables, `eval-when`.
  Two entries now interact with decisions already taken, so this is a live question rather than an inherited given.
  Conditions being out of scope is the *sole* reason DIV-0011 cannot be fixed — there is no "signal without unwinding" channel to signal into — and D13 is precisely the change that makes a minimal `signal`/`handler-case` expressible.
  The absence of strings is what constrains R6.
  **Resolved 2026-08-01: it does not carry over.**
  D19 makes full ANSI Common Lisp the aim and turns the list into a staging roadmap; R6's expected corpus subset grows accordingly as tower and string semantics land.
- Whether `ansi-test` is usable directly, needs adaptation as `ttester_corpus.hpp` did, or must be hand-derived from the specification. R6.
- Whether the symbol table is a compile-time-only structure or survives into the runtime program, which interacts with D14 and with BL-0002's footprint work. R2.
  **Resolved by R2, 2026-08-01: the table survives into the runtime program.**
  The slots are runtime state (`setq` mutates the value slot under the evaluator) and names must travel with the program for `symbol-name`, printing, and diagnostics; D14 holds because programs traffic in capacity-free `symbol_id` while capacities parameterise only the table.
- Whether `smdlisp` is eventually retired or kept permanently as the pivot's artefact.
  Not urgent; it must survive at least until R6 can replace it as the behavioural oracle.
- Whether the second, Lisp-family layer of the kit (AST, recursion schemes) is worth extracting at all, given it would have exactly one client until a third Lisp-family front end exists.
