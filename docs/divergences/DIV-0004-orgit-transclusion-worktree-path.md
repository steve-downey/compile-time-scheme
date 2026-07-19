# DIV-0004: new blog transclusion anchors point at the worktree, not `main`, until merge

- **Status:** open
- **Date:** 2026-07-18
- **Step:** L6 (docs/cl-pivot-plan.md)
- **Authority diverged from:** docs/cl-pivot-plan.md (established convention: every prior phase's `#+transclude:` orgit link uses
  `orgit:~/src/compile-time-scheme/main::...`)

## What diverged

`docs/blog/phase-16-reading-common-lisp.org`'s six `#+transclude:` links point at
`orgit:~/src/compile-time-scheme/wt-l6::...` (this step's own worktree), not at
`orgit:~/src/compile-time-scheme/main::...` like every existing phase's transclusion link.

## Why

`.emacs.d/init.el`'s `org-transclusion-add-orgit` resolves an `orgit:` link's repo-dir component as a literal
filesystem path (`expand-file-name inner-file repo-dir`), not as a git-blob lookup at a ref. It reads whatever
file is actually on disk at that path today.

The Common Lisp pivot runs each step in its own git worktree (`docs/codestyle.org`'s Agentic Instructions;
`AGENTS.md`'s worktree policy) and merges into the `main` worktree only after the step lands. Step L6 added six
brand-new UUID anchors — three in files that do not exist in `main` at all yet (`datum_type.hpp`,
`read_datum.hpp`) and three more inside files that do exist in `main` (`atom.hpp`, `cl_chars.hpp`) but not with
the new anchor comments this step added. Pointing the new transclusion links at
`~/src/compile-time-scheme/main` would make `make blog-md` fail inside this worktree, because `main` does not
yet contain the anchored text — `make blog-md` is a required pre-handoff check (`docs/cl-pivot-plan.md` section
2, section 9's "Verify" gloss), so it must actually pass in the worktree where the step is done.

Pointing the links at `~/src/compile-time-scheme/wt-l6` instead lets `org-transclusion-add-orgit` find the real,
current file content, and `make blog-md` renders phase 16 correctly (confirmed: all six `#+transclude:` lines
resolve and the six code blocks appear correctly in `docs/blog/phase-16-reading-common-lisp.md`).

## Consequences

- The committed `docs/blog/phase-16-reading-common-lisp.md` (GFM output) is correct and stable regardless of
  this issue — the `.md` is generated once, checked in, and does not re-resolve the orgit link at read time.
- The **source** `.org` file's transclusion links only resolve correctly today from inside a checkout at
  `~/src/compile-time-scheme/wt-l6`. Once this branch merges into `main` (per the orchestrator's `--no-ff`
  merge step) the same anchored text will exist in `main` too, so re-pointing the six links from `wt-l6` to
  `main` at that point is a trivial, purely mechanical `sed` fix with no content change — but until that
  happens, re-running `make blog-md` on `main` (or in a fresh worktree that isn't `wt-l6`) will fail to
  transclude phase 16's six blocks, because `wt-l6` may no longer exist as a checkout by then (worktrees are
  disposable once merged, per `docs/codestyle.org`'s worktree policy).
- Every later step with both new code and a blog deliverable (L11, L13, L15, L19, L21, L24 per
  docs/cl-pivot-plan.md section 8) will hit the identical problem for its own new anchors, unless the
  orchestrator's merge step for a docs-bearing branch is amended to include this repointing as a standard
  post-merge action, or the transclusion convention itself changes (e.g. always pointing new links at the
  worktree that authored them, with the understanding that `main`'s copy of the `.org` gets the path fixed up
  once, at merge time).
- This does not affect any *existing* phase's transclusion links (5-12, 15); those still point at `main` and
  were not touched by this step.

## Revisit condition

Close this divergence once `docs/blog/phase-16-reading-common-lisp.org`'s six orgit links are re-pointed from
`~/src/compile-time-scheme/wt-l6` to `~/src/compile-time-scheme/main` after this branch merges (a
one-line-per-link `sed` change, no content change), and note whether the orchestrator adopts a standing
convention for the steps listed above so this does not need a fresh divergence doc each time.
