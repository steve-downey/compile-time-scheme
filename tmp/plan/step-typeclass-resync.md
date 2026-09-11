# Step typeclass-resync — carry `main`'s typeclass rename onto the Phase B branch

## What this step is, and what it is not

It is **not** one of the plan's fourteen steps. It is branch maintenance,
inserted between B4 and B5 by the orchestrator on 2026-09-10, because `main`
moved under the Phase B branch in a way that makes the branch's central
artefact — `parser<F>`'s Monad registration — name two identifiers that no
longer exist.

Consequences of that, which differ from every other step file you may have
seen:

- **No blog phase is reserved and you land no new anchors.** The plan reserved
  phases 33–46 for A0–B8 and this step is none of them. Existing anchors in
  files you touch stay put; `scripts/verify-transclusions.sh` must still be
  green at your merge.
- **No `DIV-NNNN` is reserved.** If you find something that wants one, say so
  in your handoff addendum and do not allocate a number.
- You do tick a root-`checklist.md` line, but you **add** it first; see § 5.

Everything else in `tmp/plan/AGENT-PROMPT.md` applies unchanged: the reading
contract, the verify commands, the incidental-fix rules, and the halt rules.
Read `AGENT-PROMPT.md` and Tier 1 before you start.

## Project context

`compile-time-scheme` is a compile-time compiler proof of concept in C++26 on
GCC16. One front end, `src/smd/cl/`, over one substrate, `src/smd/kit/`.

Phase B is converting `src/smd/cl/`'s reader onto a parser-combinator layer,
`smd::kit::parser`, on the integration branch `cl-parser-combinators`. B1 split
`read.hpp`; B2 created the parser and registered it as a Monad instance; B3 and
B4 added skipping, choice and bounded repetition.

**Integration branch: `cl-parser-combinators`.**

## Why

`cl-parser-combinators` branched off `main` at `6099a84`. `main` is now eight
commits further on, and those eight commits are a pickup of `beman.transpose`'s
typeclass-instance work into `src/smd/kit/foundation/`. Two of the changes are
directly load-bearing for this branch:

- **The lookup variables took the plain names.** `monad_typeclass<T>` is now
  `monad<T>`, and the same for `functor`, `applicative`, `alternative`,
  `traversable` and `foldable`.
- **The CRTP bases took the longer names.** What was `foundation::monad<Impl>`
  is now `foundation::derive_monad<Impl>`, and so on for the other five.

`src/smd/kit/parser/parser_instances.hpp` — B2's deliverable — names both old
spellings. The textual merge of `main` into this branch is **clean**; verify
that for yourself rather than taking it on trust, but expect no conflict
markers. What breaks is the compile, and only after the merge.

Doing this now rather than at B8 is deliberate. B5, B6 and B7 all extend the
combinator layer. Every step that lands against the dead spelling widens a
rename that somebody pays for later, and — more to the point — B5 onward would
be verifying against a `src/smd/kit/foundation/` that is no longer the one on
`main`, including two Tier-1 documents (`docs/cpp-rules.md`,
`docs/CODING_RULES.md`) that gained typeclass rules this branch cannot see.

## Setup

```sh
cd /home/sdowney/src/compile-time-scheme/main
git worktree add ../step-typeclass-resync -b step-typeclass-resync cl-parser-combinators
cd ../step-typeclass-resync
git submodule update --init --recursive
```

## Verify GREEN baseline

Before the merge, on the branch as B4 left it:

```sh
make test-matrix > /tmp/verify-resync-base.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-resync-base.log
```

Two `100% tests passed` lines is GREEN. Not green ⇒ `blocked-typeclass-resync.md`.
Record the ctest count per leg; you will compare it after.

## The change

### 1. Merge `main`

```sh
git merge --no-ff main
```

Expect no conflicts. If you get any, resolve them and say exactly what and why
in the commit message — an unexpected conflict is information, not noise.

### 2. Make it compile

The known breakage is confined to `src/smd/kit/parser/parser_instances.hpp`:

- `struct parser_monad_map : foundation::monad<parser_monad_impl>` — the base
  is now `foundation::derive_monad`.
