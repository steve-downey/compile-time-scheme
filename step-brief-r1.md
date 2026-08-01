# Step brief: R1 (rebuild substrate)

Forward-only handoff for the next agent.
Read `docs/codestyle.org`, `AGENTS.md`, `docs/CODING_RULES.md`, `CLAUDE.md`, and this file.
Nothing else, except the anchored sections named under "Dependencies" below.

## Next step's goal

Create `src/smd/cl/foundation/` — the substrate the rebuilt Common Lisp front end is built on, and the future contents of the shared kit (R8).

It starts as the reviewed union of the two existing copies of the same code: `src/smd/smdscheme/foundation/` and, in the sibling repository `compile-time-forth`, `src/smd/forth/foundation/`.
Add what `docs/CODING_RULES.md` requires and neither copy has.

Contents: `static_vector`, `result`, `parse_error`, `source_pos`, `source_span`, `arena_box`, `functor`, `applicative`, `alternative`, plus two additions.

1. A **short-circuiting fold**.
   `std::ranges::fold_left` does not short-circuit, and the elaborator's error propagation needs one.
   This is the piece D15 rests on and it belongs here rather than in any front end.
2. **Foldable and Traversable typeclass instances**, with law tests, per `docs/CODING_RULES.md`'s Foldable/Applicative/Traversable rules and its "add law-focused tests before performance tests".

Delete `fold_fix` rather than porting it — see "What R0 found" below.

## Merge criterion

`make compile`, `make test`, `make lint` green, plus `make compile-headers` since this step defines a header set.

Law tests pass for every typeclass instance added.
These are the step's real evidence: the substrate has no behaviour of its own to demonstrate, so the laws are what "correct" means here.

`grep -c "for (int i = 0" src/smd/cl/` returns zero outside the generic algorithms themselves.
That is a real merge criterion, not a style preference: D15 records that the raw-loop count in `smdlisp` (43 across five headers) is a consequence of arena indices plus wide `std::visit`, and this step is where the alternative gets built.

## Files this step owns

```txt
src/smd/cl/CMakeLists.txt
src/smd/cl/foundation/*.hpp
src/smd/cl/foundation/*.test.cpp
src/smd/cl/foundation/CMakeLists.txt
src/CMakeLists.txt            (one add_subdirectory line)
checklist.md                  (tick R1)
```

Do not touch anything else.
`src/smd/smdlisp/**` is frozen from this step onward as the rebuild's behavioural oracle and is never edited — a different rule from the retired D1 freeze, and for a different reason.
`src/smd/smdscheme/**` is no longer frozen (D11) but is not this step's business either; read and copy from it.

## Dependencies already satisfied

Referenced by section rather than re-narrated.
Read these, do not reconstruct them.

- `docs/cl-rebuild-plan.md` §2 — decision records D11–D18.
  D15 is this step's mandate.
- `docs/compiler_architecture.org` §"A sender has three completion channels, and that is a semantic resource" — the rationale for D13.
  Not needed to build the substrate, but read it before designing `result`, because R5 will need a third alternative and it is cheaper to leave room now than to widen later.
- `docs/CODING_RULES.md` §Typeclass Design, §Foldable Rules, §Applicative Rules, §Traversable Rules — the authoritative specification for what this step builds.
  `fold_map` is the semantic centre; `length`, `to_vector`, `fold_left` and `fold_right` are derived where practical; traversal order is part of the instance contract and must be documented.

## What R0 found that this step needs and cannot get elsewhere

**Three findings were never compiled.**
R0 ran in a container with no gcc-16, so the following are careful code reading and nothing more.
This step runs where a compiler exists.
Confirm each before acting on it, and record the outcome.

- `foundation::fold_fix` (`src/smd/smdscheme/foundation/fix.hpp:49`) appears **uninstantiable**.
  Line 50 calls `fmap(tree.inner, lambda)` but the CPO in `functor.hpp` takes `(function, value)`.
  With those arguments the lookup is `functor_typeclass<lambda>`, which is `std::false_type`, and that has no `fmap`.
  Unqualified `fmap` inside `foundation` finds the CPO *variable*, so ADL never runs and no overload can rescue it.
  It also has **zero call sites** in either front end, and `fix.test.cpp` is a two-line stub whose `CMakeLists.txt` lists only the header — so nothing has ever instantiated it.
  Expected outcome: confirm, then delete rather than repair.
  `mendler_para` supersedes it, and there are two copies of that to unify (DIV-0016).
- DIV-0013 records a GCC trunk defect: under `-fsanitize=null`, a null comparison against the address of a subobject of a namespace-scope `constexpr` object does not fold.
  The doc carries a five-line reproduction.
  Re-run it against the current toolchain; it may be fixed, which would close the divergence for free.
- BL-0002's layout numbers came from a standalone probe built with g++-13, not from the real headers.
  Re-measure if this step touches `static_vector`'s shape.

**The two foundation copies have not diverged.**
Diffing `smdscheme` and `forth` gives 7–15 changed lines per file, and for `functor`, `applicative`, `alternative`, `result` and `source_pos` that difference is *exactly* the include guard, the namespace, and the "adapted by copy" comment.
So "reviewed union" is closer to a rename than a merge.
Read both, take the better of any genuine difference, and expect there to be almost none.
`compile-time-forth` is a separate public repository (`steve-downey/compile-time-forth`) and must be cloned to compare.

**A prose rule that is real and partly unobserved.**
`docs/CODING_RULES.md` requires one sentence per line in Markdown and Org, with no hard wrapping to a column.
Newer divergence docs follow it; `docs/cl-pivot-plan.md` does not.
New documents follow it.

**A test-writing rule that now has teeth.**
`docs/divergences/README.md` classifies each divergence, and D16 makes the consequence binding: a test may pin a `scope-decision`, never a `defect`.
Four named tests in `smdlisp` pin defects and have no counterpart in the new tree.
This does not bite in R1, which adds no language behaviour, but it governs every later step and is easiest to internalise now.

## Note for whoever picks this up

R0 was completed in a remote web session; this brief exists because that session's conversation history does not transfer to a local CLI.
Everything R0 concluded is in `docs/cl-rebuild-plan.md`, `docs/divergences/README.md`, and this file.
Nothing important is lost, but nothing is in anyone's head either — if a decision here looks unmotivated, the reasoning is in the plan's §1 and §3, not in a person.
