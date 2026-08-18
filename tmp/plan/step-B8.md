# Step B8 — close the series: measure the cost, and record what dissolved

## Project context

`compile-time-scheme` is a compile-time compiler proof of concept in C++26 on
GCC16. One front end, `src/smd/cl/`, over one substrate, `src/smd/kit/`. B1
split the reader; B2–B7 built `smd::kit::parser` and converted every reader
function onto it, one primitive at a time, each landing with its first real
consumer.

**Integration branch: `cl-parser-combinators`.** This is the last
implementation step; the integration review follows and is not a code step.

**Reserved for this step:** blog phase **45**, divergence number **DIV-0041**.

## Phase B's standing constraints

They still apply here, and this step is where they are checked as a whole
rather than one function at a time.

## Why

Three things are owed and none of them is code.

**Decision D30 says the compile-time cost is measured and recorded, not gated.**
The reasoning, recorded on the owner's direction: the restrictions of
`constexpr` are valuable here as a correctness discipline and as a
demonstration, not as a product feature — "having a full CL at compile time is
almost beside the point." So a large constant-factor increase in compile time
is an acceptable price for structure that is correct by construction, and cost
does not veto the change. But evidence-before-claim is house style, and "we
measured it, it cost this much, we accepted it" is a far better record than
silence. Only a qualitative failure counts against: an ops limit that cannot be
raised, or a test matrix that becomes unusable in practice.

**DIV-0028 dissolved rather than closing.** It recorded that "the parser
combinator layer has no `cl` client", with the revisit condition "either
`src/smd/cl/reader/cursor.hpp` and its callers are rewritten onto a
`parser<T>`-shaped combinator layer, making `cl` a genuine **third** client, or
a fourth front end needs the same abstraction." The condition fired — and
firing it showed the framing was wrong. There is no third client and no fourth
front end. Phase A deleted the other two, so the layer has exactly **one**
client and that is what makes it worth having: R8's lesson was that extracting
for its own sake produces a module with no callers, and the answer turned out
not to be more clients but one real one.

**The reader's own account is out of date.** `docs/compiler_architecture.org`
has been updated in place at an anchor by seven steps in a row, which is right
for durable facts and wrong for the shape of the thing. It needs one section
that says what the reader is now.

## Setup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree add ../step-b8-close -b step-b8-close cl-parser-combinators
cd ../step-b8-close
git submodule update --init --recursive
```

## Verify GREEN baseline

```sh
make test-matrix > /tmp/verify-B8-base.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-B8-base.log
```

Not green ⇒ `blocked-B8.md`.

## The change

### 1. The D30 measurement

You need a before and an after. The **before** is the merge base of
`cl-parser-combinators`, which is `main` as Phase A left it — nothing in B1–B7
was asked to record it, deliberately, because taking it here from git is more
reliable than asking seven agents to remember.

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
base=$(git merge-base main cl-parser-combinators)
git worktree add /tmp/b8-before "$base" --detach
cd /tmp/b8-before && git submodule update --init --recursive
```

Measure the same three things in both trees, both matrix legs:

- **Whole-matrix wall time.** `date +%s` around `make test-matrix` from a cold
  build directory in each. Cold, not incremental — the incremental number
  depends on what you touched last and is not comparable.
- **The reader translation unit's own wall time.** `cl_reader_test`'s objects
  are where the change lands. Compile one of them alone and time it; take the
  exact command from `compile_commands.json` so the flags match, and run each
  three times, reporting the minimum rather than the mean — a single warm run
  is noise.
- **`-fconstexpr-ops-limit` headroom.** The project sets no explicit limit
  today, so GCC's default applies. Find the smallest power-of-two-ish limit at
  which `cl_reader_test` still compiles, in each tree, by passing
  `-fconstexpr-ops-limit=N` to that one compile and bisecting. The absolute
  numbers matter less than the ratio, and the qualitative question D30 actually
  asks is whether any limit exists that works — a limit that cannot be raised is
  the one outcome that counts against the change.

Write the numbers into a new subsection of `docs/compiler_architecture.org`,
with the toolchain version, the machine, and the commit each was taken at. Do
not put them in `metrics.jsonl`; that file measures the fan-out, not the
compiler, and mixing the two would corrupt both.

If the after-numbers are dramatically worse, that is a **finding to record, not
a failure to fix**. D30 is explicit that cost does not veto. Say plainly what it
cost.

Remove the scratch worktree when done: `git worktree remove /tmp/b8-before`.

### 2. DIV-0028

Append a dated note; the divergence docs are append-only and are never edited
in place. Say that the revisit condition fired, that satisfying it revealed the
condition was framed around a repository with three front ends, and that the
outcome is one client rather than three. Say that this is a dissolution rather
than a closure: the divergence did not stop being true by being fixed, it
stopped being a meaningful statement about the repository.

Update `docs/divergences/README.md`'s row for DIV-0028 to match. Do **not**
change its classification unless the README's taxonomy has a category that
genuinely fits better — read the taxonomy before deciding, and say in your
handoff which you chose and why.

### 3. The architecture section

Write one section of `docs/compiler_architecture.org` describing the reader as
it now is: the kit's parser layer and its final primitive set; the threaded
context and the two context types actually in use (`reader_context` and
`readtable const`); the file layout B1 created; and the two places where the
combinator deliberately does the *worse* thing because D31 required it — the
block comment that consumes to end of input, and the repetition that diagnoses
at capacity rather than truncating.

