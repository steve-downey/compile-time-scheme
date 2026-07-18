# SchemePoC handoff

## Project

This is `smd/schemepoc`, a compile-time Scheme-light proof of concept in C++26 on GCC16.

The architecture is intentionally staged:

```txt
source string
  -> reader datum tree
  -> elaborated core tree
  -> CPS / defunctionalized program
  -> closure backend
  -> Beman Execution sender backend
  -> reflection reification spike
```

The reader parses Scheme data.
It must not classify special forms except as ordinary lists.

The elaborator owns recognition of `if`, `lambda`, calls, quote semantics, and later binding behavior.

The first implementation uses fixed-capacity constexpr arenas instead of heap-backed recursive ASTs.
This is deliberate.
It keeps the Godbolt demo small and avoids constexpr allocation persistence issues.

A generic `Fix<F>` playground may exist separately.

The closure backend is the stable demonstration path.
It is surfaced via the `compiled_closure<"...">` one-shot API in `schemepoc.hpp`.
The sender backend uses Beman Execution through a project adapter if helpful.
Reflection remains isolated until explicitly integrated.

## Rule precedence

`docs/codestyle.org` is authoritative.

Rule order:

```txt
docs/codestyle.org
  > AGENTS.md
  > docs/CODING_RULES.md
  > CLAUDE.md
  > handoff.md / handoff-next.md / checklist.md
```

## Baseline

C++26 is the baseline.

GCC16 is the baseline compiler.

Tests use Catch2.

Do not introduce GTest.

## Sender dependencies

Beman Execution is the sender backend dependency.

It is vendored as a git submodule under:

```txt
vendor/execution
```

Beman Task is added only if needed.

If added, it is vendored as a git submodule under:

```txt
vendor/task
```

Use `add_subdirectory`.

Do not use FetchContent, vcpkg, install-time discovery, or git subtree for these dependencies.

## Build

Run:

```bash
make compile
make test
make lint
```

All steps must keep those green.

## Namespace

Use:

```cpp
namespace smd::schemepoc {
}
```

## Layout

Use merged `src/` layout:

```txt
src/smd/schemepoc/
```

Headers, implementations, and tests live together by component.

## Style

- Canonical repository-relative file path and Emacs mode line first.
- SPDX immediately after.
- Include guards, no `#pragma once`.
- Canonical angle-bracket includes only.
- No relative project includes.
- No `using namespace` in headers.
- Test files include the component header first and twice.
- Prefer `constexpr` everywhere practical.
- Use C++26 directly.

## Architecture facts

- `core_tree` and `core_node` internal variant structures must NEVER embed `value` or `closure` types (or anything containing dynamically allocating types). The entire AST nodes must be trivially destructible. This is to avoid constructor/destructor boundary leak errors in C++26 `constexpr` evaluating closures out to global space (`compiled_closure`).
- `value`, `environment`, and `closure` are distinct concepts that only exist during evaluation (`eval_direct`). They do not bleed into the `core_node` AST variations. For example, `core_quote` stores a variant of literal constants (`int`, `bool`, `std::string_view`) rather than `value` directly.
- Nested lambda capturing and lexical closure state tracking is explicitly managed with a custom `constexpr_box<env<MaxList>>` type which wraps pointer manual lifetime in `value.hpp`.
- AST arrays (`core_type` and `datum_type`) rely on integer-ID-based `tree_arena` lookups instead of internal pointer structures. This strictly forces compiler execution boundaries when instantiating constexpr components.
- The C++26 `constexpr` engine lambda execution requires `tree_arena` elements to be captured BY VALUE within returned compiler expressions (such as `cps_code`) to outlive source object lifetimes.

- The sender backend (`sender_backend.hpp`) uses Beman Task (`vendor/task`) coroutines to model Scheme AST interpretations as asynchronous, non-blocking state machines over the `core_tree`. Returning a `task<result<value>>` breaks the static C++ variant recursion limits inherent to dynamically chained deeply-recursive AST structures statically.
- The `sender_adapter.hpp` facades bridging Beman Task using standard primitives: `make_ready_future` (just/value return), `then`, variables capturing cleanly.
- The `reflection_reify.hpp` spike successfully uses C++26 standard reflection (`std::meta::define_aggregate` via `^^int` and `-freflection`) to dynamically generate aggregate structures representing captured environments at compile time, completely outside evaluation mechanisms.

## 2026-05-26 stabilization notes

