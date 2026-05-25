# SchemePoC checklist

## Ground rules

- [x] `docs/codestyle.org` copied into repo.
- [x] `docs/CODING_RULES.md` copied into repo.
- [x] `AGENTS.md` created.
- [x] C++26 baseline recorded.
- [x] GCC16 baseline recorded.
- [x] Catch2 test framework recorded.
- [x] No GTest references remain.
- [x] Beman Execution policy recorded.
- [x] Beman dependency submodule policy recorded.
- [x] Every file has canonical path comment and Emacs mode line.
- [x] Every source-like file has SPDX header.
- [x] Header guards use project convention.
- [x] Includes use canonical angle-bracket spelling.
- [x] No relative project includes.
- [x] No `using namespace` in headers.
- [x] Component header is included first and twice in tests.
- [x] Every public constexpr API has a constexpr/static_assert test.
- [x] CMake uses targets and file sets.
- [x] Local CMakeLists only list local files.
- [x] `make compile` passes.
- [x] `make test` passes.
- [x] `make lint` passes.

## Steps

- [x] Step -1: add agent/style governance files
- [x] Step 0: repository recustomization and skeleton
- [x] Step 1: core utility vocabulary
- [x] Step 2: input cursor and lexical primitives
- [x] Step 3: minimal parser object
- [x] Step 4: functor and applicative combinators
- [ ] Step 5: alternative, repetition, and lexeme
- [ ] Step 6: reader atom model
- [ ] Step 7: fixed-capacity datum tree
- [ ] Step 8: datum reader for lists and quote
- [ ] Step 9: core language model
- [ ] Step 10: datum-to-core elaborator for literals, variables, calls
- [ ] Step 11: direct evaluator for core arithmetic
- [ ] Step 12: elaborate `if`
- [ ] Step 13: typeclass-object facade for parser operations
- [ ] Step 14: generic fixpoint playground
- [ ] Step 15: CPS closure backend facade
- [ ] Step 16: bottom-up CPS direction decision
- [ ] Step 17: defunctionalized CPS program
- [ ] Step 18: closure materialization over CPS program
- [ ] Step 19: public one-shot API
- [ ] Step 20: lambda syntax
- [ ] Step 21: runtime closure values
- [ ] Step 22: function application
- [ ] Step 23: lexical closure capture
- [ ] Step 24: quote elaboration
- [ ] Step 25: error quality pass
- [ ] Step 26: negative compile tests
- [ ] Step 27: Godbolt single-file extraction
- [ ] Step 28: vendor Beman Execution
- [ ] Step 29: sender adapter over Beman Execution
- [ ] Step 30: sender backend over CPS program using Beman Execution
- [ ] Step 31: optional Beman Task integration, only if needed
- [ ] Step 32: reflection spike
- [ ] Step 33: documentation consolidation
