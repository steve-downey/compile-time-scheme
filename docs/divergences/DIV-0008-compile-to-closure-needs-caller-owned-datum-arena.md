# DIV-0008: compile_to_closure needs a caller-owned datum arena parameter

- **Status:** accepted-permanent
- **Date:** 2026-07-21
- **Step:** L13 (CPS closure backend for the baseline)
- **Authority diverged from:** docs/cl-pivot-plan.md (the plan's own phrasing,
  "adapted from the Scheme CPS backend", implies the same single-string
  `compile_to_closure(src)` entry point `smd::smdscheme::closure` already has)

## What diverged

`smd::smdlisp::closure::compile_to_closure` takes an explicit, caller-owned
datum arena out-parameter:

```cpp
template <int MaxNodes = 128, int MaxList = 16>
constexpr auto compile_to_closure(
    std::string_view src,
    smd::smdscheme::foundation::tree_arena<
        reader::datum_type<MaxNodes, MaxList>, MaxNodes> &datum_arena);
```

`smd::smdscheme::closure::compile_to_closure` (the frozen reference this step
adapts from) takes only `std::string_view src` and constructs its datum arena
as a function-local temporary.

## Why

Per decision D2, `smdlisp` folds symbol/keyword spellings to uppercase at
read time, so `reader::folded_name` (L5) owns its (folded) character
storage inline, unlike the Scheme reader's `datum_symbol.name`, which is a
bare `std::string_view` into the original, never-rewritten source text.
`core_lambda::params` and `core_function::target`'s `std::string_view`
alternative (both from `elaborated_core.hpp`, see L10's handoff note on the
identical reasoning for why those two fields stayed `string_view` rather
than an owned `folded_name`) are views into a datum node's `folded_name`
storage -- i.e. into the datum arena's memory -- and those views are read
every time the *compiled* program is later called (resolving an
application head, looking up a lambda parameter name by position), not
merely during elaboration.

A function-local datum arena (mirroring the Scheme original's function
body, since Scheme's views trace back to the caller-owned source string
instead) is therefore unsound: it goes out of scope when
`compile_to_closure` returns, while the returned program's core nodes
still view into it for the remainder of the program's evaluated lifetime.
GCC's constexpr evaluator caught this directly the first time this
function was written with a function-local arena (an "accessing
'arena_dr' outside its lifetime" diagnostic during `make compile`, not
merely a runtime/ASan failure) -- confirmed by build, not guessed.

The core arena (`core_arena`), in contrast, does not need the same
treatment: `compile_cps`'s `cps_of` captures its `arena` argument BY VALUE
(a full deep copy) into the returned `cps_code`, so `compile_to_closure`
keeps `core_arena` local without issue.

## Consequences

- Every caller of `smd::smdlisp::closure::compile_to_closure` (this step's
  own `closure_program.test.cpp`, and any later step reusing this entry
  point -- plausibly L21's sender backend, or L22's public API) must
  declare its own datum arena and keep it alive for as long as the
  returned `closure_program` is called, exactly the same caller-owned-arena
  discipline `eval_direct.test.cpp`'s `run()`/`run_mut()` helpers already
  follow for the identical reason.
- `closure_program.test.cpp`'s own test helpers were built around this
  from the start (see that file's header comment for the parallel,
  independently-discovered lifetime lesson about not returning a `value`
  out of a function that holds the *compiled program* itself as a local).
- L22 (public API, Godbolt, FFI parity) should check whether
  `smd::smdlisp::smdlisp.hpp`'s planned one-shot API
  (`compiled_lisp<Source>`) needs to own its datum arena as a member
  rather than exposing this parameter to end users directly.

## Revisit condition

None expected: this is a structural consequence of D2 (owned, case-folded
symbol storage) that would only go away if `core_lambda::params` and
`core_function::target`'s bare-symbol alternative were changed to hold an
owned `reader::folded_name` instead of a `std::string_view` (mirroring
`core_symbol`/`core_keyword`'s own L10 fix) -- which would remove the need
for a caller-owned datum arena but would cost an extra copy per parameter
name at elaboration time. Not attempted in this step; a candidate for L22
or a later cleanup pass if the caller-owned-arena parameter proves
awkward for the public one-shot API.
