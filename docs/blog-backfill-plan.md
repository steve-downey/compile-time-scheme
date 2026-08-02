<!-- markdownlint-disable MD013 -->

# Blog backfill plan (phases 23–27)

Companion to `docs/epistolary-pinning-plan.md`, which owns the pinning machinery, and to `docs/cl-rebuild-plan.md` §7, which owns the per-phase sequence from R5 onward.
This plan covers only the arrears: R0–R4 landed without posts.

## 1. What was dropped, and what went with it

The pivot made a blog post a named deliverable of its step (`docs/cl-pivot-plan.md`; `checklist.md` records eight of them).
The rebuild plan's phase list carried no such deliverable, so R0–R4 merged without one.
D20 (`docs/cl-rebuild-plan.md` §2) records the repair.

A second thing went with it, and it is the reason this is more than five drafting tasks.
Posts transclude live code by UUID anchor and never quote it, so the anchors are placed *for* the posts — and `src/smd/cl/**` has **zero** UUID anchors today, against 51 in `src/smd/smdlisp/**`.
There is currently nothing for a post about the rebuild to transclude.

## 2. Why tagging today's `main` will not do

The obvious shortcut is to land all the anchors in one commit and point `blog/phase-23` through `blog/phase-27` at it.
That reintroduces exactly the leak `docs/epistolary-pinning-plan.md` was written to close: a post showing code from a step later than the one it describes.

The drift is not hypothetical here.
R4 alone rewrote three headers that R1 and R3 created:

| File | Created | Later change |
|---|---|---|
| `foundation/result.hpp` | R1 | R4, +26 lines (`and_then`) |
| `core/ast.hpp` | R3, 111 lines | R4, +77 lines (four new node kinds) |
| `reader/read.hpp` | R3, 591 lines | R4, ±24 lines (the `and_then` using-declaration) |

