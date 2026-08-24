# BL-0008: `make lint`'s auto-fixing hooks pass on the rerun, so their repairs never reach the commit

- **Status:** open — three occurrences in one fan-out; wants a guard, not another warning
- **Date:** 2026-08-24
- **Origin:** Orchestrator acceptance sweeps during the `tmp/plan/` fan-out.
  Steps A1, A4 and B2 each merged with formatting the hooks had already
  rewritten but nobody had staged, and each needed a follow-up commit on the
  integration branch.
- **Frozen-tree impact:** none.

## What

Several `.pre-commit-config.yaml` hooks are *fixers* rather than *checkers* —
`clang-format`, `gersemi` ("CMake linting"), `trailing-whitespace`,
`end-of-file-fixer`. A fixer rewrites the file and exits non-zero on the run
that did the rewriting, then exits zero on the next run because there is
nothing left to fix.

So the sequence a careful author naturally performs —

```sh
make lint     # fails, hooks rewrite files
make lint     # passes
git add -A && git commit
```

— is safe, but the sequence that actually happened three times is:

```sh
make lint          # fails, hooks rewrite files
make lint          # passes; author records "lint green"
<edit one more file, or commit an earlier staged state>
git commit         # the rewrite is not in it
```

The result is a commit that is green for its author and red for the next
person to run `make lint` on a fresh checkout, whose first run rewrites the
files again.

A guard would make this mechanical. The obvious shape is a Makefile target
that runs the hooks and then fails if the working tree is dirty:

```make
lint-strict: lint
	@git diff --quiet || { \
	  echo "lint rewrote tracked files; stage them and re-run"; \
	  git diff --stat; exit 1; }
```

Whether that becomes `lint` itself, a separate target, or a `pre-push` hook is
the open question below.

## Why it is not done

It is a change to shared tooling that every future step and every human commit
would run, made in the middle of a fan-out whose steps are sized against the
current `make lint`. Changing the acceptance tooling underneath running work is
how a plan acquires failures it cannot attribute. It also wants a decision
about which target changes, which is not the orchestrator's to take alone.

## Evidence

Three occurrences, each caught by the orchestrator's post-merge acceptance run
and each requiring a fix commit on the integration branch:

- **A1** — a trailing blank line in `docs/history/architecture-iterations.org`,
  removed by `end-of-file-fixer`. Fixed in `9fb0152`.
- **A4** — `clang-format` reflow of `cl/printer/prin1.hpp` and
  `prin1.test.cpp`, plus `gersemi` collapsing two blocks in
  `cl/printer/CMakeLists.txt`. Fixed in `a50345c`.
- **B2** — `clang-format` reflow of four files under `kit/parser/`, plus
  `gersemi` on `kit/parser/CMakeLists.txt`. Verified formatting-only by
  hashing each file with all whitespace stripped: identical before and after.

The A4 and B2 workers were both warned about this failure mode explicitly, in
writing, before they started. **The warning did not prevent it**, which is the
argument for a mechanical guard: the trap is not that the author is careless,
it is that the evidence they check — a green `make lint` — is genuinely green
at the moment they check it.

## Open questions

- Should `lint` itself fail on a dirty tree, or should that be a separate
  `lint-strict` used by acceptance and CI? Making `lint` strict is safer and
  is mildly hostile to the ordinary edit-lint-edit loop, where a rewrite is
  expected and welcome.
- ~~Does CI already catch this?~~ **Answered: yes.**
  `.github/workflows/pre-commit-check.yml` ("Lint Check (pre-commit)") runs the
  hooks on a clean checkout via the Beman reusable workflow, so each of the
  three commits above would have gone red there. That reframes the item rather
  than closing it: the failure is caught, but only after a push, by a job whose
  red has to be traced back to a step that reported itself green. The value of
  a local guard is attribution and latency, not detection. It also means an
  unattended fan-out that pushed per step — this one did not — would have
  produced three red CI runs.
- Would installing the hooks as actual git pre-commit hooks (`pre-commit
  install`) solve it more directly for humans, while doing nothing for agents
  that call `git commit` with hooks bypassed or absent?

## Cost and risk

The Makefile target is about five lines. The risk is in the choice, not the
code: a strict `lint` that fails on a dirty tree will surprise anyone running
it mid-edit, and a separate target only helps the callers who remember to use
it — which is the same class of problem as the warning that already failed
three times.

## Decision criteria

Worth scheduling as soon as the current fan-out closes, when changing the
acceptance tooling can no longer confuse the metrics it is being measured by.
Worth closing as `declined` if the answer to the second open question is that
CI already fails these commits, since then the gap is only in local
acceptance and the orchestrator's sweep is the guard.
