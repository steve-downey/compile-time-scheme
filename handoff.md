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

The `docs/blog/%.md` Makefile rule's generated `.md.deps` now also captures `orgit:` transclusion targets (previously only `[[file:...]]` links), so blog posts rebuild when their transcluded `smdscheme` source files change (Step L3).

## 2026-07-18 Step L1: smdlisp skeleton landed

`src/smd/smdlisp/` now exists with `CMakeLists.txt`, `version.hpp`, `version.cpp`, `version.test.cpp`, wired into `src/smd/CMakeLists.txt` via `add_subdirectory(smdlisp)` (added after `smdscheme`).

CMake target names, chosen to mirror `smdscheme`'s own top-level umbrella target and to stay forward-compatible with the section-7 layout (`reader/`, `macroexpand/`, `elaborator/`, `closure/`, `sender/` subdirectories arriving in later steps, each contributing its own `smdlisp.<subdir>` target that the umbrella target will eventually link, exactly as `smdscheme.smdscheme` links `smdscheme.foundation`, `smdscheme.parser`, etc.):

```txt
smdlisp.smdlisp   -- STATIC library target, currently just version.{hpp,cpp}
smdlisp_test      -- Catch2 test executable, globs *test.cpp under src/smd/smdlisp recursively
```

`smdlisp.smdlisp` links `PUBLIC` against `smdscheme.foundation` and `smdscheme.parser` (both already-built `smdscheme` targets, consumed via their canonical includes, per D1 "use as-is, no copy").
`version.hpp` proves the dependency direction compiles and links by defining two trivial `constexpr` functions, `links_smdscheme_foundation()` and `links_smdscheme_parser()`, that touch `smd::smdscheme::foundation::version_major` and construct a `smd::smdscheme::parser::cursor`; both are exercised by `static_assert` and by a Catch2 `TEST_CASE` in `version.test.cpp`.

`src/smd/smdscheme/**` was not touched (verified via `git diff -- src/smd/smdscheme` showing nothing).

`make compile`, `make test` (286/286 passed, including the 3 new `VersionTest` cases), and `make lint` all passed at the point of landing this step.

The root `CMakeLists.txt`'s `beman_install_library(schemepoc.schemepoc TARGETS ...)` list was intentionally left unchanged; it does not yet list any `smdlisp.*` target, and adding install/export wiring for `smdlisp` was out of scope for L1 (`make compile`/`make test`/`make lint` do not exercise `make install`). A later step (plausibly L22, public API) should revisit whether `smdlisp.smdlisp` needs to join that install list.

## 2026-07-18 Step L2 (blog phase 15)

`docs/blog/phase-15-why-common-lisp.org` is drafted (`DRAFT — pending author revision`), covering plan section 0: the sender one-shot completion contract, why multishot `call/cc` cannot ride it, Kiselyov's independent argument against `call/cc`, Common Lisp's dynamic-extent control operators, the thesis, and what the pivot does and does not change.
`docs/blog/index.org` gained a Phase 15 entry (and, in the process, a Phase 14 entry it was missing before this step).
`make blog-md` renders phase-15 cleanly; `docs/blog/phase-14-set-bang.md` and `docs/blog/index.md` had never been generated/committed before this step and are now included as part of keeping the rendered set consistent with their `.org` sources — no existing phase `.org`/`.md` content changed.
`docs/blog/references.bib`'s `beman_execution` entry had no `author` field, which broke the GFM export with an opaque `org-element-insert-before: No location found to insert node` error when cited; this step added the missing `author` field (a bib entry without one cannot be cited safely).

## 2026-07-18 Step L4: CL lexical layer landed

`src/smd/smdlisp/reader/{cl_chars.hpp,cl_chars.test.cpp,CMakeLists.txt}` landed, wired into `src/smd/smdlisp/CMakeLists.txt` via `add_subdirectory(reader)` plus `target_link_libraries(smdlisp.smdlisp PUBLIC ... smdlisp.reader)`, following the `smdlisp.<subdir>` target naming pattern from L1's handoff note. New CMake target: `smdlisp.reader` (STATIC "" header-only, mirrors `smdscheme.parser`'s pattern), linking `PUBLIC smdscheme.parser`.

`cl_chars.hpp` (namespace `smd::smdlisp::reader`) reuses `smdscheme::parser::cursor` unchanged and supplies: `is_whitespace`, `is_terminating_macro_char` (currently `( ) ' ;` only — `" \` , #` are deferred to later steps per D6/L6/L18), `is_constituent_char`, `is_delimiter` (all `char -> bool`), `to_upper_char` (single-char uppercase fold, decision D2), `skip_line_comment` (`;` to end of line/input), and `skip_cl_intertoken_space` (interleaves whitespace and `;`-comment skipping to a fixpoint).

Naming note for future steps: the cursor-consuming function is named `skip_cl_intertoken_space`, not `skip_intertoken_space`, deliberately. `smdscheme::parser::skip_intertoken_space(cursor)` already exists, and because both take a `smdscheme::parser::cursor` argument, an identically-named `smdlisp` overload would be pulled into every unqualified call site by argument-dependent lookup on `cursor`'s namespace — permanently ambiguous, not just a one-time collision. This was caught by `make compile` failing on the test file; confirmed by build, not guessed. Any future `smdlisp` function taking a `smdscheme::parser::cursor` by value should pick a name distinct from existing `smdscheme::parser` free functions for the same reason. Functions taking only `char` (`is_delimiter`, etc.) do not have this hazard since fundamental types have no associated namespaces for ADL.

`make compile`, `make test` (295/295 passed, including 9 new `ClCharsTest` cases), and `make lint` all passed at the point of landing this step. `git diff -- src/smd/smdscheme` is empty; the only changes are the new `src/smd/smdlisp/reader/` directory and the one-line `add_subdirectory`/link edit to `src/smd/smdlisp/CMakeLists.txt`.

## 2026-07-18 Step L7: CL value model landed

`src/smd/smdlisp/closure/{value.hpp,value.test.cpp,CMakeLists.txt}` now exist, wired via `add_subdirectory(closure)` in `src/smd/smdlisp/CMakeLists.txt`; target `smdlisp.closure` (STATIC, header-only, mirroring how `smdscheme.closure` is STATIC with no `.cpp` files) is linked `PUBLIC` into `smdlisp.smdlisp`, which links `PUBLIC smdscheme.foundation` for `foundation::result`.

`smd::smdlisp::closure::value<Core>` is `std::variant<nil_t, int, symbol, keyword, builtin, closure<Core>, foreign_function<Core>>` — no pair/cons alternative yet (arrives L8). `nil_t` is the sole false value (D3); `is_true(value<Core> const&) -> bool` is `!std::holds_alternative<nil_t>(v)` and is meant to be the only truthiness check used anywhere in `smdlisp`.

