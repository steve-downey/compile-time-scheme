# Scoping note for the language plan

- **Status:** ratified by merge, PR #48, 2026-08-22. As provisionally as anything here — later facts and experience can override an earlier decision, and D32 below is the first exercise of exactly that.
- **Date:** 2026-08-16
- **Successor to:** `docs/cl-rebuild-plan.md`, closed by `docs/cl-rebuild-closing-note.md` at step R8.

This is the document that has to exist before `docs/cl-language-plan.md` can be written honestly.
It records what the language surface actually is today rather than what the last plan assumed, argues for a decomposition, and proposes the decision records D22–D26 that decomposition rests on.
It is deliberately not a phase list: §4 sketches one only far enough to show the proposed decisions have consequences, and the real list belongs in the successor plan once these are ratified.

The rebuild plan earned the right to be written by first writing §1–§3 of itself.
This is the same move, and the reason is the same: the last three plans in this repository each began by measuring the tree instead of trusting the previous plan's account of it, and each time the measurement moved something.

---

# 1. Where the language stands

Measured against `src/smd/cl/` at `1ace4ad`, 2026-08-16, by grep across every area rather than from any plan document.

**The rebuild rebuilt the machine, not the language.**
R0–R8 delivered a substrate, an interned symbol table, a columnar AST with instances and schemes, an elaborator made of three named traversals, a three-channel evaluator, a conformance harness with an SBCL oracle, a sender backend, and an extracted kit.
Every one of §1's three complaints in `docs/cl-rebuild-plan.md` is met, and `docs/cl-rebuild-closing-note.md` is right to say so.

What none of that did was grow the object language.

| | `src/smd/cl/` (live) | `src/smd/smdlisp/` (oracle, never edited) |
|---|---|---|
| Special operators | 7 in `operator_ids`, plus `quote`/`function` handled at emission | ~20 |
| | `quote function if progn defun block return-from unwind-protect` | adds `let setq defvar defparameter catch throw tagbody go defmacro lambda multiple-value-bind` |
| Macros | none — no expander, no `defmacro`, no `cond`/`case`/`when`/`unless`/`and`/`or` | `macroexpand/expander.hpp`, 1602 lines, with host macros and backquote |
| Builtins | 18 (`eval/builtin.hpp`) | adds at least `funcall apply values append` |
| Functions as values | **none.** `eval::value` has no function alternative (DIV-0025) | yes |

The gap is total rather than partial, and that is worth stating precisely because it is easy to assume otherwise: the strings `LET`, `SETQ`, `LAMBDA`, `DEFMACRO`, `CATCH`, `TAGBODY`, `FUNCALL` and `DEFVAR` appear **nowhere at all** under `src/smd/cl/`.
They are not diagnosed-but-unimplemented. They are absent.

So `cl` is currently behind its own behavioural oracle, and the honest description of the next stretch of work is not "extend Common Lisp" but "re-cross ground the pivot already covered, on the substrate that was built to carry it, with a conformance harness that can now measure the crossing against SBCL rather than against ourselves."

Two things sit on the credit side of that ledger, and both are D19 paying out.

**The reader is ahead of the evaluator, on purpose.**
`reader/readtable.hpp` already dispatches `` ` `` and `,` to `macro_kind::backquote` and `macro_kind::comma`, and `#'` to `sharpsign_kind::function_quote`.
The macro lane and the function-value lane each find their reader half already built and tested.
This is exactly the staging D19 asked for — "an atom kind may be readable before it is executable" — and it means those lanes start at the elaborator rather than at the character level.

**The conformance harness exists before the language it will measure.**
R6 shipped `conformance/corpus.hpp`, `driver.hpp` and `sbcl_oracle.hpp`, with every entry stating its expected outcome *by channel*.
Growing the corpus is now cheaper than growing the language, which inverts the usual order and is the basis of proposed D26.

---

# 2. The shape of the work

The decomposition question is not "which operators" but "which of them can be worked on at the same time," and the answer has a sharp discontinuity in it.

## 2.1 A serial spine

Five pieces of work strictly unblock each other, in this order. None of them parallelises, and attempting to would mean inventing the same channel twice in two lanes and reconciling it later.

