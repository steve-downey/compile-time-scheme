# Integration review — fourteen steps, read as one change

Run 2026-09-11, against `cl-retire-trees` (Phase A, merged to `main` at
`a356738`) and `cl-parser-combinators` (Phase B, tip `8d15c70`, **not** merged).
Merge base `d6ae364`. Read-only; nothing was built and no source file was
touched by this step.

**Verdict up front.** Phase B is ready to merge, with one mechanical conflict to
resolve and two findings the owner should see first. Neither is a defect in the
code. The most important thing this run produced is not in section 1 — it is the
calibration in section 4, and it says something different from what the brief
predicted.

---

## 1. Cross-step drift

### No abstraction was forked. This is the headline and it is unqualified.

The plan's hardest rule held under six different agents across seven steps. The
five types with forking pressure on them were checked individually against the
final tree:

- **`cursor`, `parse_state`, `advance_while`** moved once, in B2, to
  `src/smd/kit/parser/cursor.hpp`. `src/smd/cl/reader/cursor.hpp` is a pure
  `using`-shim — three `using` declarations and two includes, no type of its
  own. No converter exists because there is nothing to convert between.
- **`result`** is unchanged. `src/smd/cl/foundation/result.hpp` remains R8's
  forwarding shim over `smd::kit::foundation::result`, and the parser layer uses
  the kit one directly. One type, two spellings, no second definition.
- **`parse_result<T>`** is a type *alias*
  (`src/smd/kit/parser/parser.hpp:22`): `foundation::result<parse_state<T>>`.
  Not a new type.
- **`child_list`** has exactly one definition repo-wide
  (`src/smd/cl/foundation/tagged_tree.hpp:122`) and one alias to it
  (`src/smd/cl/reader/detail/read_context.hpp:64`).

No nonce type with a good name appeared anywhere in the Phase B diff. Nor did an
abstraction used only after step N: the primitive set closed at B4 and B5, B6
and B7 each consumed it and added nothing.

### One genuinely dead abstraction, and it is not the one the brief expected

