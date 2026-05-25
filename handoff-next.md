# Next step: Step 16 (bottom-up CPS direction decision)

## Goal

Decide whether the bottom-up CPS compiler for step 17 will be
**structural** (recursing directly over `core_tree` nodes) or
**Fix<F>-based** (using `fold_fix` over the expression fixpoint type
from step 14).  Produce a design-spike file `docs/cps-direction.md`
that records the decision, the tradeoffs considered, and the sketch of
the chosen approach.  No new production headers or test files are added
in step 16.

## Files expected to change

```txt
docs/cps-direction.md          (new — design spike and decision record)
checklist.md                   (mark step done)
handoff-next.md                (update for step 17)
```

No `src/` files change in step 16.

## Context from previous steps

Steps 11–15 completed:

- `value.hpp`: `enum class builtin_op { add, multiply }`, `struct builtin`,
  `using value = std::variant<int, bool, builtin>`,
  `template <int MaxBindings> class env`,
  `template <int MaxBindings> constexpr auto default_env() -> env<MaxBindings>`
- `eval_direct.hpp`:
  `template <int MaxNodes, int MaxList, int MaxBindings>`
  `constexpr auto eval_direct(core_tree<MaxNodes, MaxList> const&, node_id, env<MaxBindings> const&) -> result<value>`
- `cps.hpp`:
  `template <class F> struct cps_code { F f; operator()(Env, K) }`,
  CTAD guide `cps_code(F) -> cps_code<F>`,
  `template <int MaxNodes, int MaxList>`
  `constexpr auto compile_cps(core_tree<MaxNodes, MaxList> const&, node_id) -> cps_code<...>`
  (interpreter-backed: wraps `eval_direct` in a CPS closure)
- `elaborator.hpp`: all core types, `constexpr auto elaborate(...) -> result<core_tree<...>>`
- `reader.hpp`: `read_datum<MaxNodes, MaxList>(cursor) -> parse_result<datum_tree<...>>`
- `fix.hpp`: `template <template <class> class F> class fix`,
  `template <class R, template <class> class F, class Algebra>`
  `constexpr auto fold_fix(fix<F> const&, Algebra) -> R`

111 tests pass. `make compile`, `make test`, `make lint` are all green.

## Key design question for step 16

`core_tree<MaxNodes, MaxList>` is a **flat arena**: nodes are stored in a
`static_vector<core_node<MaxList>, MaxNodes>` and referenced by `node_id`
(integer index).  It is NOT a recursive data type.  `fix<F>` requires a
recursive type.

Therefore the bottom-up CPS compiler has two options:

### Option A — structural recursion over the flat arena

Walk `core_tree` directly.  A recursive function `cps_of(core_tree&, node_id, continuation)`
dispatches on the node variant and recurses via `node_id`.  The continuation
is passed as a template parameter (or `auto`) to avoid erasing it.

**Pros:** No additional data structure.  Directly composes with the
arena that elaborate already produces.

**Cons:** Every call to `cps_of` instantiates a new template from the
continuation type, potentially creating a chain of nested types as deep
as the tree.  For a tree of depth D, the continuation type at depth 0
wraps the continuation at depth 1, which wraps depth D.  Type depth is
O(D).  GCC can handle this for small D (arithmetic only, D <= ~4), but
it may become expensive for deeper expressions.

### Option B — Fix<F>-based fold

Convert `core_tree` to `fix<expr_f>` first (a separate pass), then apply
`fold_fix` with a CPS algebra.  Step 14's `fix.hpp` already provides
`fold_fix`.

**Pros:** Clean algebraic structure.  The algebra is a single function.
Well suited to the step-14 playground.

**Cons:** Requires a conversion pass from the flat arena to a recursive
`fix<expr_f>` type.  The `fix<F>` node uses `new`/`delete` so it is not
a literal type and cannot appear in `static_assert` constexpr tests.
This breaks the existing compile-time test discipline.

### Recommended decision

**Option A** (structural recursion over the flat arena) for steps 17-18.
The tree depth for the arithmetic-only phase is at most 3-4 levels, which
is safe for nested template continuation types.  `fix<F>` is not a literal
type and would lose the `static_assert` test discipline that the project
depends on.  The `fix<F>` playground (step 14) remains a standalone
experiment.

The `docs/cps-direction.md` should record this decision, sketch the
`cps_of` signature for step 17, and note when/if `fix<F>` becomes viable
(e.g., once `constexpr new` allocation lands and `fix<F>` can be literal).

## Safety notes for the next Claude

**Compilation memory:**
The previous Claude session OOMed the machine compiling step 15.  The
root causes were template/type errors in the test file combined with ninja
running 20 parallel GCC jobs.  The safe compilation incantation is:

```bash
make compile
```

The build is now clean and incremental builds are fast.  If you are doing
a from-scratch build (e.g., after a CMake reconfigure), limit parallelism:

```bash
cmake --build .build/build-gcc-16 --config Asan --target all -- -k 0 -j4
```

Do NOT use `ulimit -v` for the build -- ASan requires ~15 TB of virtual
address space and fails immediately under `ulimit -v`.

**Test pattern discipline:**
All `static_assert` tests must use the immediately-invoked lambda pattern:

```cpp
static_assert([] {
    // local auto variables, not constexpr auto
    ...
    return bool_result;
}());
```

Do NOT use `constexpr auto` for large intermediate objects (`core_tree`,
`cps_code`) at namespace or function scope.  GCC's constexpr evaluator
tracks full copies of these large objects; at 17 KB per `core_tree<32,16>`
with 20 parallel jobs the machine will OOM.

**Root access:**
`core_tree<N,M>` has no `.root()` method.  Use `ct.size() - 1` for the
root node index.

**Template parameters:**
`compile_cps` has only `<int MaxNodes, int MaxList>` -- no `MaxBindings`
parameter.  The env type is deduced by the lambda's `auto const& env`.

## Do not do

- Do not write any `src/` production headers or test files in step 16.
- Do not implement the bottom-up CPS compiler (that is step 17).
- Do not add lambda support (step 20+).
- Do not proceed to step 17 in the same commit.
