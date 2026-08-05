<!-- markdownlint-disable MD013 -->

# C++ Rules

Read this last, immediately before writing code.
It binds human and AI authors.
Where it is silent, `docs/codestyle.org` (style and rationale) and `AGENTS.md` (process, build, CMake) govern; this file deliberately says nothing about CMake or workflow.

## Baseline

- C++26 on GCC16.
- No fallback paths for older standards or other compilers, and no compatibility scaffolding.
- When an older idiom and a newer one express the same thing, write the newer one.

## Files and names

- A file holds one component and is named after it: `result.hpp` declares `result`, `result.cpp` defines it, `result.test.cpp` tests it.
- The trio is co-located under `src/<namespace-path>/`, and namespace, directory path, and include spelling are the same path.
- File names are `snake_case`; new work uses `.hpp`, `.cpp`, `.test.cpp`.
- The `.cpp` exists only if there is out-of-line code; a header-only component is `<name>.hpp` plus `<name>.test.cpp`.
- Non-public helpers live in `detail/` below the component directory; a `detail/` header big enough to need its own tests is a component — promote it.

Every source file opens with the repo-relative path and Emacs mode line, then SPDX:

```cpp
// src/smd/cl/foundation/result.hpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
```

- Classical include guards named from the repo-relative path: `SRC_SMD_CL_FOUNDATION_RESULT_HPP`.
  Never `#pragma once`.
- Include project headers only by canonical angle-bracket spelling: `#include <smd/cl/foundation/result.hpp>`.
  Never relative, never by leaf name, never via a transitive include.
- A `.cpp` includes its own header first; a `.test.cpp` includes it first and then a second time.
- Headers are self-contained and never contain `using namespace`.

## Form of the code

- **An iteration is an algorithm.**
  Use `std::ranges` algorithms, views, and folds, or the foundation's short-circuiting fold when early exit is needed.
  A raw `for`/`while` loop is permitted only inside the substrate's own generic algorithms; anywhere else it is a defect, not a style nit.
- **An index range is not a loop counter, and `for_each` is rarely the algorithm.**
  `for_each(views::iota(0, c.size()), ...)` that only indexes back into `c` is a hand-written loop wearing an algorithm's clothes, and it is usually a symptom: the container is not a range.
  Fix the container first — give it the sequence the algorithms want (`nodes()`, `leaves()`) — and the callers become `transform`, `fold_left`, `fold_right`.
  Two more tells: a `for_each` whose lambda ignores its parameter is a fill, and a `for_each` that mutates a captured accumulator is a fold.
- **Two sequences walked together is `views::zip`, not `iota` plus two subscripts.**
  What decides whether an index survives is *where the pass writes*, not whether it happens to mention one:
  a read at the current position is a zip element;
  a write at the current position is an append, so the accumulator starts empty rather than prefilled with a sentinel;
  a read at another position is ordinary random access and is fine;
  and only a **write** at another position — a scatter — actually needs the index.
  The elaborator is one of each. Emission reads the node and its plan at the current position and appends its result, so it is a zip over `(nodes, plans)`.
  The role pass assigns each node's children their roles, which is a scatter, and stays indexed: zipping the table it writes into would alias it against its own iteration and still need the table by reference.
  A scatter is a scatter because of the representation — `tree_branch` names children and not a parent — not because of the loop.
- **A recursion over a tree is a catamorphism.**
  Consume recursive structure by passing an algebra to the substrate's fold (`mendler_para`, the Foldable and Traversable instances), not by hand-written recursive descent.
  A new operation over an AST is a new algebra, not a new `switch` or another wide `std::visit` ladder.
  `std::visit` may dispatch the alternatives of a single node inside an algebra; it must not drive the recursion.
- **Error propagation is `traverse`.**
  Threading `result` through a structure is `traverse` over the result applicative, not an `if (!r.has_value()) return r;` ladder.
- **`constexpr` everything; `consteval` what cannot be runtime.**
  Every API that can be meaningfully constant-evaluated is `constexpr`, and the contract is pinned with `static_assert` or a compile-time test.
  Use `consteval` where runtime invocation is meaningless: metafunctions, reflection queries, table generation.
- **Declarations tell the truth.**
  Public declarations carry their concepts and `requires` clauses; never drop a constraint to shorten a synopsis.
- **Definitions are out of line and fully qualified.**
  Hidden friends only for short, clearly marked customization points.
- **Capacity is not part of type identity.**
  No runtime value type is parameterised on a container's capacity; capacities belong to storage (decision D14).

## Typeclasses

- Instances are selected by variable template; a datatype and its typeclass adaptation are separate concerns.
- `fold_map` is the semantic centre; `fold_left`, `fold_right`, `length`, `to_vector` are derived where practical.
- `traverse` is the minimal Traversable operation; it preserves shape, and effect order follows the documented Foldable order.
- Traversal order is part of the instance contract; document it.
- The default tree Applicative is monad-derived; zip-like semantics are an explicitly named alternate, never a silent replacement.

## Tests

- Catch2, co-located, `.test.cpp`.
- The first two moves in every test file: double-include the component header, then a bootstrap test that always passes.
- Law tests — Functor, Applicative, Traversable identities, shape preservation, effect order — come before any other substantive test.
- `constexpr` contracts get compile-time tests alongside the runtime ones.
- **A contract is witnessed three ways, not two.**
  A `static_assert`, an `-O0` run (`CONFIG=Debug`), and an optimized sanitizer run (`CONFIG=Asan`, which is `-O3`).
  These are three evaluators with three bug classes and none subsumes another: constant evaluation lowers nothing to a builtin, `-O0` never folds, `-O3` is where the sanitizers have anything to trap and is also the only place a constant-folding defect appears.
  Both of those have already cost this project a defect, in opposite directions — see `docs/verification-matrix.md`.
  `make test-matrix` runs the pair; `make test` alone is the `Asan` leg only.
- **Never leave the order of two side-effecting calls to argument evaluation.**
  `g(f(a), f(b))` does not sequence the two `f` calls, and if effect order is part of a documented contract — as traversal order always is here — then the contract is being left to the optimizer.
  Sequence them into named locals and combine those.
- A test may pin a `scope-decision` divergence; a test must never pin a `defect` (decision D16).

## Final check on the code you just wrote

- No raw loop outside the substrate's own generic algorithms.
- No `switch` or wide `std::visit` doing a job an algebra should do.
- Nothing left non-`constexpr` that could be `constexpr`; nothing `constexpr` that should be `consteval`.
- Every new instance has law tests; every new `constexpr` API has a compile-time test.
- Every file names its component, opens with the prolog, and guards with its path.
