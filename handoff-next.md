# Next steps for future development

## Current State

The namespace-migration stabilization pass is complete enough for normal development again.
The project currently satisfies:

- `make compile` passes
- `make test` passes
- `make lint` passes

The largest breakages from global namespace rewrites were fixed in parser, foundation, CPS, sender, and eval-direct paths.

## Recommended next work

1. Documentation alignment pass
	- Update user-facing docs to consistently refer to `smdscheme` paths/names where migration changed spelling.
	- Verify `docs/compiler_architecture.org` transclusions still point at intended files and UUID spans.

2. Namespace-cleanup pass
	- Audit for remaining legacy `smd::schemepoc` references where `smd::smdscheme` is now intended.
	- Keep compatibility aliases only if intentional and documented.

3. Repository hygiene pass
	- Review ad-hoc helper scripts/logs created during migration debugging and remove anything not required for the build or docs flow.
	- Keep generated and checked-in trees clean and minimal.

## Guardrails

- Keep C++26 and GCC16 as the baseline.
- Keep Catch2 tests and existing merged `src/` layout conventions.
- Before handoff, run and report: `make compile`, `make test`, `make lint`.