1. **The function value channel.** `eval::value` gains a function alternative; `lambda` in expression position produces one; `#'f` retrieves one; `funcall` and `apply` consume one. Closes DIV-0025.
2. **Lambda lists.** `&optional`, `&rest`, `&key`, `&aux` in `elaborator::read_lambda_list`. Closes DIV-0024. Needs (1), because `&rest` binds a value that only means anything once functions can be passed.
3. **Lexical binding.** `let`, `let*`, and `setq` on the lexical environment.
4. **Special variables and dynamic binding.** `defvar`, `defparameter`, and the dynamic rebinding rule, on D12's value slot. Needs (3) for the binding form and (1) because the closure capture rule is stated against both.
5. **Macros.** `defmacro`, backquote, `gensym`, and the host macros (`cond`, `case`, `when`, `unless`, `and`, `or`). Needs (1) — a macro function *is* a function — and needs (4), because a macro function evaluates in the global environment.

Everything else in Common Lisp needs at least one of these five.
That is why the spine is a spine and not the first five items of a list.

## 2.2 Then a wide fan-out

Once (5) merges, the following are genuinely independent of each other. Each touches the elaborator and the evaluator, and none needs anything another lane produces:

- `catch` / `throw`, and `tagbody` / `go` — D13's channels, second and third users.
- Multiple values — `values`, `multiple-value-bind`; per D18 the pivot did this with **no** new core node, which is the shape to reuse.
- Conditions and restarts — D19 says these enter on D13's channels, "which were designed for them." The rebuild reached R8 without them; this is where they land.
- The numeric tower — fixnums exist; floats, ratios and bignums are the parallel lanes D19 already framed, each gated on the evaluator with its own conformance evidence.
- Sequence and list library — `mapcar`, `reduce`, `append`, `nth`, and the rest. Unreachable before spine (1) and trivial after it.
- Strings and characters as *semantics* — the reader has the syntax since R3.
- Packages — D12 already observes that a package is a symbol table; DIV-0001 is the standing limit.
- CLOS, through a static-dispatch subset before the full protocol.
- `loop` and `format` — library-shaped, and last, because both are large and both are downstream of everything above.

Alongside those, five standing repairs that belong to whichever lane touches their file: DIV-0023 (`(block nil ...)`), DIV-0027 (no source position on elaborator/evaluator diagnostics), DIV-0022 (dotted-pair syntax), BL-0003 (implicit block elision as a `cata`), BL-0004 (retire the `cl/foundation` forwarding shims).

## 2.3 Why the discontinuity matters for how agents run

The spine wants sequential steps with disk handoff: each step's merge is the next step's precondition, and there is nothing for a second agent to do concurrently that would not be speculative.
The fan-out wants parallel lanes with independent merges, which is the structure `AGENTS.md` already anticipates when it says steps run in parallel lanes with a brief each.

The ratified answer for this planning pass is sequential disk handoff, which is correct for the spine and is where the work starts.
The transition point is named in proposed D25 rather than left to be discovered.

---

# 3. Proposed decision records

Numbering continues `docs/cl-rebuild-plan.md`, which owns D11–D21.
Each of these needs ratification before `docs/cl-language-plan.md` is written against it.

**D22 — Parity with `smdlisp` is the first phase, and retiring `smdlisp` is its merge criterion.**
`docs/cl-rebuild-plan.md` §8 left "whether `smdlisp` is eventually retired or kept permanently as the pivot's artefact" open, on the ground that it must survive at least until R6 could replace it as the behavioural oracle.
R6 has now done that.
This decision closes the question in the direction of retirement, and makes it a deliverable rather than a someday: phase one closes the gap in §1's table, using `smdlisp` as the differential oracle throughout, and the phase is not done until `src/smd/smdlisp/**` can be deleted from trunk without losing evidence.

The reason to spend a phase re-treading known ground: every step of the spine has, in `smdlisp`, a working reference implementation of the same operator with the same reader in front of it — a cheaper and more exact oracle than SBCL for exactly the semantics that are subtlest, because it already made this project's choices.
Throwing that away before using it is the waste, not the re-treading.

Two limits on D22, so it does not become an instruction to copy.
`smdlisp` is the oracle for *behaviour*, never for *shape*: its 79 raw index loops and 23-arm visits are what the rebuild exists to not have, and D15 governs the implementation.
And the four tests listed under "Tests that pin a defect" in `docs/divergences/README.md` are anti-specifications; where `smdlisp` pins a `defect`, SBCL is the oracle and `smdlisp` is a wrong answer by design (`docs/cl-rebuild-plan.md` §4, tier 3 over tier 4).

