# Next steps: Common Lisp pivot, after Step L9

> **2026-07-18 update:** L4, L5, L7, L8, and L9 are all landed (see the
> `DONE` markers below and `handoff.md`'s corresponding dated sections —
> L9's records the full env API, the closure-capture ownership decision,
> and the builtin dispatch story).
> **L6** (CL datum reader, depends on L5, done) is dispatched and in
> flight; once it lands, **L10** (CL core model and baseline elaborator,
> depends on L6 + L9) is the next step, unifying Track A and Track B.
> Read `handoff.md`'s L7/L8/L9 sections before touching
> `src/smd/smdlisp/closure/**` or writing an elaborator/evaluator against
> it — they record real constraints, not just history.
> The rest of this file below is historical dispatch notes, annotated
> where stale; do not re-run L4/L5/L7/L8/L9.

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

Per plan section 9, these two steps both depend only on L1 (done) and are explicitly independent of each other — the plan's parallelism summary (section 9, "Parallelism summary") puts L4 on Track A (code spine: L1 -> L4 -> L5 -> L6 -> ...) and L7 on Track B (values: L7 -> L8 -> L9, starting right after L1, parallel to L4-L6).
Dispatch them as separate workers in separate worktrees.

### Step L4 — CL lexical layer — DONE (2026-07-18, in worktree cl-pivot/l4-cl-chars)

Landed: `src/smd/smdlisp/reader/{cl_chars.hpp,cl_chars.test.cpp,CMakeLists.txt}`, `smdlisp.reader` CMake target, wired into `src/smd/smdlisp/CMakeLists.txt`.
See `handoff.md` "2026-07-18 Step L4: CL lexical layer landed" for the exposed predicate names and a naming hazard worth reading before L5/L6 touch this header: the cursor-consuming skip function is named `skip_cl_intertoken_space`, not `skip_intertoken_space`, to avoid a permanent ADL ambiguity against `smdscheme::parser::skip_intertoken_space` (same `cursor` argument type). Apply the same care to any new `smdlisp` function taking a `smdscheme::parser::cursor` by value.
`make compile`/`make test` (295/295)/`make lint` all green at landing; `src/smd/smdscheme/**` untouched.
No divergence doc was needed for this step (predicate shape and comment/case-fold behavior follow the plan directly).

### Step L5 — CL atoms — DONE (2026-07-18, in worktree cl-pivot/l5-cl-atoms)

Landed: `src/smd/smdlisp/reader/{atom.hpp,atom.test.cpp}`, added to the existing `smdlisp.reader` target's `FILE_SET` (no new CMake target).
`atom = std::variant<atom_integer, atom_symbol, atom_keyword>`; `atom_keyword` is a distinct kind from `atom_symbol` per D7 (spelling stored without the leading `:`). Both hold a `folded_name` (own fixed storage, not a `string_view` into source — folding per D2 rewrites characters). Public parsers: `integer_p()`, `symbol_p()`, `keyword_p()`, `atom_p()` (tries them in that order), and `read_atom(std::string_view)` as the convenience entry point (`read_atom("foo")` → symbol `FOO`; `read_atom(":bar")` → keyword `BAR`). `t`/`nil` are not special-cased; they read as ordinary symbols `T`/`NIL`.

**Read before touching this file again, especially for L6:** integer recognition is NOT greedy-digit scanning (that was the initial port from `smdscheme::reader::integer_p` and it failed `make test` on `read_atom("1+")`, since CL symbols may start with a digit). The fix, and the reason, is `docs/divergences/DIV-0003-atom-maximal-munch.md` — atoms are read as a whole maximal-munch token first, then classified as integer-or-symbol. L6's datum reader should call `atom_p()` (or `read_atom` for whole-string cases) rather than reimplementing token scanning, or it will likely reintroduce the same bug.

`make compile`/`make test` (333/333, including 37 new `AtomTest` cases)/`make lint` all green at landing; `src/smd/smdscheme/**` untouched (`git diff -- src/smd/smdscheme` empty).
One divergence doc filed: `docs/divergences/DIV-0003-atom-maximal-munch.md` (plan sketch "same rules as Scheme side" for integers didn't survive contact with CL's digit-permissive symbol syntax; category 2 divergence — step implemented differently than the plan specifies).

### Step L7 — CL value model — DONE (2026-07-18, in worktree cl-pivot/l7-cl-value)

Landed: `src/smd/smdlisp/closure/{value.hpp,value.test.cpp,CMakeLists.txt}`, `smdlisp.closure` CMake target, wired into `src/smd/smdlisp/CMakeLists.txt`.
See `handoff.md` "2026-07-18 Step L7: CL value model landed" for the value-kind list and, importantly, the raw-pointer captured-env constraint in `closure<Core>` (`env<Core, 16> const *captured`) that L9 must address when it builds real `env`.
`make compile`/`make test` (296/296)/`make lint` all green at landing; `src/smd/smdscheme/**` untouched.

### Step L8 — Cons cells and list builtins — DONE (2026-07-18, in worktree cl-pivot/l8-cons-cells)

Landed: `src/smd/smdlisp/closure/value.hpp` gained `pair_ref`/`pair_cell`/`pair_heap` (adapted by copy from Scheme's PR #26) and `pair_ref` as a new `value<Core>`/`foreign_function<Core>::val_t` alternative; new files `src/smd/smdlisp/closure/{pairs.hpp,pairs.test.cpp}`, both wired into the existing `smdlisp_closure_headers` FILE_SET (no new CMake target — `pairs.hpp` joins `value.hpp` under `smdlisp.closure`).
See `handoff.md` "2026-07-18 Step L8: cons cells and list builtins landed" for the full API surface: `enum class list_op { cons, car, cdr, list, null, eq, eql, atom }` and `apply_prim<Core, MaxPairs>(list_op, std::span<value<Core> const>, pair_heap<Core, MaxPairs>*) -> foundation::result<value<Core>>`.
Read it before L9 or L10 touch pairs: it records the CL deltas (`nil`, not a separate `null_t`, is the empty list; `car`/`cdr` of `nil` is `nil`; predicates return the symbol `T` or `nil`, not `bool`) and points at `docs/divergences/DIV-0002-eq-eql-conflated.md` (new this step: `eq`/`eql` are implemented identically, same simplification Scheme made for `eq?`/`eqv?`, currently unobservable since there are no numeric types beyond `int`).
`list_op` is a free-standing enum, not reused from an `elaborator::prim_op` — `smdlisp` has no elaborator yet (L10). Whichever step adds a `core_prim`-equivalent node should map its primitive-symbol table onto `list_op` directly rather than reinventing the op set.
`make compile`/`make test` (316/316, including 11 new `PairsTest` cases)/`make lint` all green at landing; `src/smd/smdscheme/**` untouched (the `pairs.test.cpp` glob hit under `src/smd/smdscheme/` in build logs is a pre-existing, unrelated file at that path, not something this step created or touched).

## Standing constraints (apply to both steps)

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
