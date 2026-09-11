# Handoff to A2

A1 merged into `cl-retire-trees` (`--no-ff`, merge commit `b7a5742`). Two
annotated tags exist and are confirmed resolving:

- `iteration/smdscheme-final`
- `iteration/smdlisp-final`

Both point at an ancestor of `cl-retire-trees` (A1's pre-merge commit
`549a1fc`), which is all `git show <tag>:<path>` needs — do not expect them to
point at the merge commit itself, and do not move them.

## `src/examples/godbolt_lisp.cpp` and `godbolt_lisp_ffi.cpp`: no living link left

All three transclusions of these two files (`05b6b13a…`, `89fd04b3…`,
`c19f8ea7…`) now live only in `docs/history/architecture-iterations.org`,
pinned via `orgit-file:` against `iteration/smdlisp-final`. `git show
iteration/smdlisp-final:src/examples/godbolt_lisp.cpp` reproduces them
regardless of what happens to the worktree copy. `docs/compiler_architecture.org`
no longer mentions either file at all. So: deleting both files outright, as
your step file's part 1 does, breaks no living link and needs no companion
edit to the living document. `verify-transclusions.sh`'s "living docs" pass
doesn't touch them either way, since it was never checking `src/examples/` — it
walks links **out of** the living doc, not references **into** a given path.

## The `orgit-file:` spelling used, if you need to pin anything else

```
[[orgit-file:~/src/compile-time-scheme/main::iteration/smdlisp-final::src/smd/smdlisp/closure/env.hpp::1232b7f6-c9b9-4611-a958-bd1bb14b0ff5]]
```

Repo field `~/src/compile-time-scheme/main` is copied verbatim from the
working example in `docs/blog/phase-32-extracting-the-kit.org` — the verifier
never reads that field (it only uses `rev`, `path`, `uuid` from the `::`-split
link; `rev` is the tag, `path` is **repo-relative**, no leading `../`). You
should not need a new pin for anything in scope for A2 — part 5 of your step
file says so — but if a spot check surprises you, this is the shape to copy.

## `verify-transclusions.sh`: `posts_globs` is now an array, `$1` override intact

`posts_glob` (singular, string) became `posts_globs` (array), holding
`('docs/blog/phase-*.org' 'docs/history/architecture-iterations.org')` by
default, iterated the same unquoted-array way `md_globs` already was
(`# shellcheck disable=SC2048`). The positional-override contract for `$1`
still works unchanged: `./scripts/verify-transclusions.sh SOME_GLOB` still
overrides just the posts side. Nothing in your declared scope touches this
script; noted only in case a future spot check greps its source and expects
the old singular-string name.

## Baseline to expect

`make test-matrix`: 1123/1123 both legs, unchanged by A1 (docs-only). `make
lint` and `verify-transclusions.sh` both green — 142 resolved, 1 documented
WARN (the `phase-12-cps.org` triple-UUID, unchanged).

## One inaccuracy found in A1's own step file, for the pattern only

Not something you need to act on, but worth knowing this plan's spot-check
arithmetic has had more than one error caught in review (the "five vs six"
`cl` transclusion count, caught before A1 started): step-A1.md's spot check
`grep -c 'cl-limitations.md' docs/compiler_architecture.org
docs/history/architecture-iterations.org # expect 2 across both` is wrong.
`grep -c` counts *lines*, and six lines carry a bare prose mention of
`docs/cl-limitations.md` (not a link) alongside the two real `[[file:…]]`
links that moved — true count is 8, all landed in the history doc, 0 in the
living doc, 0 dropped. Recorded in A1's `metrics.jsonl` entry. Nothing for you
to do about it; flagging only so a similar off-by-arithmetic in your own spot
checks doesn't get mistaken for something you broke.
