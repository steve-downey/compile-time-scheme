# Step B1 — split `read.hpp` into component headers plus an umbrella

## Project context

`compile-time-scheme` is a compile-time compiler proof of concept in C++26 on
GCC16. After Phase A, `src/smd/` holds exactly three packages: `kit` (the
shared constexpr substrate), `cl` (the live Common Lisp front end), and
`fixpoint`. The two earlier iterations are frozen at `iteration/smdscheme-final`
and `iteration/smdlisp-final` and no longer exist in the worktree.

**Integration branch: `cl-parser-combinators`.**

**If that branch does not exist, stop and write `blocked-B1.md`.** Phase A ends
at a deliberate human gate: `retire-three-trees` merges to `main`, the owner
reviews the deletion of two trees and the retirement of the `smdlisp`
never-edited rule, and only then is Phase B's branch created off `main`. An
absent branch is the gate, not a defect.

**Reserved for this step:** blog phase **38**, divergence number **DIV-0034**.

## Phase B's standing constraints — these apply to every B step

From decision D31 (`docs/cl-parser-scoping.md` § 3), and they are not
negotiable:

- **No observable behaviour change.** No new syntax, no changed diagnostic
  text, no different error position.
- **DIV-0003 must not regress.** Whole-token classification: `1+` reads as the
  symbol `1+`, not the fixnum 1 followed by a stray `+`. See
  `docs/divergences/DIV-0003-atom-maximal-munch.md`.
- **`scan_token` and `classify_number` are out of scope for all of Phase B.**
  Not "handle with care" — out of scope. Classifying a whole token *is* the
  mechanism by which DIV-0003 holds, so leaving them untouched makes the
  divergence structurally unbreakable rather than merely tested against. A
  parser may be built that *calls* them; neither may be rewritten.
- **Existing reader tests and the conformance corpus pass unchanged.** Adding a
  new case is fine. **Changing an existing expectation is a halt**, always.

## Why this step

`src/smd/cl/reader/read.hpp` is 583 lines holding eleven `Ctx`-templated
functions, two public entry points, a character-name table, and two
`using`-declarations. Every later Phase B step edits some part of it, which
means every later step's declared file scope would be the same single file and
every diff would be against a moving target.

Splitting first is boring and load-bearing. It gives each later step a small,
precise scope; it makes each step's incremental rebuild narrower; and it makes
the diffs legible, which matters because the claim Phase B is making —
"nothing observable changed" — is checked by reading diffs.

A mechanical split that quietly changes behaviour is the worst outcome
available, because every later step inherits it. This step's whole discipline
is that the test matrix must pass with **zero test-file edits**.

