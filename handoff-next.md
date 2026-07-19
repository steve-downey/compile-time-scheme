# Next steps: Common Lisp pivot, after Step L9

> **2026-07-18 update:** L9 (Lisp-2 environment) is now landed (see the
> `DONE` marker below and `handoff.md`'s "2026-07-18 Step L9: Lisp-2
> environment landed" section for the full env API, the closure-capture
> ownership decision, and the builtin dispatch story).
> Per `checklist.md`, the remaining open steps on Track A/B are **L5** (CL
> atoms, depends on L4, done — may be in progress or already landed by a
> parallel worker; check `checklist.md` before starting it) and, once both
> **L6** (CL datum reader, depends on L5) and **L9** (done) are landed,
> **L10** (CL core model and baseline elaborator).
> Read `handoff.md`'s L7/L8/L9 sections before touching `src/smd/smdlisp/closure/**`
> or writing an elaborator/evaluator against it — they record real
> constraints, not just history.

## Direction

The project is pivoting from Scheme-light to Common Lisp-light semantics.
The full rationale, reuse inventory, gap analysis, decision records (D1-D10), orchestration protocol, and step plan are in:

```txt
docs/cl-pivot-plan.md
```

Read that file, plus `handoff.md`, before doing anything.

## What exists already

- `docs/cl-pivot-plan.md` — the plan.
- `docs/divergences/TEMPLATE.md`, `DIV-0001-single-package-and-case.md` (D2/D7), `DIV-0002-eq-eql-conflated.md` (L8) — accepted ANSI divergences.
- `checklist.md` — Steps L0, L1, L2, L3, L4, L7, L8, L9 are ticked; L5, L6, L10 onward are open.
- `src/smd/smdlisp/{CMakeLists.txt,version.hpp,version.cpp,version.test.cpp}` — the L1 skeleton.
- `src/smd/smdlisp/reader/{cl_chars.hpp,cl_chars.test.cpp,CMakeLists.txt}` — the L4 lexical layer.
- `src/smd/smdlisp/closure/{value.hpp,value.test.cpp,pairs.hpp,pairs.test.cpp,env.hpp,env.test.cpp,CMakeLists.txt}` — the L7/L8/L9 value model, cons cells, and Lisp-2 environment.

## Actual CMake target names (read before adding subdirectories)

```txt
smdlisp.smdlisp   -- STATIC library target, links smdlisp.reader + smdlisp.closure (+ future subdirs)
smdlisp.reader    -- STATIC header-only, links smdscheme.parser
smdlisp.closure   -- STATIC header-only, links smdscheme.foundation
smdlisp_test      -- Catch2 test executable, globs *test.cpp recursively under src/smd/smdlisp
```

Each new `smdlisp/<subdir>/` (macroexpand, elaborator, sender) should follow the same `smdlisp.<subdir>` naming pattern and get linked `PUBLIC` into `smdlisp.smdlisp` in `src/smd/smdlisp/CMakeLists.txt`.
New `.test.cpp` files anywhere under `src/smd/smdlisp/` are picked up automatically by `smdlisp_test`'s glob — no per-file CMake registration needed for tests, only for `FILE_SET HEADERS` entries.

## What Step L9 actually built (read `handoff.md` for full detail)

- `env<Core, MaxBindings>` (`src/smd/smdlisp/closure/env.hpp`): two independent, linear, most-recent-first namespaces (`define_value`/`lookup_value`, `define_function`/`lookup_function`), no `store`/mutation yet (that is L12's `setq`), no parent-environment link (nested scope = copy-and-extend, matching the Scheme original's architecture).
- `default_env<Core, MaxBindings>()` installs `+ * CONS CAR CDR LIST NULL EQ EQL ATOM FUNCALL APPLY` into the *function* namespace, as `value<Core>{builtin{builtin_op::<name>}}`.
- `value.hpp`'s `builtin_op` enum is now the superset `{add, multiply, cons, car, cdr, list, null, eq, eql, atom, funcall, apply}` — the single tag type installed into environments. `pairs.hpp`'s `list_op`/`apply_prim` are unchanged and still the only thing that actually *executes* a list primitive; the two enums share names by construction but are different types (header-cycle reasons, see `handoff.md`). **Whichever step wires a looked-up `builtin` value to actual execution (plausibly L10's elaborator turning call syntax into a dispatch, or L11's evaluator) needs a `builtin_op -> list_op` bridge for the eight shared names, and real `funcall`/`apply` semantics (invoke a `closure<Core>` value) that `apply_prim` deliberately does not provide.**
- `closure<Core, MaxBindings = 16>` in `value.hpp` is now parameterized (was hard-wired to `env<Core, 16>`); `captured` is still a **non-owning raw pointer** to `env<Core, MaxBindings>`, not an owning deep copy — this was a forced decision (header-cycle reasons), not a stylistic one. **Whichever step first constructs real closures (L10/L11) must decide where the `env` instances a closure captures actually live** — the L9 handoff entry sketches an "env arena" (stable-index, `pair_heap`-style) as the natural fit; a raw pointer onto a C++ call-stack local `env` would dangle once the constructing call returns, so this cannot be deferred forever.

## Standing constraints

- `src/smd/smdscheme/**` is frozen for semantic changes (decision D1); blog phases 5-12 transclude live code from it by UUID anchor.
  Do not edit anything under `src/smd/smdscheme/**`; only read it as a reference pattern or link against its existing targets.
- New work goes in `src/smd/smdlisp/`, namespace `smd::smdlisp`, one sub-namespace per directory, per plan section 7.
- Keep C++26 and GCC16 as the baseline; tests use Catch2; mirror the file prolog / include guard / canonical-include conventions already used throughout `src/smd/smdlisp/`.
- File a divergence doc under `docs/divergences/` (using `TEMPLATE.md`) for anything done differently than the plan specifies, or any knowing ANSI CL deviation. An internal C++ implementation-technique decision (like L9's closure-capture-ownership choice) does not need one; a semantic/API deviation does.
- Before handoff, run and report: `make compile`, `make test`, `make lint`.
- Do not continue past your assigned step unless blocked; if blocked, document the blocker here instead.

## Leftover pre-pivot cleanup (background, low priority)

Carried over, still not addressed:

- Audit for remaining legacy `smd::schemepoc` references where `smd::smdscheme` is intended.
- Verify `docs/compiler_architecture.org` transclusions still point at intended files and UUID spans.
- Remove ad-hoc migration debugging scripts/logs not required for the build or docs flow.
- The root `CMakeLists.txt`'s `beman_install_library(schemepoc.schemepoc TARGETS ...)` list does not yet include any `smdlisp.*` target; revisit when `smdlisp` needs `make install`/`make testinstall` support (plausibly around L22).