**D23 — The function value channel lands as one step, and it is the spine's root.**
D19's staging rule is "do not add a channel with no producer, and do not add a producer whose only consumer is the same step," which is why DIV-0025 was accepted at R5 and R6.
The rule is satisfied, not broken, by landing the `value` alternative, `lambda` in expression position, `#'f`, `funcall` and `apply` together: that is one channel with a producer and two consumers, all witnessed in the same step.
Splitting it would produce exactly the half-built channel D19 forbids.
This step is the precondition for every other item in §2, and nothing may be scheduled before it.

**D24 — Macroexpansion is a datum-to-datum pass between the reader and the elaborator.**
*This is the one proposed decision with a real alternative, and it should be ratified deliberately rather than nodded through.*

The proposal: a macroexpander that rewrites the reader's `tagged_tree` datum into another datum, running to fixpoint before elaboration begins, leaving the elaborator's three schemes (D15) untouched.
Backquote is a macro under this scheme, not reader syntax that builds core forms.

For it: the elaborator is the rebuild's cleanest artefact and this keeps it so; expansion becomes another tagged-tree traversal and therefore another user of the schemes rather than a new kind of pass; `macroexpand-1` and `*macroexpand-hook*` are later exposures of structure that already exists, which is the same argument D19 made for the readtable.

Against it: it means a second full tree-building pass over a fixed-capacity arena, and the capacity question is real — a macro-heavy program's expanded datum is unboundedly larger than its source, which interacts with BL-0002 and with D14.
The alternative is expansion interleaved into the elaborator's descending scan, which avoids the second arena but puts a fixpoint loop inside a scheme that currently has none.

`smdlisp` did the former with `macroexpand/expander.hpp`, and did it before the columnar tree existed. That is evidence but not a decision.

**D25 — A step joins the parallel fan-out only when it needs nothing another unlanded step produces, and the transition point is named in the plan.**
The spine of §2.1 runs as sequential steps with disk handoff.
The fan-out of §2.2 runs as parallel lanes in worktrees with their own briefs, and the plan states which merge opens it — under the current sketch, the macro step's.
A lane that discovers a dependency on another lane mid-flight stops and says so in its brief rather than reaching sideways; that is the failure mode this decision exists to make visible early rather than at integration.

**D26 — The corpus leads the lane.**
A lane's conformance entries are written and failing before its implementation begins, derived from the specification and checked against SBCL, not written afterwards to describe what was built.
R6 made this affordable — `conformance/corpus.hpp` states expected outcomes by channel, and the SBCL oracle already runs — and D16 already forbids the alternative in its strong form ("correctness evidence comes from a corpus derived from the standard... not from tests written against this implementation's behaviour").
D26 is the operational version: the corpus is the lane's brief, and a lane is done when its entries pass, not when its author judges it complete.

---

# 4. The phase list, in sketch only

Not a plan. Shown so §3's decisions can be seen to have consequences, and so the ratifier can see what they would be ratifying into.
Numbering continues R0–R8 as C-steps, so a reader can tell at a glance which plan a step belongs to.

Spine, sequential: C1 function values (D23) · C2 lambda lists · C3 `let`/`let*`/`setq` · C4 special variables · C5 macros (D24).
Transition (D25) at C5's merge.
Fan-out, parallel lanes: `catch`/`throw` and `tagbody`/`go` · multiple values · conditions and restarts · numeric tower (itself several lanes) · sequence library · string and character semantics · packages.
Then, gated on the above and not scheduled here: CLOS subset, `loop`, `format`.
D22's retirement of `smdlisp` is the merge criterion of the last spine-or-fan-out step that closes §1's table, wherever that falls.

Every step ships a post, and its author is not its implementer (D20, unchanged).
Post numbering continues from 32.

---

# 5. Open questions

Carried forward from `docs/cl-rebuild-plan.md` §8 and `docs/cl-rebuild-closing-note.md`:

- Whether `ansi-test` becomes more usable as strings, characters and the tower land. R6 answered it empirically for its own subset only. D26 makes this a per-lane question rather than one global one.
- Whether the Lisp-family layer of the kit (`node_f`, `topo_fold`, `tagged_tree*`) is worth extracting, given one client. Untouched by anything here.
- `parser/` has no `cl` client (DIV-0028). Untouched by anything here, unless a lane rewrites the reader, which none should.

New, and needing answers inside the phases that reach them:

- **Where the capacity limits for an expanded datum arena come from**, if D24 is ratified as proposed. Interacts with BL-0002 and D14.
- **Whether the closure representation from C1 survives conditions and restarts**, or whether the fan-out's condition lane forces a second look at it. This is the most likely place for D25's "lane discovers a dependency" failure to actually happen, and naming it now is cheaper than discovering it at integration.
- **Whether `defmacro`'s lambda list is C2's or C5's.** DIV-0010 classifies the lambda-list subset as `scope-decision` and the per-form scope as `defect`; the two halves may not want the same step.

---

# 6. What happens next

Two artifacts, in order, and the second is not started until the first is ratified.

**1. This document, ratified.** Specifically: D22 through D26 accepted, amended or rejected, with D24 given a deliberate answer rather than a default one.

**2. `docs/cl-language-plan.md`,** written against the ratified decisions — the successor plan proper, in the form `docs/cl-rebuild-plan.md` established: why, the decision records, what the divergences say now, test architecture, the phase list, the blog column, open questions. Plus `checklist.md` entries for its steps.

**3. The execution fan-out,** produced by the `plan-fanout` skill once the phase list is fixed: a `tmp/plan/` directory of `AGENT-PROMPT.md`, `checklist.md`, `step-NN.md` files, `README.md` and `INTEGRATION-REVIEW.md`, for cleared agents to execute in sequence with disk-based handoff, per the ratified mechanism and `AGENTS.md`'s step-brief contract. Each step's merge criterion is `make test-matrix`, both legs, per `docs/verification-matrix.md`.

---

# Amendment, 2026-08-23: D32 overrides D22's sequencing

**Note under D22 (§3):** D32 below supersedes this decision's sequencing — see D32. D22's goal survives; only the order changes.

**D32 — `smdlisp` is retired to a tag before the parity phase, not as its merge criterion.**
It supersedes D22's sequencing and leaves D22's goal intact: the C-series still closes § 1's table, it just does so without `smdlisp` in the worktree.
`tmp/plan/` step A3 deletes `smdlisp` from trunk before the C-series' first step opens, which is a direct override of D22's ordering and earns a decision record of its own rather than arriving as a side effect of tidying up.

- **The deciding reason is agent friction, and it is the owner's.** Agents keep getting hung up on not touching the dead trees and doing strategically wrong things, sometimes silently, while the trees are there to be gotten hung up on. That cost is paid continuously; D22's benefit is not.
- **D22's oracle argument was measured before being overridden, and it was substantially right.** `conformance/corpus.hpp` holds 75 entries, and because `satisfies()` runs `evaluate_program()` they reach exactly `cl`'s seven special operators and eighteen builtins and nothing else. `smdlisp` holds roughly 120 source-in / value-out evaluated cases covering precisely the gap — `LET`, `LET*`, `SETQ`, `LAMBDA`, `FUNCALL`/`APPLY`, `DEFMACRO` and backquote, `CATCH`/`THROW`, `TAGBODY`/`GO`, `DEFVAR`/`DEFPARAMETER`, multiple values. The corpus has none of it. What D22 wanted to keep is real.
- **But `smdlisp` supplies source programs, never expectations.** D16 says correctness comes from external authority; D22's own second limit says that where `smdlisp` pins a `defect` it is "a wrong answer by design"; D26 says a lane's entries are "derived from the specification and checked against SBCL, not written afterwards to describe what was built." Harvesting its answers would need a provenance kind meaning "our earlier implementation agreed," putting "this implementation agrees with itself" back into the one artefact R6 built to remove it. SBCL is at `/usr/bin/sbcl` and the harness already runs, so every entry would be re-adjudicated against SBCL anyway.
- **A tag supplies those source programs losslessly and for free.** After A1, `git show iteration/smdlisp-final:.../closure/eval_direct.test.cpp` prints all 96 of its cases. Harvesting now would land ~120 entries for operators that do not elaborate — a red build, or inert data, and inert data is what the tag already is.
- **So the harvest is deferred to its consumer, which is where D26 puts it.** Standing obligation on every C-series lane, in these words: before writing a lane's conformance entries, read the corresponding `smdlisp` test file out of `iteration/smdlisp-final` for source programs, and take every expectation from SBCL or the specification.

**Provisional.** Revisit if a C-series lane finds that reading source programs out of a tag is awkward enough in practice to be worth a real harvest step for that one file. An abstraction nobody dared touch and one nobody needed to touch look identical from outside.

D32 does not need `DIV-0032` confused with it: `D32` is this decision record; `DIV-0032` is `tmp/plan/step-A3.md`'s reserved divergence number, and the two are unrelated. Step A3 executes D32 and appends the dated "executed at ..." note here; this amendment writes the decision, A3 does not re-write it.