- `template <class F> inline constexpr auto monad_typeclass<...> = ...` — the
  lookup variable is now `monad`.
- Two prose comments in that file and one in `src/smd/kit/parser/parser.hpp`
  name `monad_typeclass` / `functor_typeclass`. Update them; a comment that
  names an identifier which no longer exists is worse than no comment.

Grep for the rest rather than trusting that list:

```sh
grep -rn 'monad_typeclass\|functor_typeclass\|applicative_typeclass\|alternative_typeclass\|traversable_typeclass\|foldable_typeclass' src/ docs/
grep -rn 'foundation::monad<\|foundation::functor<\|foundation::applicative<' src/smd/kit/parser/ src/smd/cl/reader/
```

Be careful with the second grep: after the rename, `foundation::monad<T>` is a
**correct** spelling of the lookup variable and a **wrong** spelling of the
base. Read each hit; do not sed this one.

### 3. Answer the question `derive_monad`'s documentation puts to you

This is the part of the step that is not mechanical, and it is why this is a
dispatched step rather than an orchestrator one-liner.

`src/smd/kit/foundation/monad.hpp`'s comment on `derive_monad` now says:

> The derivations assume `bind` invokes its function before returning, which
> holds for every strict instance … An instance that defers its continuation
> instead of running it — a parser that stores what to do next — has to supply
> these operations itself rather than inherit them.

That sentence names `parser<F>` in all but the word. `parser_monad_map`
inherits `derive_monad`, and therefore inherits `fmap`, `join`, `then`,
`apply`, `kleisli` and `as_functor`, all derived under an assumption the parser
does not satisfy.

Establish the facts before you decide:

- Which of the derived operations does anything on this branch actually call?
  Grep `src/smd/kit/parser/` and `src/smd/cl/reader/` for each name. If the
  answer is "none — only `bind` and `pure`", say so with the evidence.
- Do the derived members even instantiate for `parser<F>`? They are templates;
  an unused one costs nothing and is never checked. Establish whether that is
  the situation rather than assuming it.
- Does `parser<F>` satisfy the new `monad_impl` / `monad_object` concepts? They
  need `element_type_t<Context>`, and `parser<F>` has no `value_type`. If it
  does not satisfy them, that is very likely correct and not a defect — but it
  must be **recorded** as a deliberate position, not left as a silent fact
  somebody rediscovers at B8.

Then take the smallest action consistent with what you found, in this order of
preference:

1. **Keep inheriting `derive_monad` and document the position** — in the
   comment block in `parser_instances.hpp`, in the terms the new
   `monad.hpp` comment uses: which operations are live, which are inherited but
   never instantiated, and why that is safe here rather than merely untested.
2. **Stop inheriting and supply `bind`/`pure` directly**, if you find a derived
   operation that is actually reachable and actually wrong. This is a larger
   change and you must show the reachable call.

If you find a third situation — a derived operation that is reachable and whose
correctness you cannot settle — that is an `amendment-typeclass-resync.md`, not
a guess. Write it and stop.

**Do not** change anything under `src/smd/kit/foundation/` or
`src/smd/cl/foundation/`. Those files arrived from `main` and this branch does
not own them. If one of them needs a fix, that is an "Ask" under
`AGENT-PROMPT.md` § "Incidental fixes": halt and say so.

### 4. Update the architecture doc

`docs/compiler_architecture.org` § `smd::kit::parser` was written by B2–B4 and
names the old spellings if it names them at all. Open **that named section
only**. Correct the spellings, and add a short paragraph recording the § 3
position — this is the section B8 will build its architecture write-up on, and
a position stated only in a commit message is one B8 will not find.

### 5. The root `checklist.md`

Unlike every other step, your line does not exist yet. Add it immediately after
the `Step B4` line in the Phase B section, and tick it:

```markdown
- [x] Step typeclass-resync: carry `main`'s typeclass rename onto the Phase B branch (out-of-band maintenance, not one of the fourteen)
```

Nothing else in that file.

## Verify GREEN

```sh
make test-matrix > /tmp/verify-resync.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-resync.log
make compile-headers
make lint
./scripts/verify-transclusions.sh
```

