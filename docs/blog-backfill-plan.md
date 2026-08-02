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

## 3. The construction: an anchor branch per step

Anchors cannot be added to history that is already written, and `main` is pushed, so rewriting is out.
Instead, branch from each step's merge, add that step's anchors *there*, tag it, and merge the branch forward.
The tag then names a commit carrying that step's code and its anchors and nothing later; `main` ends up with every anchor.

Nothing is published yet, so choosing a pin now is the normal case, not a repoint: `docs/blog/pins.md` fixes a pin only once the prose is out.

| Post | Step | Branch from | Branch | Tag |
|---|---|---|---|---|
| 23 | R0 | — | — | none (no transclusions) |
| 24 | R1 | `35d5bd6` | `anchors-r1` | `blog/phase-24` |
| 25 | R2 | `352a6ac` | `anchors-r2` | `blog/phase-25` |
| 26 | R3 | `441a64c` | `anchors-r3` | `blog/phase-26` |
| 27 | R4 | `34a835a` | `anchors-r4` | `blog/phase-27` |

R0 is decisions and documents; it transcludes no code and therefore takes no pin, which is the established treatment for ten of the first twenty-one posts.

Per branch: add anchors, confirm `make compile && make test` still pass at that vintage (anchors are comments, so a failure means something else is wrong), commit, tag the commit, merge into `main` with `--no-ff`, push the tag.

Merge `anchors-r1` first and work forward, so each merge resolves against a `main` that already has its predecessors.
Expect one real conflict: `anchors-r3` adds anchors around `core_leaf` and `core_tag` in `core/ast.hpp`, and R4 added alternatives to both variants.
Resolve it by keeping `main`'s code and the branch's anchors — never by moving an anchor pair onto different code, which would make the tag and `main` disagree about what a UUID names.

## 4. Choosing the anchors

One UUID block is one post concept (`docs/CODING_RULES.md`), and a block should be short, executable, and free of include guards and duplicate-include boilerplate.
New anchors are real `uuidgen` output.
Existing pairs are never deleted or nested; there are none in this tree, so every anchor here is new.

Candidate regions, as the phase subjects in `docs/cl-rebuild-plan.md` §6 imply them — the drafting agent is not bound to use all of them, and phase 21 used seven of ten:

- **R1**: `fold_left_short` (the fold `std::ranges::fold_left` cannot be); one typeclass instance map showing variable-template selection; `result`.
- **R2**: `symbol_id`; `symbol_table`'s three slots; `intern` and the uninterned-entry path.
- **R3**: `readtable`'s dispatch; `tagged_tree`'s children-before-parent contract; `tagged_tree_instances`' traversal-order contract; `datum_atom` and `datum_branch`; `core_leaf`, `core_tag`, `core_tree`.
- **R4**: the three-pass comment block at the head of `elaborate.hpp`; `atom_lowering`; `plan_children`; `emit_quoted_list`; `elaborate` itself.

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

- **B5** — anchors and tags. Four anchor branches per §3, `make compile`/`make test` green at each vintage, four `--no-ff` merges into `main`, four tags pushed. Nothing is drafted until this lands.
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
