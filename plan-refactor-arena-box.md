# Plan to apply `arena_box` and `Fix<F>` broadly

## Phase 1: Establish the Fixpoint Infrastructure
We already have `src/smd/schemepoc/arena_box.hpp` and `test`. 
We also have `src/smd/schemepoc/fix.hpp` which defines `Fix<F>` and `fmap` etc. (Wait, let's check `fix.hpp` to ensure we have `fmap`, `fold_fix`, `refold` etc. or if we need to write them.)

## Phase 2: Refactor `datum_tree.hpp` and `reader.hpp`
- Transition `datum_node` to `DatumF<R>`:
  - `LitInt`, `LitSym`, `LitBool`, `Quote<test_box<R>>`, `List<static_vector<test_box<R>, MaxList>>`.
- Transition `datum_tree` into just an instantiation `using Datum = Fix<DatumF>`.
- The backing arena will be `tree_arena<Datum, MaxNodes>`.
- Update `reader.hpp` to construct `Datum` values instead of storing `node_id` into a monolithic `datum_tree` object.

## Phase 3: Refactor `elaborator.hpp` and `Core AST`
- Switch `core_node` to `CoreF<R>`:
  - `Var`, `Lit`, `If`, `Call`, `Lambda`, `Let`, `Quote`.
- Use `Fix<CoreF>`.
- Update the `elaborator` to map `Fix<DatumF>` to `Fix<CoreF>`.
- Since we have `fold_fix`, the elaborator can possibly be written as an algebra `DatumF<Core> -> Core`.

## Phase 4: Refactor execution backends (`eval_direct`, `cps`, etc)
- Update `eval_direct` to evaluate `Fix<CoreF>` instead of `core_node`s mapped by id.
- Update `cps` to compile `Fix<CoreF>` to `closure_program`.

This refactoring replaces the explicit `id` management logic spanning the compiler stages with natural recursive type declarations that are valid in `constexpr`.
