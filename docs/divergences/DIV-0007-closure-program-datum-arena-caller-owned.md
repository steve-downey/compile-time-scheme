# DIV-0007: `compile_to_closure` takes the datum arena by caller-owned reference

- **Status:** open
- **Date:** 2026-07-22
- **Step:** L13 (CPS closure backend)
- **Authority diverged from:** docs/cl-pivot-plan.md (step L13's brief: "Name
  the public compile entry point to mirror
  `smdscheme::closure::closure_program`'s")

## What diverged

`smd::smdscheme::closure::compile_to_closure(std::string_view) ->
result<closure_program<...>>` takes a single argument and returns a fully
self-contained value: the datum arena used during reading is a function-local
temporary, discarded once the function returns.

`smd::smdlisp::closure::compile_to_closure` cannot do this.
Its signature is `compile_to_closure(std::string_view src, tree_arena<datum_type<...>,
MaxNodes>& datum_arena) -> result<closure_program<...>>`: the caller must
supply `datum_arena` and keep it alive for as long as the returned program is
called, exactly the same caller-owns-the-arena discipline
`eval_direct.test.cpp`'s `run`/`run_mut` helpers already use for their own
datum arena.

## Why

`smdlisp`'s elaborated core nodes hold plain `std::string_view` fields that
are views into the *datum* atom data they were read from: `core_lambda::params`,
`core_function`'s bare-name alternative, and `core_setq`/`core_defun`/
`core_defvar`'s names (see `elaborated_core.hpp`'s doc comments on each of
those fields -- they are deliberately plain `string_view`, not an owned
`reader::folded_name`, specifically because "every name here is a list
element, never the elaboration root, so it is always arena-backed").
"Arena-backed" there means backed by the *datum* arena's storage, not the core
arena's.

A `smd::smdscheme::foundation::tree_arena` is index-addressed
(`docs/cps-direction.md`: "Nodes are referenced by node_id (integer index)"),
so the *core* arena is safe to copy or return by value -- copying it just
copies index-based cross-references, which remain valid. But a
`std::string_view` embedded inside a core node is a raw pointer into whatever
memory backed the datum arena at elaboration time; copying or moving the
object that memory belongs to does not update that pointer, and letting the
datum arena go out of scope while the core arena (and any `string_view` inside
it) survives leaves a dangling view.

The first draft of this function kept the datum arena as a local variable,
matching `smd::smdscheme::closure::compile_to_closure`'s shape exactly. It
failed to build: GCC's constexpr evaluator rejected the
`LambdaApplicationWithCarCdr` test (`'(x)`'s `x` is a `core_lambda` param,
i.e., exactly the string_view-into-datum-arena case above) with `accessing
'arena_dr' outside its lifetime`, pointing directly at the local datum-arena
variable's declaration. This is a real bug the constexpr evaluator caught,
not a spurious diagnostic: the identical `string_view` would dangle at
runtime too, undetected until something happened to read through it (the
build-time catch is strictly better than the AddressSanitizer-at-runtime catch
L10 already hit once for a related dangling-view bug on the *root* node case,
which `elaborated_core.hpp`'s `core_symbol`/`core_keyword` fixed by owning
their name instead of viewing it -- that fix does not reach list-element names
like a lambda parameter, which stay `string_view`).

Fixing this at its root (making the elaborator copy every name into
core-arena-owned storage instead of borrowing from the datum arena) is an
`elaborator/` change, out of scope for this step (the step brief and the
orchestrator's lane rules both forbid touching `reader/`, `elaborator/`, or
`macroexpand/` from this lane).

## Consequences

- Every caller of `smd::smdlisp::closure::compile_to_closure` -- including
  `closure_program.test.cpp` -- must declare its own datum arena and pass it
  in, and must not let it go out of scope before it is done calling the
  returned program.
- This makes the `smdlisp` and `smdscheme` `compile_to_closure` entry points
  similarly *named* but not signature-compatible, contrary to a literal
  reading of the L13 brief's "mirror `smdscheme::closure::closure_program`'s"
  instruction. The public names (`closure_program`, `compile_to_closure`,
  `code` member) are otherwise identical.
- A future step that wants a truly one-argument, fully self-contained
  `compile_to_closure` for `smdlisp` needs the elaborator-side fix described
  above (owned storage instead of datum-arena-borrowed `string_view` for every
  name field, not just the root-node case `core_symbol`/`core_keyword` already
  fixed).
- The limitations doc (step L24) should mention this alongside DIV-0004's
  similar "arenas/paths are caller-owned, not portable-by-default" theme.

## Revisit condition

Revisit if a later step needs `smdlisp` programs to be freely relocatable
after compilation (e.g., a cache of pre-compiled programs, or serialization).
At that point, change `core_lambda::params`, `core_function`'s bare-name
alternative, and `core_setq`/`core_defun`/`core_defvar`'s names to an owned
`reader::folded_name` (or equivalent fixed-capacity owned storage) instead of
`std::string_view`, the same fix already applied to the root-node
`core_symbol`/`core_keyword` case, then drop the `datum_arena` parameter from
`compile_to_closure`.