A phase-26 post about R3's core AST, pinned to today's `main`, would transclude `core_leaf` showing `core_character`, `core_string` and `core_quoted_symbol` — three node kinds R3 did not have, in a post whose whole subject is what R3 built.
That is the phase-18 failure (`docs/blog/pins.md`, "The measured leak": +56 lines of step L14's code inside the post about L13) reproduced deliberately.

So each post needs a pin whose tree is its own step's vintage.

## 3. The construction: tags on the step merges

Each step landed its own UUID anchors at its merge, per the rule in
`AGENTS.md`: the anchors a step's post will transclude exist at that step's
merge commit, because that is what the post pins to.
So the pinning needs no construction beyond naming: four annotated tags,
`blog/phase-24` through `blog/phase-27`, one per step merge, placed
retroactively when the posts were written.

| Post | Step | Pin | Tag |
|---|---|---|---|
| 23 | R0 | — | none (no transclusions) |
| 24 | R1 | the R1 merge | `blog/phase-24` |
| 25 | R2 | the R2 merge | `blog/phase-25` |
| 26 | R3 | the R3 merge | `blog/phase-26` |
| 27 | R4 | the R4 merge | `blog/phase-27` |

Three things were verified rather than assumed.
Each of the four merges compiles and passes its whole suite at its own vintage, in both configurations of `make test-matrix`.
Every anchor pair is well formed at each tag: one opening marker, one `end`.
And the pinning does the work it exists to do — `01c40466` resolves to a five-alternative `core_leaf` at `blog/phase-26` and to the eight-alternative one at `blog/phase-27`, from the same UUID in the same file, which is pinning working rather than pinning failing.

## 4. Choosing the anchors

One UUID block is one post concept (`docs/CODING_RULES.md`), and a block should be short, executable, and free of include guards and duplicate-include boilerplate.
New anchors are real `uuidgen` output.
Existing pairs are never deleted or nested; there are none in this tree, so every anchor here is new.

The anchors that landed, by the tag each is first visible at:

| Post | UUID | File | Region |
|---|---|---|---|
| 24 | `274e7d74-a6be-421c-8ecf-d0b4aeba74f6` | `foundation/fold_left_short.hpp` | `fold_left_short` — the fold `std::ranges::fold_left` cannot be |
| 24 | `065a04a3-92d8-4537-bc38-f5c19ba9250b` | `foundation/result.hpp` | `result` |
| 24 | `2c04510b-930c-4d2e-be6c-8733f844baa1` | `foundation/foldable.hpp` | `fold_left`, derived from the `fold_map` centre |
| 24 | `51f97b89-0923-4b8f-94f1-166b79f6d70d` | `foundation/static_vector_instances.hpp` | instances selected by variable template |
| 25 | `f84353ef-c765-455d-addf-27455bba15ea` | `symbol/symbol_id.hpp` | `symbol_id` — identity as an index, carrying no capacity |
| 25 | `e8219367-476a-4b85-882a-87d179d1a271` | `symbol/symbol_table.hpp` | `entry` and the two storage vectors: a name pool and three slots |
| 25 | `19c820e1-8305-4f63-a239-8234a827ca29` | `symbol/symbol_table.hpp` | `intern` |
| 25 | `bc4cbac8-befd-44db-a89b-6b6be78b1714` | `symbol/symbol_table.hpp` | `find`, and why an uninterned entry is invisible to it |
| 26 | `673865b0-7d8f-4e49-a11e-1bc747348df5` | `foundation/tagged_tree.hpp` | `tagged_tree` — children before parents |
| 26 | `8fabe263-2634-4144-abed-f69013131e52` | `foundation/tagged_tree_instances.hpp` | the Traversable instance and its order contract |
| 26 | `23ee18c4-af42-42b1-b97a-3513b932e5ef` | `reader/datum.hpp` | `datum_atom` and `datum_branch` |
| 26 | `b803edcd-959d-4364-94f8-13fb256e7b9b` | `reader/read.hpp` | `read_node` — the readtable lookup as the reader's spine |
| 26 | `01c40466-6b6c-4651-b7b6-98c7a289ef5d` | `core/ast.hpp` | the core's leaves (five at phase 26, eight at phase 27) |
| 26 | `60da8def-db67-496e-9b36-6c544c60bc1e` | `core/ast.hpp` | the core's branch tags (three at phase 26, four at phase 27) |
| 27 | `475cde6b-1633-44fe-96c7-02c60f4c9fd7` | `elaborator/elaborate.hpp` | `atom_lowering` — pass one |
| 27 | `dc3ee031-e752-4420-a3e8-610face05838` | `elaborator/elaborate.hpp` | `plan_nodes` — pass two, `scan_down` |
| 27 | `f1cd911b-1555-4d82-a97f-2044cd35cbf4` | `elaborator/elaborate.hpp` | `emit_quoted_list` — hermetic pairs via `fold_left_short` |
| 27 | `53eabd7c-d0c2-4f0e-a63b-3260a0151044` | `elaborator/elaborate.hpp` | `elaborate` — the three schemes composed |

The two `core/ast.hpp` anchors are the interesting pair: the same UUID names the same declaration at both tags and shows different code, which is pinning working rather than pinning failing.

## 5. Drafting

Five posts, five fresh agents, one each, per D20 — none of them the agent that wrote the code, and none reading another's conversation.

Each drafting agent's reading list is bounded: `git show <step-merge>`, the step's commit message, the step brief the step retired (recoverable with `git show <merge>:step-brief-rN.md`), `docs/blog/` for house format, the preceding post for continuity, and the `voice` skill.
Where a post needs a decision's reasoning rather than its consequence, the named section of `docs/cl-rebuild-plan.md` §2 is fetched by number, not read wholesale.

The five can run in parallel; each pins to its own tag and touches its own `.org`.
Only `docs/blog/index.org` and `docs/blog/pins.md` are shared, so those are updated once, at the end, rather than by five agents at once.

Each draft is then reviewed by a second clean agent running the `voice` skill, seeing only the draft and the step's diff.

One caution for every drafting agent.
These are posts about work that is already finished, written after the fact — but the genre is still the non-omniscient diary entry, and the temptation of hindsight is stronger here than it will be for R5 onward.
Write what the step's own diff and brief show, including what they show as unresolved; do not narrate R4 knowing how R5 turns out.

## 6. Steps

Numbering continues `docs/epistolary-pinning-plan.md`'s B-series, which owns B0–B4.

- **B5** — anchors and tags. **Done 2026-08-02** (§3): `main` rewritten from R1 forward, 18 anchors, four tags. The force-push is pending; the tags are local until it happens.
- **B6** — phase 23 (R0). No pin, no transclusions; the "why rebuild rather than refactor" essay, drawn from the plan's §1 and §3.
- **B7** — phase 24 (R1), pinned to `blog/phase-24`.
- **B8** — phase 25 (R2), pinned to `blog/phase-25`.
- **B9** — phase 26 (R3), pinned to `blog/phase-26`.
- **B10** — phase 27 (R4), pinned to `blog/phase-27`.
- **B11** — close-out. `docs/blog/index.org` gains five entries; `docs/blog/pins.md` gains five rows and a short third-era note recording that these pins were built by anchor branch rather than chosen from an existing merge, and why. `make blog-md` and `scripts/verify-transclusions.sh` green, and the back catalogue's regenerated `.md` unchanged apart from the known `#orgXXXXXXX` id churn.

B6 through B10 are independent of each other and depend only on B5.

## 7. What this does not do

It does not retrofit anchors into `src/smd/smdlisp/**`, which is the behavioural oracle and is never edited.

It does not attempt to make the backfilled posts look like they were written at the time.
Their `#+DATE:` is the date they are written, and `docs/blog/pins.md` will say plainly that phases 24–27 were pinned retroactively by construction.
Post-date-based pin inference (P4 method 2) remains forbidden for this series, and these five add five more reasons why.