`parser_like` (`src/smd/kit/parser/parser.hpp:26`) is defined and **used
nowhere** — not in the kit, not in `cl`, not in any test file. It landed in B2
and survived six steps without acquiring a caller. B2's step file warned that a
vacuous concept would be worse than none; this is the concept that is one, and
it sits eighty lines above `map`'s doc comment arguing against exactly this
("a typeclass instance with no second caller is the over-eagerness this project
already learned to avoid"). Delete it or give it a caller.

`alt` (`src/smd/kit/parser/choice.hpp:64`) is a near miss of the same shape: a
named alias for `operator|` called from `choice.test.cpp` three times and from
no production site. Defensible as a call-site spelling; worth noticing that it
is the third thing in two files to be defended on grounds the same files
elsewhere reject.

### `parse_context` is doing work by B7 — but the work moved, and what holds it up now is a convention

The brief's specific worry was that `parse_context` would end up decoration.
It did not. It excludes exactly one type, `cursor`
(`src/smd/kit/parser/parse_context.hpp:21`), and that exclusion is the entire
argument-order guard for a two-parameter call where both parameters are ordinary
value-like types.

What changed at B7 is *where* the guard lives. Before B7, `parser<F>` held its
callable and forwarded through its own `operator()`, which could state the
constraint. After B7, `parser<F>` privately inherits `F` and re-exports
`F::operator()` — so the wrapper has no signature of its own, and the constraint
survives only because every callable in the kit and its one client happens to
spell its second parameter `parse_context auto &` (or a refinement). `parser.hpp`
is honest about this at lines 52–62 and explains why the wrapper could not
restate it: "a wrapper that re-stated it could only do so by being a call, and a
call is the cost this removes."

That reasoning is correct and the trade was forced. But the standing consequence
should be said plainly: **one lambda written `(cursor c, auto &ctx)` reopens the
hole silently, and nothing in the build would notice.** That is a fair backlog
entry, not a blocker.

B3's thread resolved cleanly and is written down: `readtable const` needed
nothing added to `parse_context` — it models it as-is, by not being a `cursor`
(`src/smd/cl/reader/read.hpp:95`, and B8's architecture section says the same).
`reader_context` refines rather than forks it
(`detail/read_context.hpp:82-89`). Seven functions plus `read_node`'s forward
declaration are constrained by it, which matches what B8 reported exactly.

### Naming across the parser layer: one principle for most of it, two for the repetitions

`skip_many` and `many_until` are named on different principles. `skip_many`
names what it does to values; `many_until` names its stopping condition. Both
return `parse_result<std::monostate>` and **neither accumulates**, so a reader
coming to the header cold cannot tell from the names that `many_until` also
discards. B8's architecture section defends `many_until` against `many_bounded`
well — the capacity check stays in `read_delimited` so the overflow position
stays exact — but never addresses the pairing with `skip_many`. If they were
named on one principle they would be `skip_many` and `skip_many_until`, or
`repeat_while` and `repeat_until`. This is small and it is the only naming
inconsistency found.

`alt` / `operator|` are consistent: one implementation, one alias, the alias
documented as a call-site spelling.

### The amendment mechanism was never exercised, and that is not evidence of anything

Zero `amendment-NN.md`. Zero `blocked-NN.md` on disk (A3's one block was
recorded in `metrics.jsonl` and cleared within the step; it was a local
tool-permission classifier refusing `git rm`, not a plan defect).

The inverse test the brief asks for comes back clean: no nonce types, and no
step widened its scope silently. The three `read.test.cpp` additions were each
declared in `out_of_scope` and named in the handoff, which is the lower-tier
mechanism working exactly as designed.

But **three real design revisions happened and none of them used the amendment
mechanism**, so the count of zero should not be read as a decomposition score:

1. **B3 revised `skip_many`'s contract mid-step** — `skip_many(skip_many(p))`
   hit a constexpr evaluation limit, and the fix was a no-progress guard. B3's
   own primitive inside B3, so no amendment was owed.
2. **B7 reshaped `parser<F>` itself** — a revision of *B2's* design, forced at
   step 7 by `-fconstexpr-depth`. This is precisely the case section 5 asks
   about. It was handled as an `out_of_scope` entry plus a commit. The mechanism
   was bypassed by a cheaper one that worked.
3. **`typeclass-resync` revised B2's `parser_instances.hpp`** and found a real
   defect in `derive_monad`. Handled by inserting a whole out-of-band step.

So the mechanism is unexercised and untested. Two of its three occasions were
handled by other means that were adequate. That is a genuine finding about the
mechanism, not about the decomposition.

### What the same-step-as-first-consumer bet actually bought

The plan deliberately placed each primitive in the step with its first consumer:
`bind` with `read_radix_number` (B2), `satisfy`/`char_p`/`skip_many` with the
skippers (B3), `operator|`/`optional`/`many_until` with `read_delimited` (B4).
Three later steps consumed and extended nothing.

The bet paid, and the most transferable lesson is in *what it did not cover*.
The one thing that did need revising at step 7 was not a combinator — it was
`parser<F>`, the wrapper type. It had no "first consumer" to be co-located with
because everything consumes it. The layered P1/P2 alternative in
`docs/cl-parser-scoping.md` § 4 would have got that wrong in exactly the same
way, at exactly the same distance, because the problem was never where the
primitives sat. **Co-locating a primitive with its first consumer validates the
primitive's interface and says nothing about the substrate it sits on.** That is
worth carrying into the C-series plan.

---

## 2. Did the context discipline hold

**Mostly yes, with one structural contradiction in the contract itself.**

### Handoffs: forward-only, in shape and in fact

All thirteen survive and none reads like a log. Spot-read in full: `handoff-B4.md`
(29 lines) and `handoff-B7.md` (152 lines). Every heading in both is "what you
need to decide" — `## What this means for your bounded collecting repetition`,
`## You are the last chance for this claim to be wrong`,
`## Your step file's spot check is wrong as written`. No measurements, no log
excerpts, no "what I did" summary anywhere.

Sizes: 29–152 lines against a ~150 target. Three exceed it slightly
(`handoff-B7.md` 152, `handoff-B8.md` 149, `handoff-review.md` 167). The Phase B
handoffs grow monotonically — 84, 74, 92, 29, 148, 130, 152, 149 — with the back
half roughly double the front. That is the conversion steps having more to say
than the construction steps, not drift; but it is the trend to watch if a
C-series runs longer than eight steps.

### The read contract contradicts itself

`tmp/plan/AGENT-PROMPT.md:44` — "**Do not** read other steps' files."
`tmp/plan/AGENT-PROMPT.md:68` — "Read the **next** step's file and write **one**
handoff for it."

Both are load-bearing and they cannot both be followed. In practice item 10 won,
which is right — a handoff written blind to its recipient is worth less — but
the consequence is that **every worker's Tier-2 context held two step files, not
one**, and the plan's largest (`step-B2.md`, 23.9 KB) was read twice. The
prohibition should be narrowed to "any step file other than yours and the next
one," or the handoff-writing should move to the orchestrator. As written, the
contract asks every worker to break it.

### The tracker stayed out of every step's diff — perfectly

```
git log 094f065..a356738 -- tmp/plan/     → empty
git diff d6ae364..8d15c70 -- tmp/plan/    → empty
```

Fourteen steps, two phases, zero leakage. The absolute-path convention worked.

**The *other* checklist was missed twice.** `48b6502` ("checklist: tick step A5
in the root checklist (missed in the worktree diff)") and `4637b59` (same for
B4) are orchestrator repair commits. Two of fourteen steps forgot the tick that
*was* supposed to be in their diff. The two-checklist design has an asymmetry
cost: a worker who has internalised "the tracker never enters my diff" is primed
to get the other one wrong, and the two failures are both in that direction.

### Transclusions: every one resolves, including A1's, checked against the final tree

- `scripts/verify-transclusions.sh` on `main`: exit 0, **145 resolved, 1
  discrepancy, 0 failures**. The one discrepancy is the documented pre-existing
  WARN on `docs/blog/phase-12-cps.org`, which predates this plan.
- All **25** living `#+transclude:` targets in `docs/compiler_architecture.org`
  at `8d15c70` resolve — each UUID verified to occur at least twice in the named
  file at that revision. All **9** on `main` likewise.
- **A1's work, checked against the final tree rather than the one it ran on:**
  `docs/history/architecture-iterations.org` carries 31 `orgit-file:` links
  pinned to `iteration/smdscheme-final` and `iteration/smdlisp-final`. Both tags
  resolve. The script covers that file by explicit glob
  (`scripts/verify-transclusions.sh:44`), in the pinned category, and it is
  green. A1 moved the prose out *before* A3 deleted the trees, which is the only
  order that works, and nothing in B1–B8 disturbed it.
- **Anchor inventory, verified independently of B8's claim:** 17 UUID anchors
  exist under `src/smd/kit/parser/` and `src/smd/cl/reader/` at `8d15c70`. 16
  are transcluded by `compiler_architecture.org`; the 17th
  (`datum.hpp`'s `23ee18c4…`) by the pinned phase-26 post. Zero orphaned, zero
  dangling. B8's statement is exactly right.

**One gap worth naming.** `src/smd/kit/parser/parser.hpp` carries **no anchor at
all**, and neither does `cursor.hpp` or `parse_context.hpp`. So the living
architecture document describes `parser<F>` — the type B7's most consequential
edit reshaped, and the thing the whole layer is — entirely in prose, with no code
shown. That is legal under D21 and it is the one place the document's narrative
outruns its evidence.

---

## 3. The claims each phase made

### Phase A claimed nothing was lost. It holds.

```
iteration/smdlisp-final       → resolves
iteration/smdscheme-final     → resolves
git show iteration/smdscheme-final:src/smd/smdscheme/parser/parser.hpp  → prints
git show iteration/smdlisp-final:src/smd/smdlisp/smdlisp.hpp            → prints
```

`docs/history/architecture-iterations.org` pins 31 transclusions to those two
tags and every one resolves (above). `docs/blog/pins.md` explains why
`iteration/*` is a second tag family and must not sort among `blog/phase-*`.

### Phase B claimed no observable behaviour changed. It holds, with one qualification worth stating.

```
git diff d6ae364..8d15c70 -- 'src/smd/cl/reader/token.hpp' 'src/smd/cl/reader/number.hpp'
```

**Exactly empty.** `scan_token` and `classify_number` were never touched, in any
of the seven steps. DIV-0003 is structurally safe, not merely tested, exactly as
`docs/cl-parser-scoping.md` § 4 promised and as B8 reports.

The test diff is one file, +56/−1. Reproduced independently by extracting every
`fails_with(input, message)` pair at both revisions and comparing as sets:

| | count |
|---|---|
| at `d6ae364` | 17 |
| at `8d15c70` | 21 |
| removed | **0** |
| changed | **0** |
| added | 4 |

The four additions are `#\`, `"abc\`, `#| unterminated`, and the nested
`#| outer #| inner |# still open`. The one deleted line is a statement
terminator becoming a conjunction. B8's account is correct in every particular.

**The qualification.** `fails_with` (`src/smd/cl/reader/read.test.cpp:130`)
asserts the *message only*:

```cpp
return !r.has_value() && std::string_view{r.error().message} == message;
```

Error **position** is asserted in exactly three predicates in the whole file —
`error_positions_track_lines`, plus `string_capacity_boundary` (added by B5) and
`wrapped_branch_error_sits_at_the_marker` (added by B6). So D31's guarantee is
proved for message text across 21 inputs and for position across three.

Every positional argument the step files make — `skip_many`'s exhaustion-is-
success so an unterminated `#|` reports at end-of-input rather than at the
comment; `many_until` deliberately not owning the capacity check so
`read_delimited`'s overflow diagnostic keeps its exact post-skip position — is
*reasoned* rather than tested, outside those three points. It is good reasoning
and I found nothing wrong with it. It is still the residual risk in Phase B, and
it is the only one. Note that B5 and B6 each added a position assertion of their
own accord, which is the discipline working; nobody was asked to.

Each of the nine distinct diagnostic strings was located in the final tree.
`"unterminated |"` still comes from `src/smd/cl/reader/token.hpp:139`, untouched.
The other eight now come from the new `detail/` headers —
`forms.hpp:115`, `node.hpp:48`, `node.hpp:86`, `text.hpp:60`, `text.hpp:180`,
`sharpsign.hpp:137`, `:172`, `:182`, `:196`, `read.hpp:108` — and each reports at
the position the step file said it would.

---

## 4. The measurements

Sixteen rows in `metrics.jsonl`: three baselines, fourteen plan steps (A3 twice,
once blocked once green), and the out-of-band `typeclass-resync`.

### Per step

| step | wall s | verify s | verify % of wall | verify log bytes | attempts | files | +ins | −del |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| A0 | 925 | 39 *(+439 baseline)* | 4.2% *(51.7%)* | 484 615 | 1 | 9 | 158 | 34 |
| A1 | 821 | 40 | 4.9% | 484 631 | 1 | 6 | 291 | 246 |
| A2 | 911 | 70 | 7.7% | 521 707 | 1 | 23 | 12 | 1 302 |
| A3 *(blocked)* | 354 | 354 | 100% | 538 787 | 1 | 0 | 0 | 0 |
| A3 *(green)* | 354 | 28 | 7.9% | 271 179 | 1 | 173 | 183 | 31 155 |
| A4 | 926 | 13 *(+159 baseline)* | 1.4% *(18.6%)* | 104 973 | 1 | 9 | 661 | 3 |
| A5 | 687 | 20 *(+154 baseline)* | 2.9% *(25.3%)* | 105 478 | 1 | 8 | 228 | 54 |
| B1 | 695 | 55 *(+162 baseline)* | 7.9% *(31.2%)* | 106 923 | 1 | 12 | 735 | 537 |
| B2 | 1 474 | 53 | 3.6% | 114 082 | 1 | 17 | 1 004 | 116 |
| B3 | 266 | 89 | 33.5% | 120 714 | **2** | 9 | 429 | 38 |
| B4 | *(186 622 — bad)* | 25 | — | 127 513 | 1 | 7 | 536 | 32 |
| typeclass-resync | 2 027 | **2 027** | **100%** | 142 008 | 1 | 5 | 139 | 12 |
| B5 | 3 420 | 43 | 1.3% | 135 636 | 1 | 4 | 199 | 54 |
| B6 | 1 238 | 43 | 3.5% | 135 040 | 1 | 5 | 140 | 48 |
| B7 | 3 150 | 198 | 6.3% | 135 430 | **2** | 8 | 315 | 109 |
| B8 | 1 501 | 11 | 0.7% | 129 899 | 1 | 4 | 96 | 3 |

### Whole run

- **Wall: 18 749 s ≈ 5 h 12 m** across fifteen executions. B4's
  `wall_seconds: 186622` is discarded — its own note says a sandbox clock-date
  rollover made it untrustworthy, which is the right call and correctly
  recorded.
- **Recorded verify: 3 108 s, 16.6% of wall.** Adding the cold baselines that
  steps ran but did not put in the `verify` field (439 + 159 + 154 + 162):
  **4 022 s, 21.5%.**
- **Verify log produced: 3 658 615 bytes (3.5 MiB). Verify log read: ~90 lines**
  (`summary_lines_read` is 6 on thirteen rows, 3 on one, 9 on one). That ratio —
  228 KiB produced per step, six lines consumed — is the context discipline
  working, and it is the single cleanest number in the file.
- **Diff: 299 file-changes, +5 126 / −33 743.** A3 alone is 173 files and
  −31 155, i.e. **92% of every deletion in the run**.
- **Attempts: 18 across 15 executions.** Two retries (B3, B7), one block. Block
  rate 1 in 15, and the block was a local tool-permission classifier refusing
  `git rm`, not a plan defect.
- **Amendments: 0. Divergence records created: 2** (DIV-0029 at A0, DIV-0034 at
  A5), from **14 reserved**. DIV-0030–0033 and DIV-0035–0042 went unused. An
  unused reservation is expected; twelve of fourteen unused is worth saying out
  loud, because it means the reservation scheme cost fourteen numbers to avoid a
  collision that a serial run could not have had.

### Which steps ran hottest, and why — and where the brief's prediction was wrong

The brief predicts that **A0 and A1, documentation-only, would have paid full
matrix cost and that this would be the run's most actionable finding.** The
numbers say something different, and the difference matters.

A0's *after-edit* verify was **39 s**. A1's was **40 s**. Neither is expensive.
What A0 actually paid was a **439-second cold `make test-matrix` on an
unmodified tree, before touching anything** — 47% of its 925-second wall. That
run proved the previous merge was green, which the previous step had already
proved.

So the actionable finding is not "the per-step verify command is too coarse". It
is:

> **The pre-edit GREEN baseline is the expensive half of verification, and for
> every step after the first of a phase it re-proves something already proved.**

`AGENT-PROMPT.md` item 3 mandates it. It is the right instinct — a worker should
not debug a failure it inherited — but it is paid at full cold-worktree price:
`git worktree add` + `git submodule update --init --recursive` + CMake configure
+ Catch2 fetch + two full build legs. Measured cold-worktree baselines in this
run: 439 s (pre-A3), then 159 s, 154 s, 162 s (post-A3). The *warm* figure for
the same command, same tree, is 40–43 s (`00-baseline`'s note, and
`00-baseline-3` at 57 s).

**Two steps genuinely were verify-bound**, and neither for the reason the brief
anticipated:

- **`typeclass-resync`: 2 027 s wall, of which 2 027 s is verify — 100%.** Five
  files, +139/−12. The highest verify-to-diff ratio in the run by a wide margin,
  and 4.6× the cold baseline, because merging `main` invalidated the whole build.
  This is what a mid-plan merge from trunk costs here. Nothing else in the run
  comes close.
- **B7: 198 s verify**, four times any other Phase B step, because it edited
  `src/smd/kit/parser/parser.hpp` and every translation unit that includes the
  layer rebuilt.

Everything else in Phase B verified in **11–89 s**. Splitting any of those steps
would have made things strictly worse — it would have bought a second cold
worktree at 150+ s to save nothing.

### What Phase A's deletion did to the cost of everything after it

This is the rare clean before-and-after, and it is the reason the later steps
look cheap:

| | ctest entries per leg | cold `make test-matrix` |
|---|---:|---:|
| before A0 (`00-baseline-2`, the sizing row) | 1 123 | 440 s |
| after A0–A2, before A3 | 1 105 | 354 s |
| **after A3** | **304** | **159 s** |
| after A4, A5 | 310, 311 | — |
| after Phase B (`8d15c70`) | **379** | ~162 s cold-worktree |

**A3 cut the per-step verify baseline by 64%, and it did it at step four of
fourteen.** Every step from A4 onward was sized against a build that A3 had made
cheap. That is the strongest argument in the whole run for the plan's most
contested decision — running the deletion third rather than last.

### Two "cold matrix" numbers that are not the same measurement

`metrics.jsonl` records cold-worktree matrix runs at 154–162 s post-A3. B8's D30
section records 65 s before / 91 s after, "from an empty build directory with
`ccache` disabled". Both are correct and they measure different things: the
metrics figure includes `git worktree add`, submodule init, CMake configure and
Catch2 FetchContent; D30's is compiler cost with the cache defeated. Only D30's
is a compile-time measurement. Nothing is wrong here, but a future reader
comparing the two will be misled, and only one of them is in `metrics.jsonl`.

### Step sizing: this run is the baseline, not a test of one

There was no prior calibration, so the question is what a step costs here.

- **B1 is the measured floor**: pure code motion, 12 files, +735/−537, no logic.
  **695 s wall, 55 s verify, plus a 162 s cold baseline.** So ~700 s per step, of
  which ~220 s is verification and ~480 s is the agent reading and editing.
- **Content barely moves it.** A0 (9 files, prose only) cost 925 s. B8 (4 files,
  prose only) cost 1 501 s — but most of that was the D30 measurement campaign,
  which its note says outright.
- **The spread is 266 s to 3 420 s**, a 13× range, and it does not track diff
  size. B5 (4 files, +199/−54) cost 3 420 s; A3 (173 files, −31 155) cost 354 s.
  A3 was a deletion an agent could execute in one pass; B5 was a resumed WIP from
  a session that had run out of budget. **Wall time here measures how much
  thinking a step required, not how much it changed**, and any future sizing that
  keys on file count will get it backwards.

So the guesses missed in one direction consistently: the plan sized steps by
scope of change, and the cost was driven by depth of reasoning. B5, B7 and
`typeclass-resync` — the three most expensive — are all steps where something
unexpected had to be understood before anything could be written.

### Every `out_of_scope` entry

Eight entries across five rows.

| step | entry | verdict |
|---|---|---|
| A3 | clang-format reflow of 14 provenance comments (`make lint` auto-fix) | **recurring** — see below |
| B2 | `reader/CMakeLists.txt` gains `kit.parser` | genuine new dependency, correctly declared |
| B2 | `monad_typeclass<parser<F>>` declared from `smd::kit::foundation`, not `smd::kit::parser` | a C++ fact the step file got wrong; the worker verified against GCC 16 and said so |
| B3 | `src/smd/cl/reader/read.test.cpp` | **recurring** |
| B5 | `src/smd/cl/reader/read.test.cpp` | **recurring** |
| B6 | `src/smd/cl/reader/read.test.cpp` | **recurring** |
| B7 | `detail/read_node_fwd.hpp` gains the `reader_context` constraint | forced — a differently-constrained declaration is a different template |
| B7 | `parser.hpp` reshaped; `parser.test.cpp` gains a case | the series' most consequential edit, correctly surfaced here |

**Two things recur, and the brief asks what a repository that keeps provoking the
same fix should do about it.**

1. **`read.test.cpp` three times (B3, B5, B6).** This is not the repository
   provoking a fix — it is the *plan* provoking one. `AGENT-PROMPT.md:256` says
   "Changing an existing test's expectation is a **halt**, always. Adding a new
   case" is fine; B5's, B6's and B7's spot checks each said the diff "must return
   nothing" for any `.test.cpp` under `src/smd/cl/`. Three workers hit the
   contradiction and all three resolved it the same correct way. The proper fix
   is in the template: a converting step that closes a previously untested
   diagnostic *will* add a case, so the step file should say so up front rather
   than forbid it and rely on the worker to notice.
2. **`make lint`'s auto-fixing hooks landing repairs outside the commit.** A3
   declared it, and the run carries five separate follow-up formatting commits:
   `9fb0152`, `a50345c`, `18afb36`, `08fcb02`, `118406f`. B3 filed
   `docs/backlog/BL-0008-lint-autofix-lands-unstaged.md` for it. That is the
   right outcome and it is the mechanism working — a repeated one-line workaround
   became a backlog item rather than a sixth workaround.

---

## 5. Process: four things that happened and are not in any step file

These bear on cross-step drift and on the design, and none of them is recorded
where a future planner would find it.

### The tracker was lost, and the design that lost it is not sound

`tmp/plan/checklist.md`, `metrics.jsonl` and every `handoff-*.md` live
uncommitted in the main checkout **by design**, so they cannot enter a step's
diff. Section 2 confirms the diff-hygiene half worked perfectly. The durability
half did not: a session ran out of budget mid-B5 and the only surviving copy was
a `wip/plan-state` branch someone had thought to push. It was restored on
2026-09-10.

**The design conflates two different goals and pays for both with one
mechanism.** "Must not enter a step's diff" is a property of the *worktree the
step commits from*. "Must survive a lost session" is a property of *storage*.
The plan achieved the first by sacrificing the second, and it did not have to:
the tracker could live on its own branch, or in a `.git/`-adjacent path, or —
simplest — be committed to `main` by the orchestrator between steps, which no
worker's worktree would ever see. `wip/plan-state` is in fact exactly that
solution, discovered under duress and still maintained ad hoc (its tip `4c0bf91`
sits on `8d15c70` and already carries `handoff-review.md`).

Note also the asymmetry cost already found in section 2: two of fourteen steps
forgot the root-checklist tick that *was* supposed to be in their diff. The rule
"the tracker never enters your diff" trained the exact wrong reflex for the other
checklist.

**Recommendation for the next plan: keep the absolute-path rule, and have the
orchestrator commit `tmp/plan/` state to a dedicated branch after every merge.**
One extra command per step, and the failure mode disappears.

### The out-of-band step was the right call, and it found a real defect

`step-typeclass-resync.md` was inserted between B4 and B5 because `main` had
moved eight commits ahead with a typeclass rename that left B2's
`parser_instances.hpp` naming two identifiers that no longer existed.

Two things to record. First, **the plan had no slot for it** — the checklist ran
A0–A5, B1–B8, and `AGENT-PROMPT.md:26` had to be amended mid-run to admit "any
slug-named maintenance step the orchestrator has inserted between them". A plan
whose phases branch off `main` and run for weeks will need this again; it should
be in the template, not improvised.

Second, **it found a defect nothing else would have.** `derive_monad` derives
`fmap` as `bind(ma, [&](a){ return pure(f(a)); })`, holding `f` by reference. A
strict instance runs the lambda before `fmap` returns; `parser<F>`'s `bind`
stores the continuation inside the parser it returns, so the reference is dead by
the time it is read. It was demonstrated both as an Asan stack-use-after-return
and as a constant-evaluation lifetime error, and it is now
`docs/backlog/BL-0010-derive-monad-fmap-captures-by-reference.md` on `main`, with
`parser_instances.hpp`'s native `fmap` (lines 58–79) as the fix and the
explanation. **No in-tree caller other than `parser<F>` could reach it.** The
first deferred instance in the codebase found a latent bug in shared
infrastructure the moment it arrived — which is the best argument available for
building a kit against one real client rather than against an anticipated
several, and it is the same argument B8 makes when dissolving DIV-0028.

### Four defects were found in the step files by the people executing them

| | defect | found by | fixed |
|---|---|---|---|
| a | every plan file hardcoded `/home/sdowney/src/steve-downey/compile-time-scheme/main`, a path that has never existed on this machine | first worker to run setup | orchestrator, mid-run, in all 17 files |
| b | `./.build/*/*/cl_reader_test` matches nothing — binaries are at `.build/build-gcc-16/src/smd/cl/reader/{Asan,Debug}/` — and `2>/dev/null` swallowed the failure | B1 (reported in its handoff) | not until B6 |
| c | B5/B6/B7 spot checks forbade touching any `.test.cpp` under `src/smd/cl/`, contradicting `AGENT-PROMPT.md:256` | B5, then B6, then B7 | B6, B7 |
| d | B7 expected `grep and_then` to reach zero repo-wide, which was never achievable | B6 | B7 |

**What this says about step files as an artefact.** All four are defects of a
kind the author structurally could not find: (a) and (b) are facts about the
machine, (c) is a contradiction visible only by holding two files open at once,
and (d) is a fact about code that did not exist when the step was written. A step
file is written against an imagined tree and executed against a real one, and the
gap is not closeable by writing more carefully. **The workers are the only agents
positioned to find these, and the plan's own contract forbids them from fixing
them** — a cleared worker cannot edit a step file, and (correctly) does not know
whether the file or its own reading is wrong.

The mechanism that actually worked was the handoff: B1 reported (b), B6 reported
(d) and (c), and each reached the orchestrator, who is the only party who can
edit a step file. That is the loop functioning. But note the latency: **(b) was
reported at B1 and not fixed until B6**, so A4, A5, B3 and B5 each ran a spot
check that silently passed while checking nothing.

Two sharpenings the brief did not have. First, (b) is **worse than "from B1"** —
it appears in `step-A4.md:308` and `step-A5.md:188` too, and in both cases the
command was the spot check for the SBCL differential those very steps were built
to create. Second, **defect (a) reached this brief as well**: section 5 of the
original `INTEGRATION-REVIEW.md` directed the memory entry to
`…/-home-sdowney-src-steve-downey-compile-time-scheme-…`, which does not exist.
The orchestrator's mid-run sweep fixed the step files; nothing swept the one file
that is read once, at the end, by the one agent with no predecessor to warn it.

**Recommendation: the plan's first step should be a smoke step that executes
every literal path and every spot-check command in every step file against the
real machine, and reports.** It costs one cheap step and it would have caught
(a), (b) and (d) before A0.

### SBCL was absent for part of the run, and B5 merged with the oracle contributing nothing

The reader differential (`src/smd/cl/conformance/reader_differential.test.cpp`)
is the plan's only outside oracle. It calls `SKIP` when `sbcl` is not on `PATH`
(lines 198, 208) — deliberately, and `sbcl_oracle.hpp`'s header comment explains
why: the test is in the default suite, so an environment without SBCL must stay
green. **ctest counts a Catch2 skip as a pass.**

A4 and A5 recorded agreement against **SBCL 2.2.9.debian**. It was gone by B5 —
whose metrics note says plainly "sbcl absent so all four differential tests
skip". It was reinstalled at **2.6.0.debian** before B6, and B6 recorded the
differential live at 180 assertions / 3 cases, identical before and after. It is
present now at 2.6.0.

**The verification consequence.** B5 converted `read_string` and `read_character`
— the two functions whose output the printer differential compares against SBCL
character for character — and merged on a green matrix in which that comparison
did not run. B6 re-ran it and it agreed, so nothing was actually lost; but B5's
merge was not entitled to the claim its green implied, and nothing in the plan
made that visible at merge time.

**Should anything in the plan have caught it?** Yes, twice over:

1. **The step's own spot check was supposed to.** `step-B5.md:221` runs
   `./.build/*/*/cl_conformance_test "*ReaderDifferential*" 2>/dev/null | tail -5`
   — defect (b). It printed nothing whether SBCL was there or not. The two
   defects compounded: a skipping oracle and a spot check that could not see it.
2. **No step file asked for the version until B6.** B6's step file now does it
   properly (`step-B6.md:203-226`): check `command -v sbcl`, record the version
   in `metrics.jsonl`, and — the part that matters —
   *"If your run reports far fewer assertions than that, the oracle is skipping
   and you have not checked anything."* An assertion-count floor is the right
   guard, because it is the one thing a skip cannot fake.

That paragraph should be promoted out of `step-B6.md` into
`AGENT-PROMPT.md` as a standing rule: **any check that can skip must be gated on
a floor, not on an exit code.** It is the most reusable thing the run produced
about verification.

### The fan-out never fanned out

Every step ran serially, one agent at a time, in a single orchestrating session.
`docs/cl-parser-scoping.md` § 4's P6–P8 were three parallel lanes (`token`,
`text`, `forms`); executed, B5 took `text`, B6 took `forms` *and* `token`
together, and nothing ran concurrently. Every `metrics.jsonl` row has
`"lane": null`.

Three consequences, stated without inflation:

- **The reserved-number scheme paid for a risk that never materialised.** Fourteen
  DIV numbers reserved up front so concurrent lanes could not collide; twelve
  unused. Blog phases 33–46 likewise. Serial execution makes both schemes pure
  overhead — the next free number is always available.
- **The one real collision happened anyway, and not between lanes.** `BL-0008`
  was assigned on `cl-parser-combinators` (B3, `0f019bd`) *and* independently on
  `main` (`7e89780`), because the two branches could not see each other. It was
  caught and renumbered to BL-0009, whose Items row says so. Numbers collide
  across *branches*, not across lanes, and the reservation scheme addressed the
  wrong axis.
- **The isolation cost was paid in full for no isolation benefit.** Fifteen fresh
  worktrees at 150–440 s of cold build each. A serial run could have reused one
  warm build directory at 40–43 s and saved roughly **25–40 minutes of the run's
  5 h 12 m** — a modest saving, and the honest framing is that per-step worktree
  isolation is cheap here, not that it was free.

The README says this run was always going to be the calibration. It was, and the
single most useful thing it calibrated is that **the serial path is the one that
actually got exercised, so the plan template should treat parallel lanes as the
special case and stop paying for them by default.**

---

## 6. What the owner needs

### D32 is stated consistently in all six homes, and there is no second D22

| home | where | says |
|---|---|---|
| `AGENTS.md` | lines 87–99, § "Which trees you may edit" | `cl` is the only tree; both predecessors deleted at A3, frozen at tags; names D32 as what retired the never-edited rule |
| `CLAUDE.md` | line 24 | same, verbatim in substance |
| root `checklist.md` | line 98 | freeze retired by D32 at A3; source programs only; D16 still governs where expectations come from |
| `docs/backlog/README.md` | line 35, § Rules | both trees deleted at A3 (D32), frozen at the two tags |
| `docs/cl-rebuild-plan.md` | § 8, "Whether `smdlisp` is eventually retired…" | **Resolved 2026-08-23: retired**, with the reason stated honestly as agent friction rather than oracle coverage |
| `docs/cl-language-scoping.md` | lines 187–207 | D32 itself, the note under D22, and the dated execution note with the 1105→304 figure |

**None of the six still asserts the freeze.** A seventh home nobody listed —
`docs/cl-limitations.md:155` — carries a dated note and is consistent.

**No second D22 exists.** `D22` appears only in `docs/cl-language-scoping.md` (its
own record, plus the override note and the measured-before-overriding paragraph)
and in one back-reference in `docs/cl-parser-scoping.md:189`. The earlier draft
that would have had A3 author the retirement as "D22" in `docs/cl-rebuild-plan.md`
§ 2 did not leak. A0's step file and the README both warned about the D32/DIV-0032
confusion at length and it did not happen either.

### D32's deferral held, and here is what would show it was wrong

`iteration/smdlisp-final` resolves and prints. D32 carries the obligation in
checkable words, and `docs/cl-language-scoping.md:196` measured the gap before
overriding it rather than after: `conformance/corpus.hpp` holds 75 entries
reaching `cl`'s seven special operators and eighteen builtins; `smdlisp` holds
roughly 120 evaluated cases covering `LET`, `LET*`, `SETQ`, `LAMBDA`,
`FUNCALL`/`APPLY`, `DEFMACRO` and backquote, `CATCH`/`THROW`, `TAGBODY`/`GO`,
`DEFVAR`/`DEFPARAMETER` and multiple values. That is a real inventory of what is
deferred, not an assurance.

**For the C-series planner — what would show D32 was wrong.** D32 preserved
`smdlisp` as *source programs*. It did not preserve a runnable oracle, and it
cannot: `docs/cl-language-scoping.md:207` records that `smdlisp.smdlisp` links
`smdscheme.foundation` and `smdscheme.parser`, and that six of `smdlisp`'s seven
sub-library `CMakeLists.txt` files link a `smdscheme.*` target directly. Standing
up a build at that tag means standing up *both* dead trees.

So the falsifier is specific: **a C-series lane that needs `smdlisp`'s
*answers* rather than its *inputs*.** If a lane finds that reading the 120 source
programs out of the tag is not enough — that it needs to run them and compare —
then D32 traded a cheap capability for an expensive one, and the honest response
is to reconstruct those expectations from SBCL or the specification (which D16
already requires) rather than to resurrect the tree. If, as expected, the lanes
only need the *programs* and take their expectations from SBCL, D32 was right and
the cost argument (1105 → 304 ctest entries, a 64% cut to every subsequent step's
verify baseline) stands as measured.

### The two divergence lists, reconciled into one

`docs/cl-parser-scoping.md`'s amendment of 2026-08-23 (written at A0, eight
items, about how *the plan* differed from § 4's P1–P10 sketch) and
`handoff-review.md` Part 5 (written by B8, thirteen items, about how *execution*
differed from the plan) overlap substantially. Merged, deduplicated, and ordered
so the owner can amend the note once:

**Structure and ordering**

1. § 4's P1–P10 assumed the three-tree repository; most of what made the sketch
   wide was keeping two dead front ends alive. What was P1–P10 became a
   fourteen-step series behind a precursor phase that retires them. *(A0-1)*
2. **The order inverted at the front.** The sketch builds the kit first (P1, P2)
   and splits `read.hpp` fourth (P4). Executed split first (B1), built the kit
   second (B2). Splitting first is what gave every later step a file it alone
   owned. *(B8-1)*
3. **P3 does not exist as a step**, and should be deleted from the sketch. Two of
   its three jobs happened inside B2: `src/smd/cl/reader/cursor.hpp` became an
   R8-style forwarding shim, and the duck-typed `Ctx` became the named
   `reader_context` concept. *(A0-2, B8-2)*
4. **The D30 "before" measurement did not happen at P3, by design.** B8 took it
   from the merge base instead, on the reasoning that git is more reliable than
   asking seven cleared agents to remember. Worth keeping: the depth measurement
   it produced is the one that mattered, and no step before B7 could have
   produced it. *(B8-3)*
5. **The split is eight `detail/` headers, not eleven.** `read.hpp` retains the
   two public entry points. *(B8-4)*
6. **One step the sketch has no counterpart for**: `typeclass-resync`, inserted
   2026-09-10 to carry `main`'s typeclass rename onto the branch. *(B8-12)*

**What the kit is, versus what P1/P2 asked for**

7. **P1 and P2 should not exist as written.** Building the layer in two steps and
   first using it at P5/P6 maximises the guesses validated too late. Executed
   added one primitive at a time in the step with its first consumer. *(A0-3)*
8. **The layer stopped growing after B4 — the plan's guess was right.** B5, B6
   and B7 each converted reader functions and added nothing: three independent
   consumers in a row. *(B8-9)* The layered P1/P2 version could not have produced
   that fact at all. *(A0-3)*
9. **It is a Monad first, not an Applicative first.** Landed `pure`/`map`/`bind`
   and a `monad<parser<F>>` registration. `parser<F>` satisfies neither
   `monad_impl` nor `monad_object` and structurally cannot: `operator()` is a
   template over the threaded context, so the parsed value type is not a property
   of the parser type and no `value_type` can name it. `apply` is therefore absent
   from overload resolution rather than wrong. *(B8-5)*
10. **§ 2's Gap 1 and D28 need a landed-code correction.** Both were true of the
    retired `smdscheme` layer. A Monad typeclass landed in
    `smd::kit::foundation` independently (`ae3c33a`), so the step registered a
    second instance rather than inventing `bind`. *(A0-6)*
11. **There is no `empty`, no Alternative instance, and no Alternative law
    tests.** P2 asks for them; nothing needed an identity for choice. *(B8-6)*
    `docs/backlog/BL-0009` now asks whether `alternative` belongs in the kit at
    all.
12. **There is no `parser_ops` facade, and § 4's P2 should stop promising one.**
    The retired facade had exactly one client, `smdscheme`, now deleted; a facade
    wants a second client first. *(A0-4)* Callers reach `bind` through the CPO,
    brought in by a `using` in `detail/read_context.hpp` because a CPO is an
    object and ADL will not find it from a `parser<F>` argument. *(B8-7)*
13. **`many`/`some` became `many_until`, and the naming is load-bearing.** What
    landed owns no capacity and no container; `read_delimited` keeps its own
    check so its overflow diagnostic keeps its exact position. The retired
    `many<Capacity>` truncated silently on both edges, which D31 forbids.
    *(B8-8)*
14. **B7 changed the shape of `parser<F>` itself**, which no sketch step
    anticipated: a parser now *is* its callable (`F` a private base, `operator()`
    re-exported) rather than holding one. Forced by constant-evaluation depth,
    not chosen. The most consequential single edit in the series. *(B8-13)*

**What held exactly as written, and should be said so**

15. **There is no numbers lane**, and **`scan_token`/`classify_number` stayed out
    of scope for the whole series** — verified above as an exactly empty diff, so
    DIV-0003 is structurally safe rather than merely tested. `token_p` only lifts
    `scan_token`'s existing result into the parser vocabulary. *(A0, B8-11)*
16. **Both § 5 traps were real**: `many<Capacity>` succeeds where `read_delimited`
    must diagnose; `skip_block_comment` lets an unterminated `#|` run to end of
    input where the natural combinator fails at the comment. Both are preserved
    deliberately and both are now pinned by tests B3 added. *(A0)*

**Corrections not about § 4**

17. § 3's D31 named `reader/oracle_compare.test.cpp` as an "SBCL differential".
    It never was one — it compared against `smdlisp`. A4 and A5 built a real one;
    A2 deleted the old file. The witness D31 asks for exists after Phase A and not
    before. *(A0)*
18. § 6 prescribes an amendment mechanism that no longer exists. The mechanism is
    a new decision record appended to the note that owns the decision being
    overridden, which is what D32 does for D22. *(A0)*

**Numbering**

19. § 4 reserves phases 33–42 and DIV-0029 through DIV-0033 for ten steps. The
    plan is fourteen: blog phases **33–46**, **DIV-0029 through DIV-0042**.
    *(A0-5)* In the event, **two divergence records were written** (DIV-0029,
    DIV-0034) and **no blog post** (see below).
20. **The fan-out never happened; the series ran serial.** P6–P8's three lanes
    became B5 (`text`) and B6 (`forms` *and* `token`), sequentially. *(B8-10)*

### DIV-0028's dissolution is the best writing in the run, and its classification is right

B8 left DIV-0028 as `process` and argued it from the README's own taxonomy —
"about workflow or tooling, with no bearing on the object language". That is
correct; nothing observable in Common Lisp ever depended on which module a
`cursor` lived in.

The dissolution itself is worth the owner reading in full
(`docs/divergences/DIV-0028-…md`, the 2026-09-11 section). It does the thing that
is hard: it notes that satisfying the revisit condition *showed the condition was
framed wrongly*, because the condition counted to three clients and Phase A
deleted two of them. "One client is what makes the layer worth having, not a
shortfall against it" is the correct reading, and it generalises — it is the same
observation BL-0010 makes from the other direction, where the first deferred
instance found a latent bug in shared infrastructure.

### Two things B8 flagged, both confirmed, neither a defect

- **`detail/read_context.hpp:78` is stale**: "Only `read_radix_number` is
  constrained with this concept in this step". Seven functions are, plus
  `read_node`'s forward declaration — verified by grep against the final tree.
  B7 offered it to B8; B8's step file forbade source changes. It has been outside
  every step's declared scope since B3. One-line backlog item.
- **`read_delimited` is the one reader function nobody owned.** Its `Ctx` is a
  bare `class Ctx`; only its inner step closure names `reader_context`. The seven
  surviving `and_then(` call sites in `src/smd/cl/reader/` are all `result`'s bind
  around a tree append or a capacity check, not a parse step. Correct as it
  stands.

### The compile-time cost D30 recorded, which the owner should see as a number and not a paragraph

From `docs/compiler_architecture.org` § B8:

- Constant-evaluation depth per level of source nesting: **2.0 frames before,
  5.0 after.**
- Deepest form readable at the default `-fconstexpr-depth=512`: **236 before,
  93 after.**
- `-fconstexpr-ops-limit`: 7× increase, still 3% of the default budget. Never the
  binding limit.
- Per-translation-unit compile time for `read.test.cpp`: **1.24–1.26×.**

D30 says record, do not gate, and the section is properly framed as a record. But
"93" is a real number with a real edge, and B7's first attempt overshot 512 and
stopped the build with no warning — the margin before that step, unmeasured, had
been 88 frames. The section says this plainly, which is to its credit. It should
still have a watcher.

### Pre-existing rot named in `README.md`, for the backlog

None caused by this plan, none in any step's scope:

- **`schemepoc.org` transcludes `src/smd/schemepoc/*`**, a directory gone since
  the rename to `smdscheme`. It is not matched by any glob in
  `scripts/verify-transclusions.sh` (lines 44, 46, 50), which is why it has gone
  unnoticed. Either add it to the pinned category or delete the document.
- **`scripts/amalgamate.py` and `scripts/deploy_godbolt_tree.py`** both hardcode
  the same dead `src/smd/schemepoc/` path.
- **`make testinstall` exits 0 with zero installed tests** — already
  `BL-0005`, and its own row correctly calls that "worse than red".
- **Well over a hundred stale local branches**, including the nine-commits-behind
  `retire-three-trees` this plan had to route around.

---

## 7. Merge readiness

**Phase B is ready.** One mechanical thing to do first, and two things to know.

**The conflict.** `git merge-tree main cl-parser-combinators` produces exactly one
conflict:

```
CONFLICT (content): Merge conflict in docs/backlog/README.md
```

It is the Items table. `main` gained rows for BL-0009 and BL-0010 after the branch
was cut; the branch gained a row for BL-0008. Resolution is to keep all three
rows. Nothing else in 33 files conflicts.

**What is verified as of this review:**

- `src/smd/cl/reader/token.hpp` and `number.hpp`: diff exactly empty. DIV-0003
  structurally safe.
- 17 pre-existing `fails_with` expectations survive unchanged; four added; none
  removed or modified. The conformance corpus untouched.
- All 25 living transclusions resolve at `8d15c70`; 17 anchors, 0 orphaned.
- Both `iteration/*` tags resolve; 31 pinned links intact.
- D32 consistent across seven documents; no second D22.
- All fourteen plan lines plus `typeclass-resync` ticked in the root
  `checklist.md` on the branch.
- `tmp/plan/` appears in no step's diff in either phase.
- B8 reports both matrix legs green at 379 ctest entries with the SBCL oracle
  live at 180/3 and 300/5, before and after.

**What the owner should know before merging, neither of which blocks:**

1. **Error *positions* are pinned at three points, messages at twenty-one.** The
   step files' positional reasoning is sound and I found nothing wrong with it,
   but it is reasoning rather than test, and it is the only residual risk in the
   phase.
2. **The default-flags nesting ceiling fell from 236 to 93.** Recorded honestly
   and correctly not gated, per D30. It is still a capability regression and it
   deserves a backlog entry rather than only a paragraph in the architecture
   document.

**Fourteen blog posts are owed.** Phases 33–46 were reserved per step; the latest
post in `docs/blog/` is `phase-32-extracting-the-kit`, and no `blog/phase-33`
through `blog/phase-46` tag exists. This is by design — the README says "no
implementing agent drafts one; the anchors are what they owe" — but the arrears
are now fourteen posts on top of `docs/history/blog-backfill-plan.md`'s existing
five, and the anchors that would source them are in place and verified.

---

## 8. Backlog entries this review recommends

1. **`parser_like` is dead code** (`src/smd/kit/parser/parser.hpp:26`). Defined
   in B2, used nowhere in six steps. Delete it or give it a caller.
2. **The argument-order guard now rests on a naming convention, not a type.**
   After B7, `parser<F>` re-exports `F::operator()` and states no signature of
   its own; the guard survives only because every lambda spells its second
   parameter `parse_context auto &`. One lambda written `auto &ctx` reopens it
   silently. Consider a static check over the kit's own primitives.
3. **`fails_with` pins message but not position**
   (`src/smd/cl/reader/read.test.cpp:130`). Extend it to take an optional
   expected position, or add positional predicates for the diagnostics whose
   position the step files argued about.
4. **`detail/read_context.hpp:78` is stale** — "Only `read_radix_number` is
   constrained with this concept in this step". Seven functions plus a forward
   declaration are.
5. **The default-flags constant-evaluation nesting ceiling fell from 236 to 93.**
   Not a defect under D30; still a number that wants a watcher, since B7's first
   attempt overshot the limit with an 88-frame margin nobody was measuring.
6. **`schemepoc.org` transcludes a directory deleted at the `smdscheme` rename**,
   and is outside every glob in `scripts/verify-transclusions.sh` (lines 44, 46,
   50).
7. **`scripts/amalgamate.py` and `scripts/deploy_godbolt_tree.py` hardcode
   `src/smd/schemepoc/`.**
8. **Blog arrears: phases 33–46**, fourteen posts, anchors landed and verified,
   no tags cut.
9. **Prune the stale local branches** — over a hundred, including the
   nine-commits-behind `retire-three-trees` this plan routed around.
10. **`skip_many` and `many_until` are named on two principles.** Cosmetic;
    worth one sentence in `repeat.hpp` saying that neither accumulates, since the
    names do not.

## 9. Changes this review recommends to the plan template

These are for the next `plan-fanout`, not for this repository.

1. **Commit the tracker.** Keep the absolute-path rule that kept `tmp/plan/` out
   of every step's diff — it worked perfectly — and have the *orchestrator*
   commit the state to a dedicated branch after each merge. `wip/plan-state` is
   already that solution, found under duress.
2. **Add a step zero that executes every literal path and every spot-check
   command in every step file, and reports.** It would have caught defects (a),
   (b) and (d) before A0 instead of at B1, B6 and B6.
3. **Any check that can skip must be gated on a floor, not on an exit code.**
   `step-B6.md:203-226` is the model; promote it to `AGENT-PROMPT.md`.
4. **Never write `2>/dev/null` into a spot check.** It is the mechanism by which
   defect (b) survived five steps.
5. **Resolve the read-contract contradiction** at `AGENT-PROMPT.md:44` versus
   `:68`.
6. **Make the out-of-band maintenance step a first-class concept**, not an
   improvisation. A plan that branches off `main` and runs for weeks will need
   one.
7. **Treat parallel lanes as the special case.** Reserved numbering, lane fields
   and per-step worktrees all cost something and bought nothing in a serial run.
   The one collision that did occur was across *branches*, which reservation does
   not address.
8. **Size steps by depth of reasoning, not by scope of change.** A3 deleted
   31 155 lines in 354 s; B5 changed 199 in 3 420 s.