Important structural note for **L9** (Lisp-2 environment): `closure<Core>` currently captures its environment as a **raw, non-owning pointer** (`env<Core, 16> const *captured = nullptr;`) against a forward-declared `env` template, not the `constexpr_box`-owned deep-copy pattern `smdscheme::closure::closure` uses. This is a hard C++ constraint, not a style choice: `std::variant` requires every alternative's destructor to be instantiable as soon as any `value<Core>` object is constructed/destroyed anywhere (verified empirically — `constexpr_box<env<Core,16>>` as a member forces instantiation of `~constexpr_box()`, which needs `env<Core,16>` complete, at the point `value<Core>` is first used, not merely declared). Since `env.hpp` does not exist until L9, `value.hpp` cannot embed an owning box over an incomplete `env`. A raw pointer sidesteps this because pointers to incomplete types need no destructor machinery. **L9 (or whichever step first builds real closures with captured environments) will need to decide how `closure<Core>::captured` becomes an owning/deep-copyable reference once `env` exists** — likely reintroducing `constexpr_box<env<Core,16>>` at that point, once `env.hpp` is included ahead of `value.hpp`'s use sites, or restructuring so `env` owns/shares state some other way (e.g. the `store`-style shared-arena pattern `smdscheme` already uses for mutable bindings, which is non-owning-pointer-based and wouldn't hit this problem).

`make compile`, `make test` (296/296 passed, including 14 new `ValueTest` cases), and `make lint` all passed at the point of landing this step. `git diff -- src/smd/smdscheme` was empty (frozen tree untouched).

## 2026-07-18 Step L8: cons cells and list builtins landed

`src/smd/smdlisp/closure/value.hpp` gained a `pair_ref`/`pair_cell`/`pair_heap` trio adapted by copy from `smd::smdscheme::closure::pair_ref`/`pair_cell`/`pair_heap` (PR #26), plus `pair_ref` as a new alternative in both `value<Core>` and `foreign_function<Core>::val_t`. Per decision D3, there is **no** separate `null_t` empty-list kind in `smdlisp` — unlike Scheme, `nil_t` (already landed in L7) serves as both the sole false value and the empty list, so a proper list's final `cdr` is a plain `nil_t` value. `pair_ref` equality is heap-location identity, matching the Scheme original.

`src/smd/smdlisp/closure/{pairs.hpp,pairs.test.cpp}` are new, wired into the `smdlisp_closure_headers` FILE_SET in `src/smd/smdlisp/closure/CMakeLists.txt` (no new CMake target; `pairs.hpp` joins the existing `smdlisp.closure` target's headers alongside `value.hpp`). `pairs.hpp` defines `enum class list_op { cons, car, cdr, list, null, eq, eql, atom }` (CL names, no `?`/`!` suffixes — narrower than Scheme's `prim_op`, which also has `set_car`, `set_cdr`, `pairp`, `eqv`, `equal`; those are not part of the L8 builtin set per the plan) and `apply_prim<Core, MaxPairs>(list_op, std::span<value<Core> const>, pair_heap<Core, MaxPairs>*) -> foundation::result<value<Core>>`, the single home of pair-primitive semantics that every future evaluator delegates to (mirroring the Scheme `apply_prim`'s role). Because `smdlisp` has no elaborator yet (that's L10), `list_op` is a free-standing enum in `pairs.hpp` rather than reusing an `elaborator::prim_op` the way Scheme's `apply_prim` does; L10 (or whichever step wires a `core_prim`-equivalent node) should map its primitive-symbol table onto `list_op` directly.

CL deltas from the Scheme original, per the plan:
- `(car nil)` and `(cdr nil)` both return `nil` instead of erroring; `car`/`cdr` of any other non-pair value is still a diagnosed `parse_error`.
- `null`, `eq`, `eql`, and `atom` are predicates returning the canonical CL true value (the symbol `T`, i.e. `value<Core>{symbol{"T"}}`) or `nil` — not a raw `bool` — because `value<Core>` has no boolean alternative (decision D3).
- `atom` is new relative to the Scheme set: true for everything except a `pair_ref`, including `nil` (`(atom nil)` is `T`).
- `eq` and `eql` are implemented identically (see `docs/divergences/DIV-0002-eq-eql-conflated.md`, filed this step): both compare by `operator==`, which is value equality on every value alternative except `pair_ref` (heap-location identity). This mirrors the Scheme original's `eq?`/`eqv?` conflation and is currently unobservable since `smdlisp` has no numeric types wider than `int` (decision D10).

Merge-criteria coverage (`pairs.test.cpp`): `static_assert`-checked at compile time — `(car nil)` = `nil`, `(cdr nil)` = `nil`, `(null nil)` is true, `(atom nil)` is true, `(atom (cons 1 nil))` is false, `eq`/`eql` on symbols/integers/nil, plus a hand-built `'(1 2 3)` quoted-list structure (both via nested `cons` and via `list`) walked all the way to its `nil` tail. Runtime `TEST_CASE`s add error-path coverage (`car`/`cons` without a heap, `car` of a non-pair non-nil value, `eq` on pairs being identity not structure).

`make compile`, `make test` (316/316 passed, including 11 new `PairsTest` cases plus the `static_assert`s), and `make lint` all passed at the point of landing this step. `git diff -- src/smd/smdscheme` is empty; the `pairs.test.cpp` glob hit at `src/smd/smdscheme/pairs.test.cpp` in the build log is a pre-existing, unrelated frozen-tree file — not something this step touched.

The `MaxBindings == 16` hard-wiring noted in L7's handoff entry (`env<Core, 16> const *captured` inside `closure<Core>`) was left untouched this step: it lives in the `closure`/`env` coupling, not the pairs machinery, and the plan scopes "worker discretion" to when the closure code with that wart is actually copied — that is step L9 (`env.hpp` does not exist until then).

## 2026-07-18 Step L5: CL atoms landed

`src/smd/smdlisp/reader/{atom.hpp,atom.test.cpp}` now exist, added to the existing `smdlisp.reader` `FILE_SET` in `src/smd/smdlisp/reader/CMakeLists.txt` (one-line addition, no new CMake target — `atom.hpp` joins `cl_chars.hpp` in `smdlisp.reader`, which already links `PUBLIC smdscheme.parser`).

`smd::smdlisp::reader::atom` is `std::variant<atom_integer, atom_symbol, atom_keyword>` — three atom kinds, with `atom_keyword` a **distinct** kind from `atom_symbol` per D7 (not a naming convention on top of symbol). `atom_symbol`/`atom_keyword` each hold a `folded_name` (own `std::array<char, 64>` storage + length, not a `string_view` into source — folding to uppercase per D2 rewrites characters, so it cannot alias the source buffer). `folded_name::view()` returns a `string_view` into its own storage.

Public API: `integer_p()`, `symbol_p()`, `keyword_p()` (each a parser factory returning the respective atom-kind type, mirroring `smdscheme::reader`'s `integer_p()`/`symbol_p()` naming), plus `atom_p()` (tries integer, then keyword, then symbol, in that order — unconditional sequential fallback, not `operator|`) and the convenience entry point `read_atom(std::string_view) -> parser::parse_result<atom>` for callers with a whole spelling in hand (`read_atom("foo")` → symbol `FOO`; `read_atom(":bar")` → keyword `BAR`). `t` and `nil` are not special-cased anywhere in this file — `read_atom("t")`/`read_atom("nil")` just yield ordinary symbols `T`/`NIL`; their distinguished meaning is assigned downstream (value/elaborator layer, not yet built).

**Important correctness fact for L6 and any future atom-reading code (see `docs/divergences/DIV-0003-atom-maximal-munch.md`):** unlike the Scheme reader, integer recognition here is NOT "greedily consume a leading run of digit characters." Common Lisp symbols may start with a digit (e.g. the conventional symbol `1+`), so `atom.hpp` reads a whole maximal-munch token first (internal `detail::atom_token_p()`, a run of `is_constituent_char` characters) and classifies the *entire* token afterward: an integer only if the whole token is an optional `-` followed by one or more digits, a symbol otherwise. `integer_p`, `symbol_p`, and `keyword_p` all share this token scan. This was caught by a failing static_assert for `read_atom("1+")` during `make test` — the naive greedy-digit port (direct copy of the Scheme approach) misread it as integer `1` plus a stray unconsumed `+`. Any later step (L6 datum reader especially) should call into `atom_p()`/`read_atom()` rather than reimplementing digit scanning from scratch.

A lone `-` (no digits following) correctly reads as the symbol `-`, not a truncated integer.

`make compile`, `make test` (333/333 passed, including 37 new `AtomTest` cases), and `make lint` all passed at the point of landing this step. `git diff -- src/smd/smdscheme` was empty (frozen tree untouched); the only changes are the two new files under `src/smd/smdlisp/reader/` plus the one-line `FILES` addition in that directory's `CMakeLists.txt`.

## 2026-07-18 Step L9: Lisp-2 environment landed

`src/smd/smdlisp/closure/{env.hpp,env.test.cpp}` are new, added to the existing `smdlisp_closure_headers` FILE_SET in `src/smd/smdlisp/closure/CMakeLists.txt` (no new CMake target — `env.hpp` joins `value.hpp`/`pairs.hpp` under `smdlisp.closure`, and `env.test.cpp` is picked up automatically by the `smdlisp_test` target's `GLOB_RECURSE *test.cpp`).

**Env API** (`smd::smdlisp::closure::env<Core, MaxBindings>`), per decision D4 (Lisp-2: separate variable and function namespaces):

```cpp
template <typename Core, int MaxBindings>
class env {
  public:
    constexpr auto define_value(symbol name, value<Core> val) -> void;
    constexpr auto define_function(symbol name, value<Core> fn) -> void;
    [[nodiscard]] constexpr auto lookup_value(symbol name) const
        -> smd::smdscheme::foundation::result<value<Core>>;
    [[nodiscard]] constexpr auto lookup_function(symbol name) const
        -> smd::smdscheme::foundation::result<value<Core>>;
};
```

Two independent `foundation::static_vector<binding, MaxBindings>` lists (`values_`, `functions_`), each linear and most-recent-first (later `define_*` shadows an earlier one for the same name in *that* namespace only — `f` as a variable and `f` as a function coexist without interaction, verified by `static_assert` and `TEST_CASE("EnvTest - VariableAndFunctionCoexist")`), matching the Scheme `env`'s search order.
Unlike `smd::smdscheme::closure::env`, there is no backing `store` yet: every binding holds its value inline (the Scheme original's "functional" mode) because `setq`/`set_value` is explicitly out of scope for this step (arrives in L12) — nothing needs to be mutable yet.
There is also no parent-environment link, same as the Scheme original: a nested lexical scope is a *copy* of the enclosing `env` with more bindings appended, not a chain walked outward; `TEST_CASE("EnvTest - CopyingEnvCopiesBothNamespaces")` pins that a copy's extensions do not leak back into the original.
`lookup_value`/`lookup_function` return `foundation::result<value<Core>>`, erroring with `"unbound variable"` / `"undefined function"` respectively (distinct messages, matching ANSI CL's `UNBOUND-VARIABLE`/`UNDEFINED-FUNCTION` condition names) when the name has no binding in that namespace; the two namespaces fail independently of each other (`TEST_CASE`s `UnboundVariableIsError`, `UndefinedFunctionIsError`, `NamespacesAreIndependent`).

**Default environment / builtin dispatch story.** `default_env<Core, MaxBindings>()` installs `+`, `*`, `CONS`, `CAR`, `CDR`, `LIST`, `NULL`, `EQ`, `EQL`, `ATOM`, `FUNCALL`, `APPLY` into the *function* namespace only (never the variable namespace — pinned by `TEST_CASE("EnvTest - DefaultEnvInstallsBuiltinsInFunctionNamespace")`), spelled uppercase to match what the reader will produce after D2 case folding (`+`/`*` are unaffected, having no letters).
Per the plan's explicit invitation to reorganize builtin representation across `value.hpp`/`pairs.hpp`, `value.hpp`'s `builtin_op` enum is now the single tag type used for every function-namespace builtin: it grew from `{add, multiply}` to `{add, multiply, cons, car, cdr, list, null, eq, eql, atom, funcall, apply}`.
`pairs.hpp`'s `list_op` enum and `apply_prim` are **unchanged** — deliberately kept as a second, separate enum, because `pairs.hpp` includes `value.hpp` (for `value<Core>`), so `value.hpp` cannot name `pairs.hpp`'s `list_op` back without a header cycle.
The two enums share the same names for the eight list operations by construction, so the bridge from `builtin_op` to `list_op` is a same-name lookup, not a semantic mapping; whichever step first builds an evaluator that actually dispatches a looked-up `builtin` value (L10/L11) does that translation and is also where `funcall`/`apply` get real semantics — both are installed as builtin tags here but have no `apply_prim` case, since invoking a closure needs the evaluator, not value-level plumbing.

**Closure-capture ownership decision (the L7 handoff's open question).** `value.hpp`'s `closure<Core, MaxBindings = 16>` is now templated on `MaxBindings` (previously hard-wired to `env<Core, 16>`), fixing the wart the plan called out, with a default so `closure<Core>` alone still names a usable type.
`captured` **stays a non-owning raw pointer** (`env<Core, MaxBindings> const *`) — it does **not** become an owning `constexpr_box<env<Core, MaxBindings>>` deep copy the way `smd::smdscheme::closure::closure` does it.
This was a forced choice, not a style preference: `env.hpp` must include `value.hpp` (`env` stores `value<Core>`, needs `symbol`, `builtin_op`), so `value.hpp` cannot include `env.hpp` back without a header cycle, and therefore cannot embed a complete `env` by value inside `closure`/`value<Core>`.
`smdscheme` avoids this entirely by defining `constexpr_box`, `closure`, and `env` together in one file (`value.hpp`) rather than splitting `env` into its own header; the plan's step L9 spec explicitly asks for a separate `env.hpp`, so that escape hatch was not available here.
The documented alternative, recorded here per the step's instruction: ownership of the concrete `env` instances a real closure captures is deferred to whichever step first constructs them (L10's elaborator / L11's evaluator).
The natural fit, sketched in `value.hpp`'s updated `closure` doc comment, is an "env arena" in the same stable-index style already used by `pair_heap` (and by the Scheme `store`): a closure would capture a stable, arena-owned pointer, never a raw pointer onto a C++ call-stack local that could be outlived by the evaluator frame that created it.
This is an internal C++ ownership-technique decision, not a deviation from ANSI CL semantics or from the plan's stated API, so no divergence doc was filed for it (matching the plan's explicit "worker discretion, no divergence doc needed" framing for the adjacent `MaxBindings` parameterization).

`make lint`'s `pre-commit run -a` also reformatted two pre-existing L8 files this step touched only by proximity — `src/smd/smdlisp/closure/pairs.hpp` and `pairs.test.cpp` — via `clang-format`; those changes are whitespace/line-wrapping only (confirmed by rebuilding after the reformat: only `pairs.test.cpp` needed recompilation, and all 324 tests still passed), not semantic, and are included in this step's commit since they live inside this step's `closure/**` lane and were required to make `make lint` pass.

`make compile`, `make test` (324/324 passed, including 8 new `EnvTest` cases), and `make lint` all passed at the point of landing this step. `git diff --stat -- src/smd/smdscheme` is empty; the frozen tree was not touched.

## 2026-07-18 Step L6: CL datum reader landed (+ phase 16 draft)

`src/smd/smdlisp/reader/{datum_type.hpp,datum_type.test.cpp,read_datum.hpp,read_datum.test.cpp}` now exist, added to the existing `smdlisp.reader` `FILE_SET` in `src/smd/smdlisp/reader/CMakeLists.txt` (two-file addition to the existing `FILES` line, no new CMake target).

`smd::smdlisp::reader::datum_type<MaxNodes, MaxList>` is `smdscheme::foundation::fix<datum_f_factory<MaxNodes, MaxList>::type>`, the same arena/fix construction as `smdscheme::reader::datum_type`, over a six-alternative variant: `datum_integer`, `datum_symbol`, `datum_keyword`, `datum_list<R, MaxNodes, MaxList>`, `datum_quote<R, MaxNodes>`, `datum_function<R, MaxNodes>`. `datum_symbol`/`datum_keyword` each hold a `folded_name` (reused directly from `atom.hpp`, not re-derived) rather than a `string_view`, since the datum layer does not re-fold anything the atom reader already folded. `datum_keyword` is a distinct alternative from `datum_symbol` per D7, matching the atom layer. `datum_list` holds `arena_box` handles into the same arena and has no dotted-pair form — proper lists only, per decision D6, deferred until a step needs otherwise.

`datum_function<R, MaxNodes>` is new relative to the Scheme datum tree: it is the lowering target of `#'x` (sharpsign-quote). Per the plan, `#'x` does **not** desugar to a `(function x)` list at the reader layer — it gets its own dedicated node (`{ arena_box<R, MaxNodes> target; }`), the same treatment `'x` already gets via `datum_quote` instead of a synthesized `(quote x)` list. This is a reader-preserves-source-reality decision, not a semantics decision; what `#'x` *means* is left to a later elaborator step.

`read_datum<MaxNodes, MaxList>(cursor, tree_arena&) -> foundation::result<parser::parse_state<datum_type<MaxNodes, MaxList>>>` is the public entry point, structured exactly like `smdscheme::reader::read_datum`: `detail::read_datum_node` dispatches on the first non-intertoken-space character — `'` for quote, `#` (which must be followed by `'`, else a diagnosed error) for sharpsign-quote, `(` for a list (loop until `)`, erroring on unterminated input), `)` alone for an explicit "unexpected ')'" diagnostic, anything else falls through to `atom_p()`. Grammar implemented: `datum := atom | list | 'datum | #'datum` (no dotted pairs, D6). Intertoken space is skipped via L4's `skip_cl_intertoken_space`, not the Scheme `skip_intertoken_space`, so `;` comments work correctly inside lists, between arguments, anywhere a token boundary is expected — this was explicitly tested (`(1 ; a comment\n 2)` reads as a two-element list).

**Read before touching this file again:** atoms are classified exclusively by calling `atom_p()` from `atom.hpp` — this function does not reimplement any digit/symbol/keyword scanning itself, per L5's DIV-0003 guidance. The merge-criteria round trip `(defun f (x) (if x 1 2))` is tested directly by walking the arena and checking folded spellings (`DEFUN`, `F`, `X`, `IF`) and structure (4-element top list, 1-element param list, 4-element body list with two integers); this is the primary test in `read_datum.test.cpp`. Negative tests cover unterminated lists (top-level and nested), a stray leading `)`, `#` not followed by `'`, a lone `#` at end of input, and empty input. Quote and function-quote are tested both for simple targets (`'x`, `#'f`) and a compound target (`#'(lambda (x) x)`), including an explicit check that `#'f`'s node is `datum_function`, not `datum_list` — i.e. it is genuinely not lowered to `(function f)`.

Six new UUID anchor pairs were added (three in the new files, three retrofitted into the existing L4/L5 files in this same step, since all three are reader-lane files and adding a non-semantic comment-anchor pair to them is safe): `eb76ff97-b500-454c-8ad0-9b71c6db4598` (datum_type.hpp, `datum_symbol`/`datum_keyword` — the two new reader-level kinds), `d0fdf4c8-643f-44a6-8587-0f0c6d90d8c8` (datum_type.hpp, `datum_function`), `6bda5219-9ebb-4595-88ec-a863f20325f1` (read_datum.hpp, the quote/function-quote dispatch block), `b56c684b-835e-471d-b113-ab07741071eb` (cl_chars.hpp, `to_upper_char` — case folding), `53ab854d-6317-4432-a49d-1bf4712a29a1` (atom.hpp, `integer_p` — the maximal-munch/`1+` fix), `2bc98e28-002e-4db3-b4de-a8967e5d8cec` (cl_chars.hpp, `skip_line_comment`). All six are transcluded into `docs/blog/phase-16-reading-common-lisp.org` (new phase, `DRAFT`), which covers L4-L6 in one post: case folding at read time (D2), keywords as a distinct kind (D7), `;` comments as intertoken space, the DIV-0003 `1+` maximal-munch story, and `#'` as a reader-level node. `docs/blog/index.org` gained the Phase 16 entry.

**`docs/divergences/DIV-0004-orgit-transclusion-worktree-path.md` filed this step (status `open`, not `accepted-permanent`):** the six `#+transclude:` links in phase 16's `.org` point at `orgit:~/src/compile-time-scheme/wt-l6::...` (this step's own worktree), not `orgit:~/src/compile-time-scheme/main::...` like every prior phase's links. `org-transclusion-add-orgit` (`.emacs.d/init.el`) resolves the repo-dir component as a literal filesystem path, not a git-blob lookup, and `main` does not yet contain this step's new anchors — pointing at `main` would make `make blog-md` fail inside this worktree, which is a required pre-handoff check. **Action required after this branch merges to `main`:** re-point those six links from `wt-l6` to `main` (mechanical, no content change) before `wt-l6` is torn down as a worktree. The rendered `docs/blog/phase-16-reading-common-lisp.md` (GFM) committed this step is already correct and does not depend on the link fix. This same problem will recur for every later step that both adds code and has a blog deliverable (L11, L13, L15, L19, L21, L24); the orchestrator should decide whether to standardize on this pattern (worktree during development, `main` after merge) or something else.

**Cross-cutting note for the orchestrator, not part of L6's own change set:** `make lint`'s `clang-format` hook (pinned `v21.1.2` via pre-commit) reformats `src/smd/smdlisp/closure/pairs.hpp` and `pairs.test.cpp` in this environment even with zero content changes to either file (confirmed by reverting to the committed `main` content and re-running `pre-commit run clang-format --files ...`, which reformats them again, deterministically, every time). This is the same class of drift `main`'s commit `b011d52` ("Apply clang-format/gersemi formatting to merged smdlisp files") already fixed for `closure/value.{hpp,test.cpp}` after L9 landed — `pairs.{hpp,test.cpp}` appears to need the identical standalone formatting-only fix. L6 did not make this fix or touch `src/smd/smdlisp/closure/**` at all (out of lane, owned by L9's step and now merged to `main`); a scoped `pre-commit run --files` check confirms every file L6 actually changed is clang-format-clean and stable. Full `make lint` in this worktree therefore still reports one `Failed` (clang-format on the two pre-existing `pairs.*` files) that is unrelated to and predates this step.

`make compile` and `make test` (370/370 passed, including 6 new `DatumTypeTest` cases and 26 new `ReadDatumTest` cases) both passed clean at landing. `make lint` passes for every file L6 touched (verified via scoped `pre-commit run --files`); the only residual `make lint` failure across the whole tree is the pre-existing, out-of-lane `closure/pairs.*` drift described above. `make blog-md` renders `docs/blog/phase-16-reading-common-lisp.md` correctly (all six transclusions resolve) and leaves every other phase's `.md` unchanged (two incidental org-export id-churn diffs in `phase-12-cps.md` and `phase-2-front-end.md`, from re-running the whole `blog-md` target, were reverted before committing — `index.md`'s id churn was kept since its content genuinely changed for the new Phase 16 entry). `git diff -- src/smd/smdscheme` and `git diff -- src/smd/smdlisp/closure` are both empty (frozen tree and L9's lane both untouched).

## 2026-07-21 Step L18: backquote landed (Track C, ran parallel to L13)

`src/smd/smdlisp/reader/{cl_chars.hpp,datum_type.hpp,read_datum.hpp}` and their `.test.cpp` files, `src/smd/smdlisp/macroexpand/{expander.hpp,expander.test.cpp}`, and `src/smd/smdlisp/closure/{value.hpp,pairs.hpp,pairs.test.cpp,eval_direct.hpp,env.hpp}` all gained additions (all additive; no existing declaration was removed or changed shape except two test-only `static_assert`/count updates described below). No new files, no new CMake targets. `src/smd/smdlisp/elaborator/**` was **not** touched at all — this step's backquote lowering fully eliminates every `datum_backquote`/`datum_unquote`/`datum_unquote_splice` node during macro expansion, before anything reaches `elaborate()`, so the elaborator needed zero new cases. `git diff --stat -- src/smd/smdscheme` is empty (frozen tree untouched).

**Reader: three new datum kinds, mirroring L6's `datum_quote`/`datum_function` treatment of `'`/`#'` (`reader/datum_type.hpp`).** `datum_backquote<R,MaxNodes>{ arena_box templ; }`, `datum_unquote<R,MaxNodes>{ arena_box target; }`, `datum_unquote_splice<R,MaxNodes>{ arena_box target; }` — the datum tree's variant grew from 6 to 9 alternatives (both `datum_type.test.cpp`'s `variant_size_v` assertion and `expander.hpp`'s docs were updated to say so). `` ` `` (backquote), `,` (unquote), and `,@` (unquote-splice, a `,` immediately followed by `@`) are new terminating macro characters in `cl_chars.hpp::is_terminating_macro_char` (joining `( ) ' ;`), exactly like every other terminating macro character: they always end the current token, not merely at token start — this is what keeps `atom_token_p`'s maximal-munch symbol scan (L5, DIV-0003) from accidentally swallowing a comma or backquote into a symbol spelling. `read_datum.hpp`'s `read_datum_node` grew two new dispatch branches (placed after the existing quote/sharpsign-quote anchor block, not inside it, per the "do not nest UUID anchors" rule): `` ` `` reads the following datum and wraps it in `datum_backquote`; `,` peeks for a following `@` to decide `datum_unquote_splice` vs. plain `datum_unquote`, then reads the following datum and wraps it. The reader does **not** track backquote nesting depth or reject a `,`/`,@` that appears with no enclosing backquote — it just records the syntax faithfully (same "preserve source reality, let downstream passes decide meaning" philosophy as L6's `#'`); rejecting a stray comma is macroexpand's job (below).

**Macroexpand: backquote lowers to `CONS`/`APPEND`/`QUOTE` call forms, not a new evaluator-level construct (`macroexpand/expander.hpp`).** New function `expand_backquote_template<MaxNodes,MaxList>(tmpl, arena) -> result<datum>` (plus a `detail::expand_backquote_element` helper it mutually recurses with, and `detail::make_quote_form`, mirroring `expand_backquote_template`'s forward-declare-then-define pattern on `elaborate.hpp`'s `elaborate_node`/`elaborate_lambda` mutual recursion): `` `(a ,x ,@ys) `` lowers to `(CONS (QUOTE A) (CONS X (APPEND YS NIL)))`. Rules, matching ANSI CL's classic naive backquote expansion:
- `` `,x `` (an unquote as the template's own top level) is just `x` — no cons/quote wrapping.
- `` `,@x `` at the template's own top level is a diagnosed `parse_error` (`"backquote: ,@ is not valid outside a list"`) — splicing only makes sense as a list element.
- A non-list template (an atom, or a nested `datum_quote`/`datum_function`/`datum_backquote`) is literal data: wrapped whole in `(QUOTE tmpl)`. **This means a nested backquote is never re-expanded at its own comma level — see DIV-0007.**
- A list template folds its elements right-to-left into a `CONS`/`APPEND` chain bottoming out in the bare `NIL` symbol (unquoted, self-evaluating per D3 — exactly how the L17 host macros already emit `NIL`/`T` directly): an ordinary element becomes `(CONS <element-expansion> <rest>)`; a `,@x` element becomes `(APPEND x <rest>)`.

**`expand()`'s whole-tree walk needed extending, exactly as the briefing predicted — a finer-grained analogue of its existing `QUOTE` special case.** A `datum_backquote` node is lowered by `expand_backquote_template` first, and then **the lowered result is walked by `expand()` itself** (a recursive call), not left alone: this is what makes a macro call sitting in an unquoted position (e.g. `` `(a ,(when t 1)) ``) still get expanded (proven by `ExpanderTest - ExpandLowersBackquoteAndRecursesIntoUnquotedCode`), and it is also what makes the lowered form's synthesized `(QUOTE A)` sub-forms automatically hit the pre-existing "never walk into QUOTE's argument" rule — no separate "don't walk into the literal parts of a backquote template" logic was needed, because lowering turns every literal part into an ordinary `QUOTE` form the existing rule already protects. A bare `datum_unquote`/`datum_unquote_splice` reaching `expand()` with no enclosing backquote (e.g. `(a ,x)` at ordinary code position) is a diagnosed error (`"backquote: , or ,@ used outside a backquote template"`), added so this common mistake gets a clear message instead of falling through to the elaborator's generic `"elaborator: unsupported node type"`.

**`append` joins the builtin set (closure/, additive only — L13's lane was not touched beyond this).** New `builtin_op::append` (`value.hpp`), new `list_op::append` (`pairs.hpp`) with a recursive `detail::append_impl` helper (copies the first argument's spine into fresh cons cells ending in the second argument — ordinary CL `append` semantics, never mutates or shares the input's cells), a new `to_list_op` bridge case and a new `case builtin_op::append` arm in the shared dispatch switch (`eval_direct.hpp`), and an `"APPEND"` entry in `env.hpp`'s `install_default_builtins` (installed into the FUNCTION namespace, alongside `cons`/`car`/`cdr`/`list`/etc.). **Per DIV-0007, `append` is two-argument only** (`(append list1 list2)`), not ANSI CL's variadic form — every actual use (this step's `,@` lowering, itself always exactly "one evaluated list, spliced onto exactly one rest-of-chain value") only ever needs two arguments, so the narrower shape was not widened speculatively.

**Merge criteria, tested at both levels the plan asks for.** Datum-level shape test (`ExpanderTest - BackquoteTemplateLowersToConsAppendQuote`): calls `expand_backquote_template` directly on `` `(a ,x ,@ys) ``'s template and walks the resulting `(CONS (QUOTE A) (CONS X (APPEND YS NIL)))` structure node-by-node. Value-level end-to-end test (`ExpanderTest - BackquoteAtomUnquoteAndSpliceEndToEnd`, plus a `static_assert` twin): `(let ((x 5) (ys (list 6 7))) \`(a ,x ,@ys))` evaluates through `expand_and_elaborate` + `eval_direct` to the cons chain `(A 5 6 7)`, walked via the shared `pair_heap` exactly like `pairs.test.cpp`'s existing list-walking style. Additional coverage: reader-level shape tests for `` `x ``/`,x`/`,@xs` individually and together in one list (`read_datum.test.cpp`); `expand_backquote_template` edge cases (`` `x `` ≡ `(QUOTE X)`, `` `,x `` ≡ `X`, `` `,@x `` at top level is an error); the whole-tree recursion-into-unquoted-macro-call test and the stray-comma error test described above; and `append`-specific tests in `pairs.test.cpp` (copies rather than mutates its first argument, wrong-arity error, no-heap error, non-list-first-argument error).

`make compile`, `make test` (510/510 passed, including 24 new `ReadDatumTest`/`DatumTypeTest`/`PairsTest`/`ExpanderTest` cases plus new `static_assert`s), and `make lint` all passed clean at landing (clang-format reformatted the newly-authored test files in place during authoring, as is normal for brand-new content — no pre-existing file needed a standalone formatting fix this step). `git diff --stat -- src/smd/smdscheme` and `git diff --stat -- src/smd/smdlisp/elaborator` are both empty (frozen tree and the elaborator lane, untouched by design, both clean).

**Divergence doc filed:** `docs/divergences/DIV-0007-backquote-nested-and-append-two-arg.md` (status `accepted-permanent`) — (1) nested backquote (a `` ` `` template appearing inside another backquote template) is treated as opaque literal data rather than correctly tracking comma-nesting depth per ANSI CL; it either round-trips harmlessly or fails loudly at elaboration (`"quote: unsupported datum"`), never silently mis-evaluates. (2) `append` is two-argument only, not ANSI CL's variadic form. Neither is exercised by any test through this step or L19's stated scope. No blog deliverable for this step (phase 20 arrives at L19, `defmacro`, per the plan's phase table).

## 2026-07-19 Step L10: CL core model and baseline elaborator landed

`src/smd/smdlisp/elaborator/{elaborated_core.hpp,elaborated_core.test.cpp,elaborate.hpp,elaborate.test.cpp,CMakeLists.txt}` are new, wired into `src/smd/smdlisp/CMakeLists.txt` via `add_subdirectory(elaborator)` plus a link edit (`target_link_libraries(smdlisp.smdlisp PUBLIC ... smdlisp.elaborator)`). New CMake target `smdlisp.elaborator` (STATIC, header-only, mirrors `smdlisp.reader`/`smdlisp.closure`'s pattern), linking `PUBLIC smdscheme.foundation smdlisp.reader`. Unlike `smdscheme.elaborator`, this target does **not** set `target_compile_options(... INTERFACE -freflection)`: no existing `smdlisp.*` target sets that flag (checked — `smdlisp.reader`/`smdlisp.closure` don't either), so this follows the established `smdlisp` convention rather than the `smdscheme` one.

**Core-kind list** (`smd::smdlisp::elaborator`, all in `elaborated_core.hpp`, one `std::variant` layer via `core_f_factory`): `core_integer`, `core_symbol` (VARIABLE-namespace reference), `core_keyword` (self-evaluating), `core_nil`, `core_true`, `core_quote` (atom-only: `variant<int, core_symbol, core_keyword>`), `core_cons<R,MaxNodes>` (hermetic pair constructor, quote-only), `core_if<R,MaxNodes>`, `core_progn<R,MaxNodes,MaxList>`, `core_lambda<R,MaxNodes,MaxList>`, `core_function<R,MaxNodes>`, `core_application<R,MaxNodes,MaxList>`. Twelve kinds total. `elaborated_core.hpp` includes `smd/smdlisp/reader/atom.hpp` plus the three `smdscheme::foundation` headers it actually uses (`arena_box.hpp`, `fix.hpp`, `static_vector.hpp`) directly, rather than piggybacking on `reader/datum_type.hpp` the way `smdscheme`'s equivalent file does (a header-hygiene tightening, not a functional change — `elaborated_core.hpp` never references `reader::datum_type` itself, only `reader::folded_name` from `atom.hpp`, see below).

**NIL/T elaboration (decision D3), implemented exactly as the plan's "likely" sketch:** a bare `NIL` symbol elaborates to `core_nil{}` (self-evaluating, never a VARIABLE-namespace lookup); a bare `T` symbol elaborates to `core_true{}`. Both are distinct core kinds from `core_symbol`, checked by comparing the folded spelling (`sym.name.view() == "NIL"` / `"T"`) in exactly two places — `detail::elaborate_node`'s symbol-datum dispatch (ordinary, unquoted position) and `detail::elaborate_quoted_datum`'s symbol-atom case (quoted position) — both intercept `NIL`/`T` *before* falling through to `core_symbol`/`core_quote{core_symbol{...}}`, so `'nil` and `nil` elaborate identically (`core_nil{}`), matching that quoting a self-evaluating constant is a no-op in ANSI CL. `T`'s *runtime* representation (e.g. whether the evaluator ultimately materializes it as `closure::symbol{"T"}`, matching `pairs.hpp`'s existing "canonical true is the symbol T" choice) is deliberately left to L11 — `core_true` only records that the source spelled the canonical true constant, per the plan's "follow what the value model supports" instruction.

**Application-head / function-position representation (decision D4).** One core kind, `core_function<R,MaxNodes>{ std::variant<std::string_view, arena_box<R,MaxNodes>> target; }`, covers every spelling ANSI CL allows in function position: a bare symbol (`target` holds the name, resolved in the FUNCTION namespace at evaluation time — never the VARIABLE namespace `core_symbol` denotes) or an embedded `(lambda ...)` expression (`target` holds a handle to the `core_lambda` node). `detail::elaborate_function_position` is the single function that produces this node, parameterized by a `char const *error_message` so callers get a diagnostic appropriate to their context; it is called from three places: the `function`/`#'` special-form/reader-node case, `core_application.func` in the fallback "ordinary application" path of `elaborate_list`, and (transitively, for `(let ...)`/`(let* ...)`) the synthesized lambda-application nodes those desugarings build. `core_application.func` is therefore *always* an `arena_box` to a `core_function` node — never a bare `core_symbol` or any other expression kind — which is exactly the invariant `elaborated_core.test.cpp`'s `ApplicationFuncIsAlwaysArenaBox` static_assert pins. `funcall`/`apply` are deliberately **not** special-cased anywhere in the elaborator: `(funcall f x)` elaborates as an ordinary application with head symbol `FUNCALL`, resolved from the default environment's FUNCTION namespace at evaluation time exactly like any other builtin (L9 already installed both there) — this is tested (`FuncallAndApplyAreOrdinaryApplications`).

**Quoted list data / hermetic `cons` (decision: `core_cons`, not a ported `core_prim`).** `detail::elaborate_quoted_datum` lowers a quoted list to nested `core_cons<R,MaxNodes>{ arena_box car; arena_box cdr; }` cells bottoming in `core_nil{}`, mirroring the Scheme elaborator's `elaborate_quoted_datum` construction strategy exactly (same recursive right-fold shape). Unlike the Scheme elaborator, this is **not** the Scheme `core_prim`/`prim_op` node ported wholesale (which dispatches ~11 pair/list primitives hermetically): step L9 already committed `cons`/`car`/`cdr`/`list`/`null`/`eq`/`eql`/`atom` to the FUNCTION namespace as ordinary `builtin` values (`closure::builtin_op`), so ordinary application through `core_function` is how a compiled program calls those — introducing a second, elaborator-level hermetic dispatch for the same eight operations would duplicate that mechanism. Only the constructor used *internally* to build quoted list literals needs to be exempt from FUNCTION-namespace lookup (ANSI CL: `'(1 2)` must construct the same structure regardless of any later redefinition of `cons`, once `defun` exists in L12) — `core_cons` is exactly that, scoped down to the one operation that actually needs hermeticity at this layer. No divergence doc was filed for this (matches the plan's own "worker discretion" framing for adjacent structural choices in L8/L9's handoff entries): it is a C++/architecture implementation choice consistent with L9's already-landed design, not an ANSI CL semantics deviation or a plan-contradicting step. A quoted nested quote/sharpsign-quote datum (`''x`, `'#'x`) builds the two-element list `(QUOTE x)` / `(FUNCTION x)` via the same `core_cons` mechanism, mirroring the Scheme elaborator's nested-quote handling; the `QUOTE`/`FUNCTION` head symbols are built via a small `literal_symbol` helper that folds the literal through `reader::detail::fold` rather than hand-filling `folded_name`'s storage array.

**A real memory-safety bug was found and fixed during this step, not merely a test artifact.** `smd::smdlisp::reader::read_datum`'s *root* return value is not itself arena-allocated (only a datum's *children* — list elements, quote/sharpsign-quote targets — go through `make_arena_box`; this has been true since L6 and mirrors the Scheme reader's same convention). The Scheme elaborator's `core_symbol{ std::string_view name; }` is safe regardless, because Scheme's `datum_symbol.name` is a view into the *original source text* (stable, caller-owned). `smdlisp`'s `datum_symbol`/`datum_keyword` cannot do that — case folding (D2) rewrites characters, so `reader::folded_name` (L5) *owns* its spelling in an inline `std::array<char,64>`. Combining these two facts: elaborating a **bare top-level atom program** (the entire input is just a symbol or keyword, e.g. `elab("foo")`) with a first-draft `core_symbol{ std::string_view name; }` (mirroring Scheme exactly, `name = datum.name.view()`) produced a `std::string_view` into a `folded_name` that lives in the *caller's* `read_datum(...)` result, which is typically a local temporary — a stack-use-after-return the moment that temporary goes out of scope. Caught immediately by AddressSanitizer (`make test` failed two tests with `SUMMARY: AddressSanitizer: stack-use-after-return`) before this was ever committed. **Fix:** `core_symbol`/`core_keyword` hold an owned `reader::folded_name` (copied, not viewed) instead of a `std::string_view`; every construction site now copies the source datum's `folded_name` by value rather than taking `.view()` and storing the view. `folded_name` (`array<char,64>` + `int`) remains trivially destructible, so this does not violate the "core nodes are trivially destructible" architecture rule. `core_function.target`'s `std::string_view` alternative and `core_lambda.params`' `string_view`s were **not** changed — audited and confirmed safe, because every construction site for those specifically fetches through `datum_arena.get(...)` (list elements, quote/sharpsign-quote targets), which is always backed by the caller-owned, longer-lived arena, never the reader's un-arena-backed root return value. No divergence doc filed (internal C++ lifetime-safety fix, not an ANSI CL or plan deviation) — durable fact worth remembering for **L11 and beyond**: any future core-node field storing a name/spelling must go through this same "owned copy if it might trace back to a root datum, view is fine if it's provably arena-backed" reasoning; when in doubt, copy the `folded_name`.

**Error positions.** Elaborator-level (malformed-form) errors use `foundation::parse_error{{}, "message"}` — a default-constructed `source_pos` — exactly matching the Scheme elaborator's own `elaborate_list`/`elaborate_quoted_datum`, which do the same thing (verified by reading `src/smd/smdscheme/elaborator/elaborate.hpp`; it never computes a real position either). This is not a shortcut particular to `smdlisp`: the datum tree (both Scheme's and `smdlisp`'s) does not thread `source_pos` through its nodes at all, so there is no position to recover once a datum has already been read — real positions exist only for *reader*-level syntax errors (`read_datum`'s errors carry `cur.position()`). `elaborate.test.cpp` pins this distinction directly: `ElaboratorLevelErrorsUseDefaultPosition` checks `er.error().where == source_pos{}` for a malformed `(if 1)`, and `ReaderLevelErrorsCarryRealPositions` checks `er.error().where != source_pos{}` for an unterminated `(if 1 2`. Any later step wanting real positions on elaborator errors would need to thread `source_pos` through the datum tree itself first (a reader-layer change) — out of scope here, and not attempted.

**Special operators recognized (folded-spelling comparison, per D2):** `QUOTE`, `IF`, `PROGN`, `LET`, `LET*`, `LAMBDA`, `FUNCTION` — exactly the plan's list, checked in `detail::elaborate_list` by comparing the head symbol's folded spelling. `if` with 2 args (no alternative) synthesizes an implicit `core_nil{}` alternative (3-branch `core_if` always). `lambda`/`let`/`let*` bodies are **expression sequences** (`core_lambda.body` is a `static_vector<arena_box,...>`, not a single expression like the Scheme original) — implicit progn is native to the node shape, not a separate wrapping `core_progn`, except where a form needs to synthesize a progn without a lambda around it (a zero-binding `let*`, i.e. `(let* () a b)` → `core_progn{[a,b]}`). `lambda`/`progn`/`let`/`let*` all require **at least one** body expression (a zero-form body, though legal in full ANSI CL, is out of scope for this baseline elaborator — matches the Scheme original's same restriction on `lambda`/`begin`; no divergence doc filed, treated as the same class of scope cut as D10). `let`/`let*` desugar to lambda application exactly as the plan specifies, generalized for multi-form bodies; `let*`'s desugaring nests one lambda-application layer per binding, innermost-first, exactly as the Scheme original's `let*` does.

`make compile`, `make test` (421/421 passed, including 4 new `ElaboratedCoreTest` cases and 34 new `ElaborateTest` cases), and `make lint` all passed clean at landing — `make lint`'s `clang-format` hook reformatted the three new elaborator files in place during authoring (normal for brand-new files, not the `closure/pairs.*` drift L6/L9 flagged; that drift is already fixed on `main` as of commit `558427f`, confirmed by this step's `make lint` running fully clean with zero reformatting of any pre-existing file). `git diff --stat -- src/smd/smdscheme` and `git diff --stat -- src/smd/smdlisp/reader src/smd/smdlisp/closure` are both empty (frozen tree and the reader/closure lanes both untouched; only `src/smd/smdlisp/elaborator/` (new) and one link-wiring edit to `src/smd/smdlisp/CMakeLists.txt` changed). No blog deliverable for this step (phase 17 arrives at L11 per the plan's phase table). No divergence doc filed — every deviation from the Scheme original discussed above is a structural/C++ implementation adaptation already implied or invited by L9's landed choices, not an ANSI CL semantics cut or a plan-contradicting step; next free divergence number remains **DIV-0005**.

## 2026-07-20 Step L11: direct evaluator landed (+ phase 17 draft)

`src/smd/smdlisp/closure/{eval_direct.hpp,eval_direct.test.cpp}` are new. `src/smd/smdlisp/closure/env.hpp` and `value.hpp` gained additions (below); no existing declaration in either file was removed or changed shape, so L9's own tests (`env.test.cpp`) needed no changes and still pass unmodified. `src/smd/smdlisp/closure/CMakeLists.txt` gained `eval_direct.hpp` to the `smdlisp_closure_headers` FILE_SET and now links `PUBLIC smdlisp.elaborator smd.fixpoint` (previously only `smdscheme.foundation`) — `eval_direct.hpp` is the first `closure/` file that needs the elaborated core (`elaborator::core_type`) and `smd::fixpoint::overloaded`. Because `smdlisp.closure` now depends on `smdlisp.elaborator`, `src/smd/smdlisp/CMakeLists.txt`'s `add_subdirectory` order was swapped (`elaborator` before `closure`) to match `smdscheme`'s own ordering, avoiding reliance on CMake's out-of-order target resolution across sibling `add_subdirectory` calls even though testing showed it isn't strictly required.

**Evaluator API.** `smd::smdlisp::closure::eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(node, arena, environment, envs) -> foundation::result<value<Core>>` is a structural-recursive interpreter over all twelve `elaborated_core.hpp` node kinds, adapted from `smd::smdscheme::eval_direct`'s architecture. `MaxBindings` must be `16` (`static_assert`ed, matching the Scheme original's identical constraint) because `value<Core>`'s `closure<Core>` alternative is still hard-wired to `MaxBindings == 16` in the `value.hpp` alias, even though `closure<Core, MaxBindings>` itself was parameterized in L9 — full parameterization of `value<Core>` is deferred again, not this step's job (same wart, still open). `MaxEnvs` is new: the capacity of the `env_arena` (below) that owns every environment a `lambda`/`function` form captures during one evaluation. A second entry point, `apply_function_value<MaxNodes, MaxList, MaxBindings, MaxEnvs>(func_val, args, arena, heap, envs) -> foundation::result<value<Core>>`, applies an *already-evaluated* function value to already-evaluated arguments; `core_application` evaluates its function and all its arguments and then calls this, and `funcall`/`apply` (see below) call it directly, so there is exactly one call-dispatch implementation for every calling convention in the language, not three.

**Lisp-2 lookup (D4), fully wired for the first time.** `core_symbol` (an ordinary expression symbol) always calls `environment.lookup_value`; `core_function` (an application head, `#'name`, or `(function name)`) always calls `environment.lookup_function` — or, for an embedded `(lambda ...)`, recurses into `eval_direct` on the embedded lambda node directly, no lookup at all. Neither path ever falls back to the other. `#'`/`function`/an application head all funnel through the same `core_function` case because the elaborator (L10) already collapsed all three spellings into one core node; the evaluator only had to get Lisp-2 lookup right in one place as a result.

**`nil`/`t` truthiness (D3).** `is_true(value<Core> const&) -> bool` (`value.hpp`, unchanged from L7 — `!holds_alternative<nil_t>(v)`) is the only truthiness check anywhere in the evaluator; `core_if` is currently the only consumer, and it calls `is_true` rather than re-encoding the check. **Decision made this step, per the plan's explicit invitation to make the call:** `core_true` evaluates to `value<Core>{symbol{"T"}}` — the same canonical-true value `pairs.hpp::apply_prim`'s predicates (`null`/`eq`/`eql`/`atom`) already return — not a second true representation. `core_nil` evaluates to `value<Core>{nil_t{}}`. `core_keyword` evaluates to a `keyword` value (self-evaluating, D7); `core_quote`'s atom variant (`int`/`core_symbol`/`core_keyword`) evaluates each alternative to the matching `value<Core>` kind.

**Implicit progn.** `core_progn` and `core_lambda`'s body (already a `static_vector<arena_box,...>` sequence per L10's core-node shape, not a single expression) both evaluate every expression in order and return the last; no separate wrapping node is needed for a lambda body specifically, since `core_lambda.body` already has sequence shape.

**Builtin bridge (`builtin_op` → `pairs.hpp::list_op`).** `detail::to_list_op(builtin_op) -> list_op` (`eval_direct.hpp`, anonymous-namespace-free `detail` sub-namespace) is a same-name switch covering the eight shared list-op names (`cons`/`car`/`cdr`/`list`/`null`/`eq`/`eql`/`atom`); `apply_function_value`'s builtin dispatch calls it and then `pairs.hpp::apply_prim` for those eight ops, using `environment.pairs()` (below) as the shared heap. `add`/`multiply` are **variadic**, not the Scheme original's fixed 2-arg form: `(+ a b c ...)` folds left starting from `0` (`+`) or `1` (`*`), matching ANSI CL's actual `+`/`*` signature (`(+)` ⇒ `0`, `(*)` ⇒ `1`) rather than porting the Scheme restriction forward — this is *less* of an ANSI divergence than the alternative, so no divergence doc was filed; anyone reading the Scheme original for reference should not assume the 2-arg restriction carried over.

**`funcall`/`apply` real semantics (D4).** Both are ordinary FUNCTION-namespace builtins at the elaborator level (L10 already decided this — `(funcall f x)` elaborates as a plain application with head `FUNCALL`), but `apply_function_value`'s builtin case gives them call semantics instead of an `apply_prim` case: `funcall`'s first evaluated argument *is* the function to call, the rest are its arguments, and it recurses into `apply_function_value` itself. `apply`'s last evaluated argument must be a proper list (walked via `pair_ref`/`nil_t`, using the shared heap); every argument between the function and the last is passed through unchanged; the walked elements are appended and the combined argument list is passed to a recursive `apply_function_value` call. `(apply #'+ 1 (list 2 3))` ⇒ `6`, `(apply #'+ (list 1 2))` ⇒ `3`, both covered by `eval_direct.test.cpp`.

**Closure-capture ownership, resolved (the question L9's handoff explicitly deferred to this step).** `closure<Core>::captured` (`value.hpp`) is unchanged — still a non-owning `env<Core, MaxBindings> const*`, per L9's forced choice (header cycle between `value.hpp` and `env.hpp`). What's new is **where that pointer points**: `env.hpp` gained `env_arena<Core, MaxBindings, MaxEnvs = default_max_envs>` (`default_max_envs = 128`), a `static_vector<env<Core, MaxBindings>, MaxEnvs>`-backed arena in exactly the stable-index style `pair_heap` already uses — `alloc(env<Core,MaxBindings> e) -> env<Core, MaxBindings> const*` copies `e` in and returns a pointer that stays valid for the arena's whole lifetime, because `static_vector`'s backing storage is a fixed `std::array` that never reallocates. `eval_direct`'s `core_lambda` case calls `envs.alloc(environment)` and stores the returned pointer as the new closure's `captured`. The caller creates one `env_arena` (alongside the core `tree_arena` and `pair_heap`) and threads it through the whole evaluation by mutable reference — the same discipline already used for the core arena and the pair heap, extended to environments. This sidesteps `smd::smdscheme::closure::env`'s alternative fix (a `constexpr_box<env<Core,16>>` owning box backed by `new`/`delete`, relying on C++20's transient-constant-evaluation-allocation rule that the allocation must be freed before the enclosing constant evaluation finishes) entirely — no `new`/`delete` anywhere in this path, because nothing is heap-allocated; everything lives in caller-owned, fixed-capacity storage. The Scheme technique only works because `smdscheme::closure::closure` and `smdscheme::closure::env` are defined together in one header where `env` is complete at every point `constexpr_box` needs it; `smdlisp`'s split into `value.hpp`/`env.hpp` (required by the Lisp-2 design, L9) ruled that out, which is what forced this step to actually build the alternative rather than reach for the Scheme pattern unchanged.

**`env.hpp` also gained pair-heap threading**, mirroring `smd::smdscheme::closure::env`'s `pairs_`/`pairs()`: a new `env(pair_heap<Core, default_max_pairs> *p)` constructor and `pairs() const -> pair_heap<Core, default_max_pairs>*` accessor (both additive; the existing default constructor and all L9 call sites are unaffected), plus a `default_env<Core, MaxBindings>(pair_heap<Core, default_max_pairs> &p) -> env<Core, MaxBindings>` overload alongside the existing no-heap `default_env<Core, MaxBindings>()`. `core_cons` (quoted-list construction) and the `cons`/`car`/`cdr`/`list` builtins both allocate/dereference through `environment.pairs()`; without this, there was nowhere to plug a shared heap into an `env` at all. `eval_direct.test.cpp` always constructs its environment via `default_env<Core, MaxBindings>(heap)` with a caller-owned `pair_heap`.

**A second stack-use-after-return bug, one layer above L10's.** L10 fixed `core_symbol`/`core_keyword` to own their `folded_name` instead of viewing into a possibly-non-arena-backed root datum. This step hit the identical bug one layer up: `closure::symbol`/`closure::keyword` (`value.hpp`, unchanged since L7) hold a bare `std::string_view name`, and `eval_direct`'s `core_keyword`/`core_symbol`(quoted)/`core_true` cases build these values via `.view()` on a core node's `folded_name`. That view is safe exactly as long as the *evaluated root* (not just the arena) outlives the resulting `value<Core>` — a fact this step's first draft of the `eval_direct.test.cpp` helper `run()` violated, computing the elaborated root as a function-local and returning `eval_direct`'s result *out of* that function; `EvalDirectTest - KeywordSelfEvaluates` (evaluating a bare top-level `:foo`, with nothing arena-backing the sole `core_keyword` node) failed under AddressSanitizer with `stack-use-after-return`, caught before commit, the same way L10's bug was caught. **Fix applied in the test only, not in `value.hpp`:** `run()` now takes an additional caller-owned `Core &root` parameter and stores the elaborated root into it before evaluating, exactly mirroring `elab()`'s own caller-owns-the-arena discipline extended one hop further (the elaborated root must be caller-owned and must outlive use of the result, precisely like `core_arena` already must). **This is a durable fact for every later step that calls `eval_direct` directly** (not through a test helper that returns past its own locals): the caller must keep the exact `Core` value/reference passed as `node` alive for as long as the resulting `value<Core>` might be inspected, because `symbol`/`keyword` results can view into it. `value<Core>`'s `symbol`/`keyword` are **not** self-contained the way `core_symbol`/`core_keyword` now are; giving them owned spellings (the `folded_name` treatment) was considered and deliberately deferred — it would touch a type used pervasively since L7 across every prior step's tests, is a wider structural change than this step's scope, and the caller-owns-the-root discipline is sufficient and already the established pattern. A later step (plausibly whichever one first needs to store/return a `value<Core>` symbol/keyword result across a genuine ownership boundary, e.g. the CPS backend in L13) should revisit whether `symbol`/`keyword` need to own their spelling the way `core_symbol`/`core_keyword` do.

**Merge criteria**, all three as compile-time `static_assert`s in `eval_direct.test.cpp`: `(if nil 1 2)` ⇒ `2`; `((lambda (x) (car (cdr x))) '(1 2 3))` ⇒ `2`; `(funcall #'cons 1 nil)` ⇒ a `pair_ref` cell with `car = 1`, `cdr = nil`. Plus 18 runtime `TEST_CASE`s: keyword self-evaluation, `nil`/`0`/`t` truthiness, `let`/`let*` (both already pure elaborator desugarings — the evaluator needed no `let`-specific code at all), implicit progn, closures capturing both namespaces (one test proves variable capture, a separate test proves a nested lambda still reaches a FUNCTION-namespace builtin through its captured environment), `apply` spreading a final list argument (with and without extra fixed arguments), distinct `"unbound variable"`/`"undefined function"` errors (content-checked via `std::string_view` comparison, not pointer equality — `parse_error::message` is a bare `char const*`), calling a non-function, arity mismatch, and `funcall` of both a named function and a bare lambda expression.

`make compile`, `make test` (439/439 passed, including 18 new `EvalDirectTest` cases plus the 3 merge-criteria `static_assert`s), and `make lint` all passed clean at landing. `git diff --stat -- src/smd/smdscheme` is empty (frozen tree untouched). No divergence doc filed: the closure-capture-ownership design and the pair-heap-in-env extension are internal C++ architecture decisions the plan explicitly invited this step to make (matching L9's own precedent for the adjacent `MaxBindings`-parameterization decision), and making `+`/`*` variadic is a reduction in ANSI divergence, not an introduction of one; next free divergence number remains **DIV-0005**.

Blog: `docs/blog/phase-17-nil-t-lisp2.org` drafted (`DRAFT — pending author revision`), covering the four points above (one `is_true` function, Lisp-2 lookup with no cross-namespace fallback, `funcall`/`#'` real call semantics, the closure-capture-ownership resolution) via five new UUID anchors: `82728208-0712-4e1a-9252-036e99276919` (`value.hpp`, `is_true`), `a73f8d39-7455-4ddc-a541-7424ab1d3a35` (`env.hpp`, `env_arena::alloc`), `6332b396-3733-4eb0-adc5-af7a57adb809` (`eval_direct.hpp`, `core_if`'s `is_true` call site), `bd5cf19a-ccb0-45a1-b208-b76970bcb0c6` (`eval_direct.hpp`, the `core_symbol`/`core_function` Lisp-2 dispatch), `1d30e953-16e2-4812-9eee-10934eafdec3` (`eval_direct.hpp`, the `builtin` dispatch including `funcall`/`apply`). `docs/blog/index.org` gained the Phase 17 entry. Per DIV-0004's now-standing convention, the six `#+transclude:` links point at `orgit:~/src/compile-time-scheme/wt-l11::...` (this step's own worktree); the orchestrator repoints them to `main` after merge. `make blog-md` renders `docs/blog/phase-17-nil-t-lisp2.md` cleanly (all five transclusions resolve); two org-markup bugs of my own making were caught and fixed while drafting — adjacent `~verbatim~/~verbatim~` spans without an intervening space merge into one giant verbatim span in GFM export (org's closing-`~` rule requires the following character to be whitespace/punctuation, not another `~` used to open the next span), and a `~word~` immediately followed by a bare letter (`~static_assert~s`) has the same problem, since the letter right after the closing `~` isn't a valid post-match boundary character either. `docs/blog/phase-1-foundation.md`, `phase-5-fixpoint-trees.md`, and `phase-8-senders.md` were incidentally re-rendered by the first `make blog-md` run in this worktree (their transclusions had gone stale/blank in the committed `.md`, unrelated to this step) and were reverted with `git checkout` before committing, per the "existing phases' output unchanged" requirement; `index.md`'s id churn was kept since its content genuinely changed for the new Phase 17 entry, matching L6's precedent.

## 2026-07-20 Step L17: macro expander with host macros landed (Track C, ran parallel to L12)

`src/smd/smdlisp/macroexpand/{expander.hpp,expander.test.cpp,CMakeLists.txt}` are new, wired into
`src/smd/smdlisp/CMakeLists.txt` via `add_subdirectory(macroexpand)` (appended after `closure`) plus a link
edit adding `smdlisp.macroexpand` to `smdlisp.smdlisp`'s `target_link_libraries`. New CMake target
`smdlisp.macroexpand` (STATIC, header-only, mirrors `smdlisp.reader`/`smdlisp.elaborator`'s pattern), linking
`PUBLIC smdscheme.foundation smdlisp.reader smdlisp.elaborator` — it needs `smdlisp.elaborator` (unlike
`reader`/`closure`) because `expander.hpp` directly includes `elaborate.hpp` for the pipeline-wiring entry
point (below). This step ran concurrently with L12 in a sibling worktree (Track A, `setq`/`defun`); per the
plan's parallelism summary this is exactly the expected Track C / Track A split, and the only anticipated
merge conflict was on `src/smd/smdlisp/CMakeLists.txt` (both steps add an `add_subdirectory` line and a link
entry) — this step did not touch `elaborator/` or `closure/` themselves, only *consumed* `elaborate.hpp`/
`elaborated_core.hpp` as read-only includes, so there should be no conflict beyond that one file's two
`add_subdirectory`/link lines, resolvable by keeping both lines side by side in whichever order the
orchestrator's merge lands them.

**Where the pass sits (decision D9).** `smd::smdlisp::macroexpand` operates entirely on
`reader::datum_type<MaxNodes, MaxList>` trees (L6) — nothing in `expander.hpp` constructs or inspects an
`elaborator::core_type` node except `expand_and_elaborate` (below), which only calls `elaborator::elaborate`
as the next pipeline stage once expansion has finished. No changes were needed in `elaborator/`, `closure/`,
or `reader/` to land this step — the datum tree's existing shape (arena + `arena_box` handles, `datum_list`'s
`static_vector<arena_box, MaxList>` elements) was sufficient to build every replacement form the six macros
below need.

**Registry / API shape**, matching the plan's sketch almost exactly:

```cpp
template <int MaxNodes, int MaxList>
using macro_fn = smdscheme::foundation::result<reader::datum_type<MaxNodes, MaxList>> (*)(
    call_form<MaxNodes, MaxList> const &call, arena_type<MaxNodes, MaxList> &arena);

template <int MaxNodes, int MaxList>
struct host_macro {
    std::string_view name{};             // folded operator spelling, e.g. "COND"
    macro_fn<MaxNodes, MaxList> expand{};
};

template <int MaxNodes, int MaxList>
using macro_table = smdscheme::foundation::static_vector<host_macro<MaxNodes, MaxList>, max_host_macros>;
```

`macro_fn` is a real function pointer (not a type-erased callable): host macros are ordinary, stateless,
`constexpr`-callable C++ functions, matching the plan's "Host macro: C++ function from a call-form datum to
a replacement datum." `host_macro::name` is a plain `std::string_view` compared against a call form's head
symbol's folded spelling (`reader::folded_name::view()`) — the exact same pattern
`detail::elaborate_list`/`detail::elaborate_function_position` in `elaborate.hpp` already use for special-form
dispatch, reused rather than inventing a second symbol-matching mechanism, per the handoff briefing's explicit
instruction (`smdlisp`'s reader layer has no `symbol` type of its own; `closure::symbol` is a *value*-layer
type from L7, irrelevant here since this pass runs before evaluation).

**Arena reuse decision.** Expansion writes every newly constructed replacement node into the *same* datum
arena the reader already populated — no second arena. This is the simplest design that satisfies "the
expander speaks the reader's datum language": every handle `expand()` produces or consumes is an ordinary
`arena_box` into the one arena the caller already owns, exactly mirroring how the elaborator's `core_arena` is
a single caller-owned arena threaded through every recursive call.

**Budget.** `macroexpand_1` applies at most one step at the head position (returns the datum unchanged plus an
`expanded` flag if the head is not a list, is an empty list, its head is not a symbol, or the head symbol
names no registered macro). `macroexpand` iterates `macroexpand_1` at the head position to a fixpoint under a
`max_expansions` budget (`default_max_expansions = 64`); exhausting the budget without reaching a fixpoint is
a diagnosed `parse_error` ("macroexpand: expansion budget exceeded"), never a hang — pinned by
`ExpanderTest - MacroexpandBudgetExhaustionIsDiagnosedError`, which registers a pathological test-only macro
that always re-expands into a call of itself and confirms `macroexpand` reports an error at a small budget
(8) rather than looping.

**Whole-tree wiring (`expand()`).** `macroexpand`/`macroexpand_1` alone only ever inspect *one* expression's
own head, matching ANSI CL's `macroexpand`/`macroexpand-1` exactly — they do not recurse into subforms. Making
the actual pipeline work (a `cond` nested inside an `if`'s branch, or inside a `let`'s body, must also get
expanded) needed one more function, `expand()`: at each list node, apply `macroexpand` at the head to a
fixpoint first, then recurse into the (possibly replaced) list's own elements — *except* when the list's head
symbol is `QUOTE`, whose argument is literal data and must never be macro-walked (`'(cond a b)` must stay the
literal three-element list `(COND A B)`, not silently evaluate to `2`; pinned by
`ExpanderTest - ExpandDoesNotWalkIntoQuotedData`). A `#'`/`(function ...)` node (`reader::datum_function`) is
walked into its embedded lambda body only when its target is a `(lambda ...)` form (code); a bare
function-name symbol target is left untouched (nothing to walk). `expand_and_elaborate(pd, datum_arena,
core_arena, table, max_expansions) -> result<core_type>` is the actual pipeline entry point: calls `expand()`
then `elaborator::elaborate()` on the result, matching `elaborate()`'s own `(datum, datum_arena, core_arena)`
signature shape with the macro table and budget appended.

**`expand()` is generic and special-form-agnostic — this is a deliberate, documented scope cut, not an
oversight (DIV-0005).** It does not know `if`/`let`/`lambda`/etc.'s argument shapes the way the elaborator
does (by design, per D9 — macro expansion stays independent of the elaborator), so it cannot distinguish "this
sublist is a binding/parameter list, not code" from "this sublist is an ordinary subexpression." In the narrow
case where a program reuses one of the six macro names as a variable or parameter name (e.g.
`(lambda (cond) cond)`), the walk may misinterpret the unrelated list `(cond)` as an attempted zero-clause
`cond` call, and `cond_expand` would then reject it with a spurious "expected at least one clause"-shaped
error. This is not exercised by any test here and is not expected to matter for any of this project's example
programs; see DIV-0005 for the full writeup and the companion (unrelated) hygiene note below.

**The six baseline macros — composability was the API-shape sanity check, and it held.** `cond`, `when`,
`unless`, `and`, `or`, `case` are all ordinary `host_macro` registry entries (via `default_macro_table<MaxNodes,
MaxList>()`), each an unconditionally-called free function — no ad-hoc special-casing anywhere in
`macroexpand_1`/`macroexpand`/`expand()` for any particular macro name, confirming the plan's explicit
API-shape test ("if implementing these six needs ad-hoc special-casing rather than each being an ordinary
registry entry, the API shape is wrong"). Each expander peels off exactly *one* clause/argument per call and
leaves the rest as a fresh call of the same (or a related) macro, relying entirely on `expand()`'s recursive
walk to keep unwinding the result to a fixpoint — none of the six builds a multi-level expansion by hand:

- `(cond)` → `NIL`; `(cond (test forms...) rest...)` → `(if test (progn forms...) (cond rest...))`. A
  test-only clause with zero body forms (`(cond (a))`, legal in full ANSI CL) is **not supported** — a
  documented, tested scope cut (`ExpanderTest - CondWithNoClausesExpandsToNil` covers the zero-clause case;
  the zero-form-clause restriction mirrors L10's own "at least one body expression" cut for `lambda`/`progn`).
- `(when test forms...)` → `(if test (progn forms...))`, `NIL` body if `forms...` is empty.
- `(unless test forms...)` → `(if test NIL (progn forms...))`, `NIL` alternative if `forms...` is empty.
- `(and)` → `T`; `(and e)` → `e`; `(and e1 e2...)` → `(if e1 (and e2...) NIL)`.
- `(or)` → `NIL`; `(or e)` → `e`; `(or e1 e2...)` → `(let ((%OR-TMP e1)) (if %OR-TMP %OR-TMP (or e2...)))` —
  binds `e1` once via `let` rather than emitting `(if e1 e1 (or e2...))` directly, so `e1` is evaluated exactly
  once even though its value is needed in two positions, matching ANSI `or`'s single-evaluation contract.
- `(case keyform ((k1 k2...) forms...)... (t forms...))` → `(let ((%CASE-KEY keyform)) (cond ((eql %CASE-KEY
  'k1) forms...)... (t forms...)))` — each clause's key spec becomes an `eql`-against-quoted-key test (a
  multi-key clause becomes an `(or (eql ...) (eql ...) ...)`, itself expanded by `or`'s own macro when
  `expand()`'s recursive walk reaches it); a bare `T`/`OTHERWISE` key spec (not inside a list) is the
  catch-all, matching ANSI CL exactly — `(t ...)`/`(otherwise ...)` with the symbol *inside* a list is an
  ordinary one-key clause instead, also matching ANSI CL. `case`'s `let`-bound temp emits `COND`, which
  `expand()`'s walk expands in turn (relying on `cond`'s own zero-body-clause restriction transitively, so a
  case clause with no body forms is rejected the same way a bare `cond` clause would be).

**Non-hygienic temp variables (DIV-0005, second half).** `or`'s `%OR-TMP` and `case`'s `%CASE-KEY` are fixed,
ordinary (interned) symbol spellings, not `gensym`-style fresh/uninterned names — this project has no notion
of uninterned symbols yet (that arrives with L18/L19's backquote/`defmacro` machinery). A user form that
happened to bind a variable literally named `%OR-TMP` or `%CASE-KEY` inside the relevant macro's arguments
could observe variable capture. Nested `or`/`case` expansions do not collide with *each other* (each `let`
introduces its own fresh scope, most-recent-binding-wins lookup per L9's `env`), only a user-supplied name
could collide. Documented in DIV-0005 alongside the generic-walk limitation above; both are accepted scope
cuts for this step, not bugs.

**Merge criteria.** `(cond (nil 1) (t 2))` ⇒ `2` is proven both as a compile-time `static_assert` and as
`TEST_CASE("ExpanderTest - CondEndToEnd")` in `expander.test.cpp`, via `expand_and_elaborate` +
`eval_direct` — **no evaluator or elaborator changes were needed**, exactly as the plan predicts: every one
of the six macros bottoms out in `if`/`progn`/`let`/application, all already-elaborable special forms as of
L10. Plus expansion-shape `TEST_CASE`s for all six macros (checking the constructed datum tree's structure
directly via `std::holds_alternative`/symbol-name comparisons, since datum trees have no `operator==`), a
`macroexpand_1`/`macroexpand` plumbing pair (no-op on a non-macro call; the budget-exhaustion diagnosis
described above), a whole-tree-recursion test (`(if (when t 1) 2 3)` — the nested `when` gets expanded even
though it's not at the top level), the quote-boundary test described above, and end-to-end `eval_direct`
`TEST_CASE`s for `when`/`unless`/`and`/`or`/`case` beyond the mandated `cond` case (including `case`'s
otherwise-fallthrough path).

`make compile`, `make test` (461/461 passed, including 22 new `ExpanderTest` cases plus the merge-criteria
`static_assert`), and `make lint` all passed clean at landing. `git diff --stat -- src/smd/smdscheme` is empty
(frozen tree untouched); `git diff --stat -- src/smd/smdlisp/reader src/smd/smdlisp/elaborator
src/smd/smdlisp/closure` is also empty (those three lanes — the latter two owned by the concurrent L12 worker
— were read from, via ordinary includes, but never modified). **DIV-0005** filed this step (see
`docs/divergences/DIV-0005-macroexpand-not-hygienic-or-form-aware.md`) for the two scope cuts above (no
gensym; `expand()`'s form-agnostic walk); checked immediately before filing that DIV-0005 was still free (the
concurrent L12 worktree, `wt-l12`, had not claimed it as of this step's landing). No blog deliverable for this
step (phase 20 arrives at L19 per the plan's phase table).

## 2026-07-20 Step L12: `setq`, `defun`, `defvar`, `defparameter` landed

`src/smd/smdlisp/elaborator/{elaborated_core.hpp,elaborate.hpp}` and `src/smd/smdlisp/closure/{env.hpp,eval_direct.hpp}` gained additions (all additive; no existing declaration was removed or changed shape, so L9-L11's own tests needed no changes and pass unmodified). No new files, no new CMake targets — this step only extended the four files that already existed in the elaborator/closure lanes. `src/smd/smdlisp/reader/` and `src/smd/smdlisp/macroexpand/` were not touched (the latter does not exist in this worktree; it is L17's concurrent lane). `git diff --stat -- src/smd/smdscheme` is empty (frozen tree untouched).

**Four new core kinds** (`elaborated_core.hpp`): `core_setq<R,MaxNodes>{ name; value }`, `core_defun<R,MaxNodes>{ name; lambda }` (`lambda` is an `arena_box` to an ordinary `core_lambda` node), `core_defvar<R,MaxNodes>{ name; value }`, `core_defparameter<R,MaxNodes>{ name; value }`. All four `name` fields are plain `std::string_view`, not an owned `reader::folded_name` like `core_symbol`: per L10's established reasoning, a name reached via `datum_arena.get(lst.elements[i])` (always true here — `setq`/`defun`/`defvar`/`defparameter`'s name is always a list element, never the reader's un-arena-backed root return value) is safe as a bare view, matching `core_function.target` and `core_lambda.params`'s existing precedent.

**`defun`'s elaboration reuses `elaborate_lambda`, not a duplicate formals/body parser** (`elaborate.hpp`'s `DEFUN` case): `(defun name (formals...) body...)` differs from `(lambda (formals...) body...)` only by one extra leading `name` element, so the `DEFUN` case builds a synthetic `DatumList` — `{lst.elements[0] (unused placeholder), lst.elements[2] (formals), lst.elements[3...] (body)}` — and calls `elaborate_lambda` on it directly. `elaborate_lambda` never reads its list argument's element 0, only elements 1 (formals) and 2+ (body), so this is a correct, zero-duplication reuse: `defun`'s duplicate-parameter check, formal-must-be-symbol check, and body elaboration are exactly `lambda`'s, not a second copy. One cosmetic cost: a malformed `defun` formals list surfaces `elaborate_lambda`'s own `"lambda: ..."` error text, not a `"defun: ..."`-prefixed one; this is a diagnostic-text imprecision, not a behavior difference, and was accepted rather than threading a context string through `elaborate_lambda`'s signature for one caller.

**`setq`/`defun`/`defvar`/`defparameter` special-form recognition** added to `elaborate_list` alongside the existing `quote`/`if`/`progn`/`let`/`let*`/`lambda`/`function` set (folded-spelling comparison, per D2, matching the existing style exactly).

**`env.hpp` gained a `store<Core, MaxStore>` class** (`default_max_store = 256`), adapted by copy from `smd::smdscheme::closure::store` — the landed `set!` machinery the plan named as the pattern to adapt. `env<Core, MaxBindings>` gained a dual mode exactly mirroring `smd::smdscheme::closure::env`'s functional-vs-mutable split: two new constructors (`env(store*)`, `env(store*, pair_heap*)`), and `define_value`/`define_function` now allocate their bound value in the shared store (when one is present) instead of holding it inline, recording a `loc` in the `binding` struct instead. Copying an `env` copies the (shared, non-owning) `store*` shallowly, exactly like the existing `pairs_` pointer — this is what makes a mutation visible across every copy that shares the store, including a closure's captured copy, which is the entire reason `set!`/`setq` needs a store at all rather than a plain inline value.

**`env::set_value(symbol, value) -> result<value<Core>>` is `setq`'s primitive.** Decision made this step, per the plan's explicit invitation to pick a return type and record why: the plan's own section-9 sketch writes `set_value(symbol, value) -> result<void>`, but the same paragraph also says "`setq` returns the assigned value per CL, not the Scheme `unspecified`" — a `result<void>` cannot carry that value back to the caller, so `result<value<Core>>` (echoing the assigned value on success) is the type that actually satisfies the stated behavior; `result<void>` would have been internally consistent with the plan's literal signature sketch but inconsistent with the plan's own stated ANSI CL requirement one sentence later. Errors: `"setq: environment has no mutable store"` (functional-mode env) or `"setq: unbound variable"` (store-backed env, name not found in the VARIABLE namespace) — `setq` only ever searches `values_`, never `functions_`, and only ever mutates a binding already present (no dynamic/special-variable interaction; explicitly deferred to L16 per the plan).

**`defun`'s self-recursion support (a design decision beyond the plan's literal text, not required by the merge criterion, but implemented because real CL's `defun` always supports self-recursion and it would otherwise be a silent, surprising gap).** `env::define_function` now returns the store location it allocated (`-1` in functional mode), and a new `env::patch_function(symbol, int loc, value) -> void` overwrites that location in place (store-backed) or falls back to an ordinary shadowing `define_function` call (functional mode). `eval_direct`'s `core_defun` case: (1) `define_function(name, nil_t{})` — a placeholder — capturing the returned `loc`; (2) `envs.alloc(environment)` — captures a copy of the environment, which (store-backed) shares the *same* store pointer as `environment`; (3) builds the real closure value; (4) `patch_function(name, loc, real_value)` — overwrites the shared store cell. Every environment copy sharing that store — including the closure's own just-captured copy — observes the patched value on its next `lookup_function`, which is exactly what lets a self-recursive `defun` (e.g. `fact`) find its own name inside its own body. This only works when the environment is store-backed; a functional-mode (no-store) environment's `patch_function` falls back to shadow-redefining the *outer* environment object, so external calls to the function are still correct, but a self-recursive call inside the function's own body fails with `"attempted to call non-function"` (the placeholder `nil_t` was already captured and cannot be patched after the fact without a store) — `eval_direct.test.cpp`'s `DefunDoesNotSupportSelfRecursionWithoutStore`/`DefunNonRecursiveWorksWithoutStore` pin both halves of this. `defun` returns the function name as a `closure::symbol`, per ANSI CL.

**`defvar`/`defparameter`'s special-mark-only behavior (no dynamic binding yet, per the plan).** `env` gained `declare_special(symbol)`, `is_special(symbol) const -> bool` (a new `static_vector<symbol, MaxBindings> special_` member, entirely separate from the two binding lists), and `define_special_value(symbol, value)` (== `define_value` + `declare_special`, the `defvar`/`defparameter` primitive). `eval_direct`'s `core_defvar` case always calls `declare_special` but only evaluates and binds the value form if `lookup_value` does not already succeed (ANSI CL: an already-bound `defvar` does not re-evaluate or rebind); `core_defparameter` always declares special AND always (re)evaluates and (re)binds. Both return the variable name as a symbol.

**Top-level sequencing decision (the question this step's handoff-next.md explicitly flagged as open).** `eval_direct`'s `environment` parameter changed from `env<...> const&` to `env<...>&` (both `eval_direct` and its forward declaration; `apply_function_value` does not take `environment` at all, so it needed no change). This is the *only* mechanism needed: `core_progn`'s evaluation loop (unchanged code) already threads the exact same `environment` reference through every sibling expression, never a copy, so a `defun` (or `setq`/`defvar`/`defparameter`) evaluated earlier in a `progn` is visible to a sibling expression evaluated later in the *same* `progn` call — which is exactly the merge criterion's shape, `(progn (defun twice (x) (+ x x)) (twice 4))`. **Decision: no separate "run a sequence of top-level forms" driver was built.** The plan's own merge criterion already wraps both forms in one `progn`, which this mechanism handles with nothing more than the constness relaxation; real multi-form top-level input with no wrapping `progn` (e.g. two independent top-level datums fed to the reader one at a time) is explicitly out of scope for L12 and was not investigated further — a later step (plausibly whichever one first needs to run a whole multi-form source file, likely L22's public API / Godbolt work) would need a new entry point that reads a sequence of datums and threads one mutable environment across all of them, since nothing in the current pipeline reads more than one top-level datum per call. Relaxing `environment`'s constness required no changes at any recursive call site (they all just forward the `environment` parameter by name) and no changes to any pre-L12 test (every existing test already constructed its environment as a non-const local).

**Merge criterion**, as a compile-time `static_assert` in `eval_direct.test.cpp`: `(progn (defun twice (x) (+ x x)) (twice 4))` ⇒ `8`, using a store-backed `default_env<Core, MaxBindings>(store, heap)`. Plus 31 new runtime `TEST_CASE`s across `env.test.cpp` (7: store alloc/get/set, mutable-mode `set_value` success/error paths, `is_special`/`declare_special`/`define_special_value`, `patch_function` visible-through-copy and functional-mode-fallback), `elaborate.test.cpp` (12: each of the four new special forms elaborates to its core kind, plus arity/non-symbol-name/duplicate-parameter error paths), and `eval_direct.test.cpp` (12: `setq` mutating and returning the assigned value, `setq` errors with and without a store, `defun` define-and-call, `defun` returning the function name, `defun` self-recursion with and without a store, `defvar` bind-if-unbound and no-rebind-if-bound, `defparameter` always-rebinds).

`make compile`, `make test` (470/470 passed, including the 31 new `TEST_CASE`s plus the one new merge-criterion `static_assert`), and `make lint` all passed clean at landing (clang-format reformatted the newly-added code in place during authoring, as is normal for brand-new content — no pre-existing file needed reformatting). No blog deliverable for this step (phase 18 arrives at L13, the CPS closure backend, per the plan's phase table).

**Divergence doc filed:** `docs/divergences/DIV-0006-defvar-defparameter-require-value-form.md` (status `accepted-permanent`) — `defvar`/`defparameter` require a value form (`(defvar name value)`), rejecting ANSI CL's legal value-less `(defvar name)` proclaim-only form and any trailing docstring argument. Filed (rather than treated as plan-invited worker discretion, unlike the `set_value` return-type decision and the self-recursion design above) because it is a genuine, easily-overlooked ANSI CL behavior gap, not an internal C++ architecture choice — see the doc for the full reasoning and revisit condition (plausibly L16). Originally authored as DIV-0005 in this worktree; renumbered to DIV-0006 by the orchestrator at merge time because the concurrently-landed L17 step (macro expander) claimed DIV-0005 first (`DIV-0005-macroexpand-not-hygienic-or-form-aware.md`). Next free divergence number after this step: **DIV-0007**.

## 2026-07-21 Step L13: CPS closure backend landed (+ phase 18 draft)

`src/smd/smdlisp/closure/{cps_code.hpp,cps_code.test.cpp,closure_program.hpp,closure_program.test.cpp}` are new, added to the existing `smdlisp_closure_headers` FILE_SET in `src/smd/smdlisp/closure/CMakeLists.txt` (no new CMake target — both join `value.hpp`/`pairs.hpp`/`env.hpp`/`eval_direct.hpp` under the existing `smdlisp.closure` target; the two `.test.cpp` files are picked up automatically by `smdlisp_test`'s `GLOB_RECURSE`). No existing file's declarations changed shape; L9-L12's own tests needed no changes and pass unmodified. `src/smd/smdlisp/reader/` and `src/smd/smdlisp/macroexpand/` were not touched (the latter is L18's concurrent lane). `git diff --stat -- src/smd/smdscheme` is empty (frozen tree untouched).

**Architecture, adapted from `smd::smdscheme::closure::{cps_code.hpp,closure_program.hpp}` per `docs/cps-direction.md`'s structural-recursion-over-flat-arena decision.** `cps_code<F>` wraps a callable `(Env&, Envs&, K) -> result<value<Core>>`; `detail::cps_dispatch<MaxNodes,MaxList,MaxBindings,MaxEnvs,Cont,K>` pattern-matches all sixteen `elaborated_core.hpp` node kinds (the twelve L10/L11 kinds plus L12's `core_setq`/`core_defun`/`core_defvar`/`core_defparameter`), threading a two-continuation `(cont, k)` pair exactly like the Scheme original: subexpressions that need a definite value right away (an `if` condition, an application's evaluated arguments, a `setq`'s value form) are evaluated with `identity_k`/`identity_k`; a node's own tail position (an `if` branch, a `progn`'s last expression, a called closure's last body expression) tail-chains the caller's own `(cont, k)` straight through instead of returning up through the recursion — the "progn compiles to continuation chaining" structure the plan named, extended to a closure's implicit-progn body since both are the same core-node shape. `detail::cps_apply` is the CPS counterpart of `eval_direct.hpp`'s `apply_function_value`: the single call-dispatch point for ordinary application, `funcall`, and `apply` alike, reused recursively so a `funcall`ed closure tail-chains through the caller's `(cont, k)` exactly like a direct application would.

**The one deliberate structural departure from the Scheme original: `environment` is threaded as a mutable reference (`env<Core,MaxBindings>&`), not `Env const&`.** Scheme's `cps_code`/`cps_dispatch` take the environment by `const&` throughout, because Scheme's only mutation (`set!`) goes through a shared `store` cell — the environment *object* itself never changes shape. `smdlisp`'s `defun` is different: `env::define_function` appends a brand-new binding to the function-namespace vector, mutating the environment object itself, not just a store cell it points at (same reasoning L12 already used to justify `eval_direct`'s `env<...>&` parameter). A `const&` environment cannot support that, so `cps_dispatch`/`cps_apply`/`cps_code::operator()` all take `Env&`/`Envs&` by mutable reference, mirroring `eval_direct`'s discipline instead of the frozen Scheme file's.

**`setq`/`defun`/`defvar`/`defparameter` through CPS — the real design question, not a mechanical port.** The frozen `smd::smdscheme::closure::cps_code.hpp`'s own `core_define` case unconditionally returns a `parse_error` ("define not supported in expression context") — the Scheme CPS backend never supported top-level function definition at all, so there was no existing pattern to adapt for these four forms; each had to be worked out directly against `eval_direct.hpp`'s already-landed L12 semantics. `setq` mirrors Scheme's `core_set` shape closely (evaluate the value, mutate through `env::set_value`'s shared store, continue with the assigned value). `defun` reproduces `eval_direct`'s reserve-then-patch dance verbatim, at the same granularity, inside `cps_dispatch`'s `core_defun` case: `environment.define_function(name, nil-placeholder)` to reserve a store location and return it, `envs.alloc(environment)` to capture the (still-placeholder-bound) environment, build the real closure value, then `environment.patch_function(name, loc, real_value)` to overwrite the shared store cell in place. Every environment copy sharing that store — including the closure's own just-captured copy — observes the patch on its next `lookup_function`, which is what lets `cps_apply`'s `closure` branch find a self-recursive `defun`'s own name inside its own body: `(progn (defun fact (n) (if (eq n 0) 1 (* n (fact (+ n -1))))) (fact 5))` ⇒ `120` runs correctly through `compile_to_closure`, proven as a `TEST_CASE`, exercising five recursive calls through `cps_dispatch`/`cps_apply`. `defvar`/`defparameter` mirror `eval_direct`'s bind-if-unbound / always-rebind split exactly, evaluating their value form through `identity_k`/`identity_k` (a definite value is needed before `env::define_value`/`declare_special` can run) and continuing with the variable name symbol.

**A second, independently-discovered lifetime bug — not the same one L10/L11 hit, but the same *class*.** `compile_to_closure`'s first draft mirrored the Scheme original exactly: a single `std::string_view src` parameter, with the datum arena as a function-local temporary. `make compile` failed immediately with a GCC constexpr diagnostic — "accessing 'arena_dr' outside its lifetime" — not a runtime/ASan failure discovered later, but a compile-time rejection of the very first `static_assert` that exercised a `lambda`. Root cause: per D2, `smdlisp` folds symbols to uppercase at read time, so `reader::folded_name` owns its storage inline inside a datum node; `core_lambda::params` and `core_function::target`'s bare-symbol alternative (`elaborated_core.hpp`, unchanged since L10) are `std::string_view`s into that datum-node-owned storage, and those views are read every time the *compiled program* is later called (resolving an application head, looking up a parameter name), not merely during elaboration. The Scheme original never hits this because its equivalent views point into the caller-owned *source string*, never into reader-owned storage. **Fix:** `compile_to_closure<MaxNodes,MaxList>(src, datum_arena)` now takes the datum arena as a caller-owned, mutable out-parameter — the caller must keep it alive for as long as the returned program is called, exactly the same discipline `eval_direct.test.cpp`'s `run()`/`run_mut()` already follow for the analogous `Core &root` parameter. `core_arena`, in contrast, stays function-local: `compile_cps`'s `cps_of` captures its `arena` argument BY VALUE (a full deep copy, required by the same "constexpr engine lambda execution requires `tree_arena` elements captured by value" architecture fact already in this file's "Architecture facts" section), so nothing in the returned `closure_program` depends on `core_arena`'s own lifetime. Filed as `docs/divergences/DIV-0008-compile-to-closure-needs-caller-owned-datum-arena.md` (`accepted-permanent`) since it is a permanent, structural consequence of D2, not a bug to fix later.

**A third instance of the identical `value<Core>` symbol/keyword lifetime hazard L10/L11 already documented, caught by a failing `TEST_CASE` after the above fix landed, not by the compiler this time.** `closure_program.test.cpp`'s first-draft `run()`/`run_mut()` helpers (mirroring `eval_direct.test.cpp`'s own helpers) called `compile_to_closure` and returned the *evaluated value* out of the helper, with the compiled program (`pr`, a `result<closure_program<...>>`) as the helper's own local variable. `ClosureProgramTest - KeywordSelfEvaluates` failed under AddressSanitizer with a stack-use-after-return: `cps_of`'s lambda captures the elaborated core root BY VALUE, so a `core_keyword`'s owned `folded_name` storage lived inside `pr`'s `CpsCode`, and `pr` died when the helper returned. **Fix:** `pr` (the compiled program) must be declared directly in each `TEST_CASE`/`static_assert` body, not hidden behind a helper that returns a value derived from it; only the "evaluate an already-compiled, already-successful program" step (which needs no arena/root of its own) is factored into small `eval`/`eval_mut` helper templates. The same latent bug existed in `cps_code.test.cpp`'s first-draft `run()` helper (which similarly returned a value out of a function holding the compiled `code` as a local) but did not manifest there under this build/optimization configuration — almost certainly because that helper was small enough to be inlined away, eliminating the separate stack frame ASan instruments; fixed anyway by applying the identical "compile inline, evaluate via a small helper" discipline, since relying on inlining to paper over a real lifetime bug is not a fix. **Durable fact for L14 and beyond:** any helper that calls `compile_to_closure`/`compile_cps`/`cps_of` must keep the resulting compiled object (program or `cps_code`) alive in the SAME scope that inspects a `symbol`/`keyword` result — never return a bare `value<Core>` out of a function whose only reference to the compiled object was local to that function.

**Merge criterion**, per the plan: "every L11/L12 end-to-end test also passes through `compile_to_closure`." `closure_program.test.cpp` re-expresses the full L11/L12 `eval_direct.test.cpp` scenario set (not one narrow addition) through `compile_to_closure`: all three L11 merge-criteria `static_assert`s, the one L12 merge-criterion `static_assert`, and 22 runtime `TEST_CASE`s covering keyword self-evaluation, `nil`/`t` truthiness, `let`/`let*`, implicit progn, closures capturing both namespaces, `apply` spreading a final list argument, unbound-variable/undefined-function/non-function/arity-mismatch errors, `funcall`/`#'`, `setq` (mutation, return value, both error paths), `defun` (define-and-call, return value, self-recursion with and without a store), and `defvar`/`defparameter`. `cps_code.test.cpp` adds lower-level breathing tests directly against `cps_dispatch`/`cps_of`/`compile_cps` (below `compile_to_closure`'s own pipeline).

`make compile`, `make test` (531/531 passed, including 9 new `CpsCodeTest` cases, 26 new `ClosureProgramTest` cases, and 7 new merge-criterion `static_assert`s), and `make lint` all passed clean at landing (gersemi reformatted `src/smd/smdlisp/closure/CMakeLists.txt`'s `FILES` list onto multiple lines during authoring — cosmetic, expected for a new multi-file addition). `git diff --stat -- src/smd/smdscheme` and `git diff --stat -- src/smd/smdlisp/reader src/smd/smdlisp/macroexpand` are both empty.

**Divergence doc filed:** `docs/divergences/DIV-0008-compile-to-closure-needs-caller-owned-datum-arena.md` (status `accepted-permanent`) — see above. Originally authored as DIV-0007 in this worktree; renumbered to DIV-0008 by the orchestrator at merge time because the concurrently-landed L18 step (backquote) claimed DIV-0007 first (`DIV-0007-backquote-nested-and-append-two-arg.md`). Next free divergence number after this step: **DIV-0009**.

**Blog:** `docs/blog/phase-18-setq-defun-progn.org` drafted (`DRAFT — pending author revision`), covering: what a CPS backend buys over the direct evaluator (a first-class continuation L14's `block`/`return-from` will need, not yet exercised here); `progn`'s continuation-chaining structure; the discovery that the frozen Scheme CPS backend's `core_define` case is an unconditional error, making `setq`/`defun`/`defvar`/`defparameter` a genuine design question rather than a port; the reserve-then-patch `defun` self-recursion mechanism reproduced in CPS; and the caller-owned-datum-arena lifetime bug (DIV-0008) the merge-criterion tests surfaced. Four new UUID anchors, all in `cps_code.hpp`: `14e385d3-52c6-48d4-8ed2-6c6882ce1f33` (`cps_code` struct, the mutable-environment design note), `60425319-c29d-44d1-b752-8945294942b9` (`core_progn`'s continuation-chaining dispatch), `842db09a-6e1d-4b69-b456-674c421cd693` (the `core_setq`/`core_defun` dispatch pair), `39b1b1fa-eecc-4210-9dc7-4512aba2f6c3` (`cps_apply`'s `closure` branch — the implicit-progn body walk that makes self-recursion work). `docs/blog/index.org` gained the Phase 18 entry. Per DIV-0004's standing convention, the four `#+transclude:` links point at `orgit:~/src/compile-time-scheme/wt-l13::...` (this step's own worktree); the orchestrator repoints them to `main` after merge. `make blog-md` renders `docs/blog/phase-18-setq-defun-progn.md` cleanly (all four transclusions resolve); two incidental `id="orgXXXXXXX"`-only diffs in `phase-16-reading-common-lisp.md`/`phase-4-elaboration.md` from the whole-directory re-render were reverted before committing, matching prior steps' precedent. Two org-markup adjacent-verbatim-span footguns (the same class L11 documented) were caught and fixed while drafting: `~word~/~word~` with no space around the slash (`~block~/~return-from~`, `~setq~/~defun~/~defvar~/~defparameter~`, `~funcall~/~apply~`) merges into one giant literal span in GFM export exactly like the adjacent-`~verbatim~~verbatim~` case already documented, not only the "closing `~` immediately followed by a bare letter" case — both are instances of "a closing `~` must be followed by whitespace or ordinary punctuation, and `/` reopening a span without an intervening space does not count." Fixed by adding a space on both sides of the slash.