- The namespace-split tree under `src/smd/smdscheme/` now compiles and passes tests/lint after a targeted qualification repair pass.
- `make compile`, `make test`, and `make lint` all pass in the current workspace state.
- Parser adapter/tests were repaired to use `smd::smdscheme::parser` symbols explicitly where prior global lookups had been broken.
- Foundation tests were repaired to use `smd::smdscheme::foundation` symbols explicitly.
- CPS and sender paths were repaired for moved symbols in `closure` and `elaborator` namespaces.
- `src/smd/smdscheme/elaborator/eval_direct.hpp` and `src/smd/smdscheme/elaborator/eval_direct.test.cpp` were rewritten to remove regex-introduced corruption and restore a compiling direct-evaluator path.
- `src/examples/hello.cpp` now prints `smdscheme v<major>.<minor>.<patch>`, matching the example test expectation.

## 2026-07-18 Common Lisp pivot

The project pivoted from Scheme-light to Common Lisp-light semantics.
Rationale is in `docs/cl-pivot-plan.md` section 0: the sender backend's one-shot completion contract cannot express multishot `call/cc`, and full `call/cc` is unsound with `dynamic-wind`/resource cleanup regardless.
Common Lisp's nonlocal control operators (`block`/`return-from`, `catch`/`throw`, `tagbody`/`go`, `unwind-protect`) are dynamic-extent and one-shot by design, which is exactly what CPS and sender backends can express soundly.

`src/smd/smdscheme/**` is now frozen for semantic changes.
Blog phases 5-12 transclude live code from it by UUID anchor; an in-place pivot would silently rewrite published posts.
The Scheme pipeline stays buildable, tested, and demoable as-is.

New work lives in `src/smd/smdlisp/`, namespace `smd::smdlisp`, one sub-namespace per directory, mirroring the `smdscheme` component structure:

```txt
src/smd/smdlisp/CMakeLists.txt
src/smd/smdlisp/smdlisp.hpp
src/smd/smdlisp/reader/
src/smd/smdlisp/macroexpand/
src/smd/smdlisp/elaborator/
src/smd/smdlisp/closure/
src/smd/smdlisp/sender/
```

`smdlisp` consumes `smdscheme`'s language-agnostic `foundation` and `parser` targets via their canonical includes and CMake targets; it does not copy them.
The Scheme-flavored components (`reader`, `elaborator`, `closure`, `sender`) are adapted by copy into `smdlisp`, then diverge freely.

Decision records D1-D10 in `docs/cl-pivot-plan.md` section 4 govern the pivot and are binding unless overturned by a divergence doc plus orchestrator sign-off: D1 layout/frozen-tree, D2 uppercase case folding at read time, D3 `nil`/`t` semantics, D4 Lisp-2 namespaces, D5 one-shot dynamic-extent exits (the thesis), D6 proper lists first, D7 one package plus keywords, D8 `tagbody`/`go` optional, D9 macro expansion as a separate datum-to-datum pass, D10 out-of-scope features.

`nil` is the sole false value in `smdlisp`; truthiness goes through one `is_true` function, never per-site encodings (D3).
All nonlocal control is one-shot and upward-only, dynamic-extent per CL; a dead exit is a diagnosed error, not undefined behavior (D5).

Divergence issue docs live in `docs/divergences/`, one numbered file per issue, named `DIV-NNNN-short-slug.md`, using `docs/divergences/TEMPLATE.md` as the skeleton.
File one when the implementation knowingly deviates from ANSI Common Lisp semantics, when a step is implemented differently than `docs/cl-pivot-plan.md` specifies, or when a frozen-tree edit inside a `src/smd/smdscheme/**` UUID anchor block is unavoidable.
`docs/divergences/DIV-0001-single-package-and-case.md` is the first, seeded for D2/D7 (single package, keywords only, uppercase-fold-only reader), status `accepted-permanent`.

## 2026-07-18 Step L2 (blog phase 15)

`docs/blog/phase-15-why-common-lisp.org` is drafted (`DRAFT — pending author revision`), covering plan section 0: the sender one-shot completion contract, why multishot `call/cc` cannot ride it, Kiselyov's independent argument against `call/cc`, Common Lisp's dynamic-extent control operators, the thesis, and what the pivot does and does not change.
`docs/blog/index.org` gained a Phase 15 entry (and, in the process, a Phase 14 entry it was missing before this step).
`make blog-md` renders phase-15 cleanly; `docs/blog/phase-14-set-bang.md` and `docs/blog/index.md` had never been generated/committed before this step and are now included as part of keeping the rendered set consistent with their `.org` sources — no existing phase `.org`/`.md` content changed.
`docs/blog/references.bib`'s `beman_execution` entry has no `author` field; citing it with `[cite:@beman_execution]` breaks the GFM export with an opaque `org-element-insert-before: No location found to insert node` error. Give it an `author` field before citing it, or avoid citing it.
