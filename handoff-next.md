# Next step: Step 0

## Goal

Create the greenfield SchemePoC skeleton from the copied baseline, replacing example names with `smd/schemepoc`.

## Files expected to change

```txt
src/smd/schemepoc/CMakeLists.txt
src/smd/schemepoc/version.hpp
src/smd/schemepoc/version.cpp
src/smd/schemepoc/version.test.cpp
```

## Context from previous step

Step -1 installed agent governance files (`AGENTS.md`, `docs/codestyle.org`, `docs/CODING_RULES.md`, `handoff.md`, `handoff-next.md`, `checklist.md`).

The existing repo has a placeholder `schemepoc.hpp`/`schemepoc.cpp`/`schemepoc.test.cpp` triple under `src/smd/schemepoc/` that returns a name string.
This should be replaced or adapted to the `version.hpp` skeleton specified in the plan.

## Required implementation details

- Add `version.hpp` with `version_major`, `version_minor`, `version_patch` inline constexpr variables in `smd::schemepoc`.
- Add a minimal `version.cpp` (may be empty or trivial).
- Add `version.test.cpp` with double-include verification and version constant check.
- Update `CMakeLists.txt` to list the new files.

## Required tests

- Header idempotency test (double include).
- `version_major == 0` check.

## Required commands

```bash
make compile
make test
make lint
```

## Do not do

- Do not proceed to Step 1.
- Do not introduce optional dependencies unless this step requires them.
- Do not change architecture decisions without documenting the reason in `handoff.md`.
