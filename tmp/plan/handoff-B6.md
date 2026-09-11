# Handoff to B6

## The primitive set is exactly what B4 left

B5 extended nothing. `src/smd/kit/parser/` is byte-identical to its state at
the end of B4: `pure`, `map`, `satisfy`, `char_p` (`parser.hpp`); `operator|`,
`alt`, `optional` (`choice.hpp`); `skip_many`, `many_until` (`repeat.hpp`);
`bind`/`pure`/`fmap` through the Monad instance (`parser_instances.hpp`, whose
registration shape the typeclass-resync step rewrote — copy it if you register
anything). `many_until` generalised to a second, differently shaped caller
without alteration, which was the open question B5 was placed to answer.

So B6 is where "the primitive set has stopped growing" becomes a claim you can
actually make. Your step file asks you to resolve B2's provisional note one way
or the other; B5 is one data point for "stopped", not yet two.

## `satisfy` cannot serve a caller whose diagnostic is anchored elsewhere

This is the one real finding, and it will reach you if either of your functions
reports at a position it was handed rather than at its cursor.

`satisfy(pred, expected)` fails at `cur.position()`, always. `read_string`'s
two diagnostics and `read_character`'s `"expected character after #\"` all
report at `where` — the opening quote, or the `#` — which is behind the cursor
by the whole literal. So B5's two "one character" parsers are written out as
`parser{...}` lambdas rather than built from `satisfy`, and both keep the
position property `operator|` needs: fail *at* the start cursor to fall
through, fail *past* it to commit.

`satisfy`'s predicate is also context-free — it takes a `char` and never sees
the threaded context — so a readtable query (`table.macro_of`,
`table.syntax_of`) cannot be a `satisfy` predicate either without capturing
`Ctx&`, which D29 forbids. Both limits are one extension away (a position
parameter; a predicate overload taking the context), and neither was worth
making for a single caller. If B6 or B7 is the second caller, that is the
amendment — extend `satisfy` in place, do not write a parallel primitive.

## How B5 expressed "any character that is not X", since you were told to ask

It did not, and that is the answer. The stopping condition belongs to the
`many_until` step, tested once ahead of the alternatives, exactly as
`read_delimited` tests for its closing delimiter. The consequence is that
`read_string`'s second alternative is plain **any character**, not "any
character that is not a quote": testing for the quote in both places would put
the literal's termination rule where two sites have to agree, and the copy
inside the alternative could never fire. If you reach for a negated-character
parser, check first whether the thing you are negating is really your step's
stop condition.

## Answers to the two questions B4 left open

- **Zero-consumption step.** `read_string`'s step cannot succeed at zero width:
  every success of its character parser consumes at least one character, and
  the only zero-width outcome is the `std::nullopt` stop at the closing quote,
  which ends the repetition anyway. `many_until`'s guard is still unexercised
  by any production caller.
- **A consumed-span-returning repetition.** No evidence for or against.
  `read_character` has no repetition in it at all — one `bind`, and
  `advance_while` already hands back the end cursor a span is measured to. The
  name's span is still offset arithmetic on the outer cursor, captured into the
  `bind`'s continuation because a name is measured from the first character and
  the continuation's own cursor is already past it.

## `reader_context` is unchanged and nothing in it was missing

`read_string` and `read_character` are now constrained with it at the function
template level, joining `read_radix_number`. Note that `read_delimited` is
**not**: its own `Ctx` is still a bare `class Ctx`, and only its inner step
closure says `reader_context auto &rc`. If your step file's phrasing suggested
otherwise, that is why.

`read_context.hpp`'s doc comment on the concept still says "Only
`read_radix_number` is constrained with this concept in this step". That went
stale at B3 and is staler now. It is nobody's declared scope; B8 is the
integration step and is the natural owner.

## Two things about your own spot checks that are wrong as written

Both are transcription defects in the step files, not in the code.

1. **The binary path glob.** `./.build/*/*/cl_reader_test` resolves nothing.
   The real paths are
   `.build/build-gcc-16/src/smd/cl/reader/{Asan,Debug}/cl_reader_test` and
   `.build/build-gcc-16/src/smd/cl/conformance/{Asan,Debug}/cl_conformance_test`.
2. **The Catch2 filters.** There are no test cases named `*Symbol*`,
   `*Keyword*` or `*Quote*`. `read.test.cpp`'s Catch2 cases are
   `ReadTest - {HeaderIsIdempotent, Atoms, Interning, TowerSpellings,
   CompoundData, IntertokenSpace, SequentialReads, Errors,
   TraverseOverReadData}`. Symbols, keywords and quote forms live inside
   `ReadTest - Atoms` and `ReadTest - CompoundData`. Run the binary with no
   filter; it is 30 cases and under a second.

## The differential your step file leans on will skip

`sbcl` is not on `PATH` in this environment, and `cl_conformance_test` prints
`sbcl not found on PATH; differential check skipped` and skips all four
differential cases — `ReaderDifferentialTest - {ReadAndPrintAgreeWithSbcl,
ErrorsAgreeWithSbcl}` and `SbclDifferentialTest - {AnsiTestAdaptedCases,
SpecDerivedCases}` — in both legs. They are counted as passing by ctest.

Your step file calls that differential "the one that matters most" and "the
only check in this repository that DIV-0003 holds against something other than
this project's own opinion". It will not run unless somebody installs SBCL
first. Either install it before you start, or say plainly in your commit
message and handoff that DIV-0003 was checked against `read.test.cpp`'s
`whole_token_classification` (which pins `1+`, `2buffer`, `1.5x` by name) and
nothing else. Do not report the differential as green because ctest was.

## Out-of-scope file B5 touched

`src/smd/cl/reader/read.test.cpp`, additively. Three new pins, no existing
expectation changed: `string_capacity_boundary` (both sides of the
`max_string_chars` boundary and the overflow's reported position), and two new
conjuncts in `reports_errors` — a literal ending in a bare backslash, and
`read_character`'s end-of-input diagnostic. B5's own step file forbade touching
`.test.cpp` under `src/smd/cl/` in its spot checks while its Setup section
called those pins the deliverable; the Setup section and the standing "adding a
case is fine" rule won. Your spot check reads `cl-parser-combinators..HEAD`, so
B5's change is behind your base and will not appear in your diff.

Your own `read_token_datum` conversion has the same hazard B5 had: read
DIV-0003 and check what `whole_token_classification` actually pins before you
assume a green matrix is evidence.

## One counting gotcha

Adding a `CHECK` to an existing `TEST_CASE` does not change the ctest entry
count. B5 added three assertions and the count stayed where typeclass-resync
left it, which its own step file had predicted would rise. If you add cases,
predict the count from the number of new `TEST_CASE`s, not new assertions.