Resolve every "provisional" note B2–B7 left. A decision that was provisional
because its first consumer had not arrived is no longer provisional once seven
have. Where one is still genuinely open, say what would settle it.

Check the anchors while you are here: `scripts/verify-transclusions.sh` is part
of the verify, and after eight steps of anchor churn this is the last chance to
notice a section that transcludes something a later step moved.

### 4. The project checklist

Add Phase A's and Phase B's steps to the repository root `checklist.md`,
matching the file's existing format. This is the project's status document, not
the fan-out's tracker; both exist and they are different files.

### 5. What you do not do

Do **not** edit `docs/cl-parser-scoping.md`. It is a proposal the owner
ratifies, its § 4 phase sketch does not match the plan that was actually
executed, and correcting someone's unratified proposal from inside an
implementation step is the wrong direction. Instead, list in your final handoff
exactly where the executed series diverged from § 4 — that list is what the
owner amends the note from, in one pass.

## Declared file scope

```
docs/compiler_architecture.org                (measurement subsection; the
                                               reader's new account; provisional
                                               notes resolved)
docs/divergences/DIV-0028-parser-combinator-layer-has-no-cl-client.md
                                              (dated note appended)
docs/divergences/README.md                    (one row)
checklist.md                                  (project status, root)
```

No source changes. If you find yourself wanting one, it belongs to whichever
step owns that file and it is too late — record it in the handoff.

## Verify GREEN after

```sh
make test-matrix > /tmp/verify-B8-after.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-B8-after.log
wc -c /tmp/verify-B8-after.log
make lint
./scripts/verify-transclusions.sh
```

## Spot checks

```sh
git diff --name-only cl-parser-combinators..HEAD | grep -E '\.(hpp|cpp)$'
```

Must return nothing.

```sh
grep -n 'D30\|constexpr-ops-limit' docs/compiler_architecture.org
grep -c 'provisional' docs/compiler_architecture.org
grep -n '2026-' docs/divergences/DIV-0028-parser-combinator-layer-has-no-cl-client.md | tail -2
```

And the whole-series check, which no single earlier step could make:

```sh
git diff --stat $(git merge-base main cl-parser-combinators)..HEAD -- \
    'src/smd/cl/**/*.test.cpp' 'src/smd/cl/conformance/'
```

**This should be empty or near-empty.** It is D31's acceptance witness stated
as a diff: the whole series changed no reader test and no corpus expectation.
If it is not empty, find out which step changed what and put it in your handoff
in bold — the integration review needs to know, and a changed expectation was
supposed to be a halt.

## Commit and merge back

```sh
git add -A
git commit -F - <<'EOF'
docs: what the reader cost, and what DIV-0028 turned out to be

D30 asked for the compile-time price to be measured and recorded rather
than gated, on the reasoning that constexpr here is a correctness
discipline and a demonstration rather than a product feature. So the
numbers are in the architecture document with the toolchain and the
commits they were taken at, and they are not an argument about anything.
The only outcome that would have counted against was an ops limit that
could not be raised.

DIV-0028 said the parser combinator layer had no cl client, and its
revisit condition was that cl's reader be rewritten onto the layer,
making cl a genuine third client. The condition fired and showed the
framing was wrong. There is no third client. Phase A deleted the other
two, and the layer has exactly one -- which is the point rather than a
shortfall, because R8's lesson was that extracting for its own sake
produces a module with no callers. The divergence did not close by being
fixed; it stopped being a statement about this repository.

The architecture section is also where the two deliberate uglinesses go
on the record: a block comment that still runs to end of input, and a
repetition that diagnoses at capacity instead of truncating. Both are
the worse combinator, chosen because D31 said this series does not get
to improve behaviour on the way past.
EOF

git checkout cl-parser-combinators
git merge --no-ff step-b8-close
```

## Record measurements

```sh
cat >> /home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/metrics.jsonl <<EOF
{"step":"B8","lane":null,"outcome":"green","wall_seconds":<measured>,"attempts":<n>,"verify":{"command":"make test-matrix","exit_code":0,"wall_seconds":<measured>,"log_bytes":$(wc -c < /tmp/verify-B8-after.log),"summary_lines_read":<n>},"diff":{"files_changed":<n>,"insertions":<n>,"deletions":<n>},"out_of_scope":[],"note":"D30 before/after builds are in this step's wall time"}
EOF
```

The D30 measurement means this step runs several cold builds that have nothing
to do with its diff. Say so in `note`; otherwise the integration review reads
this row as a documentation change that inexplicably cost an hour.

## Cleanup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree remove ../step-b8-close
git worktree list          # /tmp/b8-before must be gone too
```

Mark B8 done in
`/home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/checklist.md`.

## Handoff

There is no next implementation step. Write `tmp/plan/handoff-review.md`
(≤ ~150 lines) for the integration review, which runs on a stronger reasoning
tier and reads the whole diff.

Tell it: the D31 whole-series diff result from the spot checks, exactly; the
final primitive set of `smd::kit::parser` and which primitives arrived in which
step; every `amendment-NN.md` and `blocked-NN.md` written during the run, if
any; and the list from part 5 — where the executed series diverged from
`docs/cl-parser-scoping.md` § 4, which the owner will amend the note from.
