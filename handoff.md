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
