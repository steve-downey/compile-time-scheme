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