## Setup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree add ../step-b1-split-read -b step-b1-split-read cl-parser-combinators
cd ../step-b1-split-read
git submodule update --init --recursive
```

## Verify GREEN baseline

Your inbound handoff carries the exact ctest count and cold matrix wall time
after Phase A's deletion; both are much lower than the 1118 and ~7 minutes the
plan was written against.

```sh
make test-matrix > /tmp/verify-B1-base.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-B1-base.log
wc -c /tmp/verify-B1-base.log
```

Two `100% tests passed` lines. Not green ⇒ `blocked-B1.md`.

## What already exists that this step builds on

`src/smd/cl/reader/read.hpp`, in source order:

1. `default_max_nodes`, `default_max_list` — namespace-scope constants.
2. `namespace detail {` opens.
3. `using foundation::and_then;`
4. `read_context<SymbolTable, MaxNodes, MaxList>` — `tree`, `symbols`, `table`,
   and the `child_list` member type alias.
5. `skip_block_comment(cursor) -> cursor`
6. `skip_intertoken_space(cursor, readtable const&) -> cursor`
7. `add_leaf_checked<Ctx>`, `add_branch_checked<Ctx>`
8. `using symbol::intern_checked;` — with a comment explaining that the two
   definitions could not coexist because ADL found both. Move the comment with
   the declaration; it is load-bearing.
9. `read_node<Ctx>` — forward declaration only.
10. `read_wrapped<Ctx>`, `read_delimited<Ctx>`
11. `read_string<Ctx>`
12. `named_char`, `named_chars`, `lookup_char_name` — note these are at
    `detail` scope but are not `Ctx`-templated.
13. `read_character<Ctx>`
14. `read_radix_number<Ctx>`, `read_sharpsign<Ctx>`
15. `read_token_datum<Ctx>`
16. `read_node<Ctx>` — the definition, inside UUID anchor
    `b803edcd-959d-4364-94f8-13fb256e7b9b`.
17. `}` closes `detail`; `read_datum<>` and `read<>` follow at
    `smd::cl::reader` scope.

## The change

### 1. Eight headers under `src/smd/cl/reader/detail/`

Everything being moved is already in `namespace smd::cl::reader::detail`, and
`docs/cpp-rules.md` says non-public helpers live in `detail/` below the
component directory. So the new headers go in `src/smd/cl/reader/detail/`, where
the directory and the namespace agree, and — by the same rule — they do **not**
each need a bootstrap test file: "a `detail/` header big enough to need its own
tests is a component — promote it", and none of these is.

| new header | contents from the list above |
|---|---|
| `detail/read_context.hpp` | 1, 3, 4, 7, 8 |
| `detail/skip.hpp` | 5, 6 |
| `detail/read_node_fwd.hpp` | 9 |
| `detail/forms.hpp` | 10 |
| `detail/text.hpp` | 11, 12, 13 |
| `detail/sharpsign.hpp` | 14 |
| `detail/token_datum.hpp` | 15 |
| `detail/node.hpp` | 16 |

`read.hpp` keeps 17 — the two public entry points — and becomes an umbrella
that includes all eight in dependency order.

`detail/read_node_fwd.hpp` is what makes the split possible. `read_wrapped`,
`read_delimited` and `read_sharpsign` all call `read_node`, and `read_node`
calls all of them. Everything involved is a template, so a declaration is
enough at the point of the call and the definitions need only be visible at the
point of instantiation, which the umbrella guarantees. Each caller includes the
forward-declaration header; `detail/node.hpp` includes it too and then defines
what it declared.

Item 1 is the odd one out: `default_max_nodes` and `default_max_list` are at
`smd::cl::reader` scope, not `detail` scope, and are used by `read.hpp`'s
default template arguments. Putting them in `detail/read_context.hpp` is fine
as long as they stay in the enclosing namespace — do not move them into
`detail`, that changes their qualified name.

### 2. Every header self-contained

`CMAKE_VERIFY_INTERFACE_HEADER_SETS` is ON and `make compile-headers` builds
each header standalone, so this is checked rather than trusted. Each new header
needs its own canonical prolog, its own `#ifndef SRC_SMD_CL_READER_DETAIL_<NAME>_HPP`
guard, and its own complete include list — canonical angle-bracket spelling,
no transitive reliance. `read.hpp`'s current include block is the union of what
the eight need; do not copy it wholesale into each, work out what each actually
uses.

Add all eight to `cl.reader`'s `FILE_SET cl_reader_headers` in
`src/smd/cl/reader/CMakeLists.txt`. The `FILES` list is relative to the
directory, so the entries are `detail/read_context.hpp` and so on.

### 3. Zero behavioural diff, and how to be sure

Move code; do not improve it. Not a renamed variable, not a reordered branch,
not a tightened `const`, not a reflowed comment beyond what `clang-format`
does. If you notice something worth fixing, put it in your handoff for the step
that owns that function, and leave it.

**No test file may change.** `read.test.cpp`, `cursor.test.cpp`,
`token.test.cpp`, `number.test.cpp`, `readtable.test.cpp`, `datum.test.cpp` and
everything under `conformance/` stay byte-identical. That is the step's
acceptance criterion, and `git diff --name-only` proves it.

### 4. Anchors

The one existing anchor, `b803edcd-959d-4364-94f8-13fb256e7b9b` around
`read_node`, travels intact into `detail/node.hpp`. Per decision D21 you may
add anchors freely; add one around `detail/read_node_fwd.hpp`'s declaration and
one around the umbrella's include list, because those two are what the post
about this step is actually about. Real `uuidgen` output.

Record in `docs/compiler_architecture.org` § "The Common Lisp Rebuild:
=smd::cl=", in place and at an anchor, the reader's new file layout and the
forward-declaration header's role — later steps reference it rather than
rediscovering the dependency shape.

## Declared file scope

