# Step brief: L22 lane (smdlisp public API)

Forward-only handoff for the next agent working the `smdlisp` public-API
surface. Read `docs/codestyle.org`, `AGENTS.md`, `docs/CODING_RULES.md`,
`CLAUDE.md`, and this file. Nothing else.

## Where this lane stands

`src/smd/smdlisp/smdlisp.hpp` exists and is the package header for the Common
Lisp front end. It provides `smd::smdlisp::source_literal`,
`smd::smdlisp::lisp_program<Source, MaxNodes, MaxList, MaxBindings, MaxEnvs>`,
and `smd::smdlisp::compiled_lisp<Source>`. Two Godbolt-extractable examples
consume it: `src/examples/godbolt_lisp.cpp` and
`src/examples/godbolt_lisp_ffi.cpp`, both wired into CTest.

## Next step's goal

L22 has no successor inside this lane. The next thing that touches this
surface is **L24**, the consolidation of the Common Lisp layer into
`docs/compiler_architecture.org`. Until L24 runs, that document deliberately
has no `smdlisp` section (its own introduction says so); durable `smdlisp`
facts live in code doc comments and in `docs/divergences/`. Three separate
workers have now re-litigated this. Do not add an `smdlisp` section early.

Merge criterion for anything further here: `make compile`, `make test`,
`make lint` green, plus `make compile-headers` if you touch the header set.

## Files this lane owns

```txt
src/smd/smdlisp/smdlisp.hpp
src/smd/smdlisp/smdlisp.test.cpp
src/smd/smdlisp/CMakeLists.txt      (top level only)
src/examples/godbolt_lisp.cpp
src/examples/godbolt_lisp_ffi.cpp
src/examples/CMakeLists.txt
```

Everything under `src/smd/smdlisp/{reader,elaborator,closure,macroexpand}/`
belongs to other lanes. L22 consumed all of it through canonical angle
includes and existing CMake targets without needing a single edit; keep it
that way.

## What this step discovered that you will need

**1. `lisp_program` owns the datum arena, and that is why it exists.**
`closure::compile_to_closure` takes a caller-owned datum arena that must
outlive the program, because elaborated core nodes hold plain
`std::string_view` fields viewing the datum atoms their names were read from
(DIV-0007). A variable template alone has nowhere to put that arena. So
`lisp_program` declares `datum_arena_` *before* `program_`, which guarantees
it is initialized first, and compiles into it from the constructor. Every
`string_view` in the compiled program therefore points into a subobject of
the same object.

Consequence: `lisp_program` is neither copyable nor movable, and the copy
operations are deleted to say so. `inline constexpr auto compiled_lisp =
lisp_program<Source>{};` still works because guaranteed copy elision means
the prvalue initializes the variable directly. If you ever find yourself
wanting to return a `lisp_program` by value, you want a different design, not
a defaulted move constructor.

**2. Constexpr evaluation through `compiled_lisp` is limited under UBSan --
DIV-0013.** In the default `Asan` config, a `static_assert` that *evaluates*
a closure-building program (`lambda`, `function`, `defun`) through
`compiled_lisp<Source>` fails to compile: GCC 16 under `-fsanitize=null`
refuses to fold `cps_code.hpp`'s `clo.node == nullptr` guard when the pointer
targets a subobject of a namespace-scope constexpr object. Write the
constexpr twin against a function-local `lisp_program<Source>{}` and let the
runtime `TEST_CASE` cover `compiled_lisp`. Nothing is wrong at run time.
`docs/divergences/DIV-0013-*.md` has a five-line reproduction.

**3. `source_literal` is duplicated, not shared -- DIV-0012.** The Scheme
original lives in the frozen package header `smd/smdscheme/smdscheme.hpp`,
inside UUID anchor `44cc988c-7353-43aa-a7d3-8840f92371a6`, and aliasing it
would put `smdscheme.closure` (and its INTERFACE `-freflection`) on every
`smdlisp` consumer. The two types are distinct and must stay in sync by hand.

**4. `compile_to_closure` does not run the macro expander.** A source
containing `defmacro` is a hard compile error through `compiled_lisp` --
`result::value()` is `std::get` on the error alternative, which throws, and a
throw is not a constant expression. `macroexpand/expander.hpp` is a separate
pass that nothing in the public API currently chains. If a future step wants
`compiled_lisp` to accept macros, the chaining belongs in
`closure/closure_program.hpp` (another lane's file), not in a second
`compiled_lisp`-shaped wrapper here.

**5. Environment construction is the caller's job, and there are three
overloads.** `closure::default_env<Core, 16>()` is enough for arithmetic and
`defvar`. Add a `pair_heap` for anything that conses (including quoted list
literals). Add a `store` as well for `setq` on an existing binding. The
nested aliases `lisp_program::pair_heap_type` and `::store_type` exist so
examples do not have to spell `closure::default_max_pairs`. Note that `setq`
on an *unbound* name still fails -- bind with `defvar` first.

**6. Recursive `defun` remains unsupported (DIV-0009).** Neither example
recurses. Do not write one that does.

## UUID anchors created by this step

Prose that a blog post may transclude:

```txt
54b5ce61-30e2-448d-b830-933d229b999e  smdlisp.hpp        source_literal
fe65137c-94d0-4c11-bc1f-b0dae61e2190  smdlisp.hpp        lisp_program + compiled_lisp
05b6b13a-69a8-4324-a056-2d754a038802  godbolt_lisp.cpp   two-namespace demo
89fd04b3-d44b-4bcb-86f9-048297a36b7a  godbolt_lisp_ffi.cpp  the foreign_function
c19f8ea7-df86-41a3-9586-ca2daf90d093  godbolt_lisp_ffi.cpp  injecting it into the env
```

No blog post was written for L22. Per repository policy any future post must
pin its transclusions to a tag, not to a branch tip.