The ctest count **will** rise relative to your baseline — `main` added tests to
`src/smd/kit/foundation/` and `src/smd/cl/foundation/`. That is the merge
arriving, not this step's doing. Record both numbers and the difference, and
confirm the increase is accounted for by foundation tests alone: no
`src/smd/cl/reader/` or conformance test may appear, disappear, or change.

```sh
git diff --stat cl-parser-combinators..HEAD -- 'src/smd/cl/reader/' 'src/smd/cl/conformance/'
```

`docs/compiler_architecture.org` aside, that diff should be empty. If it is
not, you have done more than this step asks.

`make lint` must be green on a **first** run against a clean tree — see
`docs/backlog/BL-0008-*.md`. If its first run rewrites files, commit the
rewrite and note it in `out_of_scope`.

## Spot checks

```sh
# the old spellings are gone from the whole tree
grep -rn 'monad_typeclass\|functor_typeclass' src/ docs/ ; echo "expect no hits"

# the registration is present under its new name
grep -rn 'inline constexpr auto monad<' src/smd/kit/parser/

# the merge really did bring main in
git merge-base --is-ancestor main HEAD && echo "main is an ancestor"
```

## Commit and merge

Two commits, not one: the merge commit, then the fix. Draft messages — these
are drafts, not final text; the orchestrator has them reviewed.

```sh
git commit -m "$(cat <<'EOF'
kit: the parser's Monad registration, under the names main now uses

main's typeclass pickup gave the lookup variables the plain names and
the CRTP bases the longer ones, so monad_typeclass<parser<F>> and
foundation::monad<Impl> are both spellings of things that no longer
exist. This is the same registration under the current names.

<one paragraph recording the § 3 position: which derived operations are
live, which are inherited and never instantiated, and why that is safe
here.>
EOF
)"
```

Then merge to the integration branch **from inside your own worktree**. The
main checkout at `/home/sdowney/src/compile-time-scheme/main` stays on `main`
and is never switched off it; `cl-parser-combinators` is checked out nowhere, so
your worktree may take it:

```sh
git checkout cl-parser-combinators
git merge --no-ff step-typeclass-resync -m "Merge step typeclass-resync: main's typeclass rename onto the Phase B branch"
```

Leave the branch there. Do not push to `origin`; the orchestrator pushes.

## Metrics

```sh
cat >> /home/sdowney/src/compile-time-scheme/main/tmp/plan/metrics.jsonl <<EOF
{"step":"typeclass-resync","lane":null,"outcome":"green","wall_seconds":N,"attempts":1,"verify":{"command":"make test-matrix + compile-headers","exit_code":0,"wall_seconds":N,"log_bytes":N,"summary_lines_read":6},"diff":{"files_changed":N,"insertions":N,"deletions":N},"out_of_scope":[],"note":"out-of-band maintenance between B4 and B5, not one of the fourteen. ctest count before/after and what accounts for the difference. The section-3 position in one sentence."}
EOF
```

## Tracker and handoff

Tick your line in `/home/sdowney/src/compile-time-scheme/main/tmp/plan/checklist.md`
(the fan-out tracker, in the main checkout, by absolute path — never inside
your worktree).

Then **append** to the existing
`/home/sdowney/src/compile-time-scheme/main/tmp/plan/handoff-B5.md` a dated
section headed `## Addendum, typeclass-resync, 2026-09-10`. Do not rewrite what
B4 wrote; everything in it about `operator|`, `many_until` and the
zero-consumption guard is still true. Your addendum says:

- The branch now carries `main` up to `d6ae364`. The exact spellings B5 will
  see for the Monad registration, and the fact that `bind` and `pure` at the
  call sites are unchanged.
- The § 3 position, in two or three sentences.
- That `docs/cpp-rules.md` and `docs/CODING_RULES.md` gained typeclass rules in
  this merge, so B5's Tier-1 read is not the one B4 did.
- The new ctest baseline per leg, so B5 does not read the rise as its own.
- That the WIP commit on `step-b5-text` (`78cc244`) predates this merge. B5
  will have to merge or rebase it forward; the orchestrator's dispatch says so
  too, but say it here as well.