```
src/smd/cl/reader/read.hpp                        (reduced to an umbrella)
src/smd/cl/reader/detail/read_context.hpp         (new)
src/smd/cl/reader/detail/skip.hpp                 (new)
src/smd/cl/reader/detail/read_node_fwd.hpp        (new)
src/smd/cl/reader/detail/forms.hpp                (new)
src/smd/cl/reader/detail/text.hpp                 (new)
src/smd/cl/reader/detail/sharpsign.hpp            (new)
src/smd/cl/reader/detail/token_datum.hpp          (new)
src/smd/cl/reader/detail/node.hpp                 (new)
src/smd/cl/reader/CMakeLists.txt                  (FILE_SET entries)
docs/compiler_architecture.org                    (in-place, at an anchor)
```

**No `.test.cpp` file is in scope.** If you find yourself needing to edit one,
the split has changed behaviour and you should stop rather than adjust the test.

## Verify GREEN after

```sh
make test-matrix > /tmp/verify-B1-after.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-B1-after.log
wc -c /tmp/verify-B1-after.log
make compile-headers
make lint
./scripts/verify-transclusions.sh
```

The ctest count must be **exactly** the baseline. Not one more, not one fewer.
`make compile-headers` must pass — it is the check that each new header is
genuinely self-contained rather than accidentally working because the umbrella
includes them in a lucky order.

## Spot checks

```sh
git diff --name-only cl-parser-combinators..HEAD | grep '\.test\.cpp$'
```

Must return **nothing**. That is the zero-behavioural-diff claim, made
mechanical.

```sh
wc -l src/smd/cl/reader/read.hpp                       # ~60, not ~583
ls src/smd/cl/reader/detail/
grep -c 'b803edcd-959d-4364-94f8-13fb256e7b9b' src/smd/cl/reader/detail/node.hpp   # 2
grep -rn 'scan_token\|classify_number' src/smd/cl/reader/detail/ | wc -l
```

The last is a call count, and it must match the call count in the pre-split
`read.hpp` — two calls to `scan_token`, two to `classify_number`. Neither
function is edited by any Phase B step.

## Commit and merge back

```sh
git add -A
git commit -F - <<'EOF'
cl: split read.hpp so later steps have somewhere to stand

read.hpp was 583 lines holding eleven Ctx-templated functions and two
public entry points. Every step of the combinator conversion edits some
part of it, so without a split every one of those steps would declare
the same single file as its scope and every diff would be against a
moving target.

The eight new headers go under reader/detail/ because that is where the
code already was -- all of it is in namespace detail -- and the rule
about detail/ headers is that one big enough to need its own tests is a
component to be promoted, which none of these is.

read_node_fwd.hpp is the piece that makes it work. read_wrapped,
read_delimited and read_sharpsign all call read_node and read_node
calls all of them; everything is a template, so a declaration suffices
at the call and the umbrella supplies the definitions at the point of
instantiation.

Nothing else changed. No test file is touched, the ctest count is
identical, and compile-headers proves each new header stands alone
rather than working by luck of include order.
EOF

git checkout cl-parser-combinators
git merge --no-ff step-b1-split-read
```

## Record measurements

After the merge, before cleanup.

```sh
cat >> /home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/metrics.jsonl <<EOF
{"step":"B1","lane":null,"outcome":"green","wall_seconds":<measured>,"attempts":<n>,"verify":{"command":"make test-matrix + compile-headers","exit_code":0,"wall_seconds":<measured>,"log_bytes":$(wc -c < /tmp/verify-B1-after.log),"summary_lines_read":<n>},"diff":{"files_changed":<n>,"insertions":<n>,"deletions":<n>},"out_of_scope":[],"note":""}
EOF
```

This is the plan's measured floor: a pure code motion with no logic in it. What
it costs is what any Phase B step costs before it does any thinking, and the
integration review compares every other row against it.

## Cleanup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree remove ../step-b1-split-read
```

Mark B1 done in
`/home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/checklist.md`.

## Handoff

Read `tmp/plan/step-B2.md`, then write `tmp/plan/handoff-B2.md` (≤ ~150 lines).
B2 creates `smd::kit::parser` and converts `read_radix_number` onto it. Tell it:
which header holds `read_radix_number` (`detail/sharpsign.hpp`) and exactly what
else is in there; the anchors you landed; anything you found while splitting
that suggests a function is not where the table above put it; and any include
that turned out to be needed in more headers than expected, since that is a
hint about coupling B2 will hit.
