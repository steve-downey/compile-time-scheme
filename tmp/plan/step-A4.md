# Step A4 — a `prin1`-shaped printer for `cl` datums, proved against SBCL

## Project context

`compile-time-scheme` is a compile-time compiler proof of concept in C++26 on
GCC16. `src/smd/cl/` is the live tree: a Common Lisp front end whose reader
builds a `datum_tree`, elaborated to a core tree and run by a small-step
machine. `src/smd/kit/foundation/` is the shared constexpr substrate. As of
A1–A3, `src/smd/cl/` and `src/smd/kit/` are the only trees in `src/smd/`
besides `fixpoint`; `smdscheme` and `smdlisp` are gone from the worktree and
live only at `iteration/smdscheme-final` and `iteration/smdlisp-final`.

**Integration branch: `cl-retire-trees`** (exists, branched off `main`).

**Reserved for this step:** blog phase **37**, divergence number **DIV-0033**.
An unused reservation is fine.

## Why, and why it runs fourth rather than first

`src/smd/cl/` has no printer. Grepping the tree for `prin1`, `print_datum` or
`to_string` finds only `conformance/sbcl_oracle.hpp`, which prints on SBCL's
side of a comparison, never on ours. Until A2 retired it, the reader's only
differential evidence was `reader/oracle_compare.test.cpp` — a structural
comparison against `smdlisp`, another implementation in this same
repository, written by the same hand, which decision D16 is explicit is not
what external authority means. A3 has since deleted that comparison's other
half along with the rest of `smdlisp`. Right now, between A3 and this step,
the reader has **no** differential oracle at all — that is a real, temporary
gap this plan opened deliberately, on the owner's direction that removing the
dead trees mattered more than sequencing a replacement first. This step is
where the gap starts closing.

A printer changes what is possible here. Once a datum renders as text, SBCL's
`prin1` becomes available as a reader oracle, and D16's "differential oracle
running a reference implementation" applies to the reader and not only to the
evaluator.

A5 builds the differential out into a real corpus. This step lands the
printer **and its first real oracle comparison together**, deliberately: a
printer whose only witness is a test written against its own output is
exactly the evidence D16 rejects, and a wrong guess about the rendering
should surface here rather than one step later.

## Setup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree add ../step-a4-cl-printer -b step-a4-cl-printer cl-retire-trees
cd ../step-a4-cl-printer
git submodule update --init --recursive
```

A fresh worktree has no submodules and no build directory. The submodule init
is required — `vendor/execution` and `vendor/task` are `add_subdirectory`
dependencies and CMake will not configure without them.

## Verify GREEN baseline

Your inbound handoff carries the exact ctest count and cold `make test-matrix`
wall time measured after A3's deletion; both are much lower than the
pre-Phase-A figures.

```sh
date +%s
make test-matrix > /tmp/verify-A4-base.log 2>&1; echo "exit=$?"
date +%s
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-A4-base.log
wc -c /tmp/verify-A4-base.log
```

Expected: exit 0, and two `100% tests passed` lines at the post-deletion
count your handoff names. If the baseline is not green, stop and write
`blocked-A4.md`.

## What already exists that this step builds on

- `src/smd/cl/reader/datum.hpp` — `datum_atom` is a `std::variant` of
  `datum_fixnum`, `datum_symbol`, `datum_keyword`, `datum_character`,
  `datum_string`, `datum_tower`; `datum_branch` is an enum of `list`,
  `vector`, `quote`, `function`, `backquote`, `unquote`, `unquote_splice`;
  `datum_tree<MaxNodes, MaxList>` is a `foundation::tagged_tree` over the
  two. The anchor `23ee18c4-af42-42b1-b97a-3513b932e5ef` covers both
  declarations.
- `src/smd/cl/foundation/tagged_tree_schemes.hpp` — `cata` (anchor
  `11011910-7c9b-42e5-bc50-53bbfd3242d9`) and `cata_short`. `cata_short`'s
  algebra is `result<A>(node_f<Leaf, Tag, A, MaxChildren> const&)`, the
  leftmost failure wins, and nothing past it is visited.
- `src/smd/cl/foundation/static_vector.hpp` — a forwarding shim onto
  `smd::kit::foundation::static_vector`, with `push_back`, `append_range`,
  `size`, `capacity`.
- `src/smd/cl/symbol/symbol_table.hpp` — `name(symbol_id)` returns the
  interned name. Symbol names are already upcased by the reader
  (`token.hpp`'s `to_upper_char`, decision DIV-0001), and a keyword's name is
  interned *with* its leading colon.
- `src/smd/cl/conformance/sbcl_oracle.hpp` — `find_sbcl_version()` and
  `sbcl_prin1(form)`, both `inline`, both runtime-only. Anchors
  `29a14fe5-7268-446e-9489-7ec8df52494c` and
  `1e9c6a05-3f50-4a61-8552-e879e2053d2d`. This header is unaffected by A1–A3.
- `docs/compiler_architecture.org` § "The Common Lisp Rebuild: =smd::cl=" is
  where this step records its durable facts.

## The change

### 1. A new component, `src/smd/cl/printer/prin1.hpp`

Namespace `smd::cl::printer`. Header-only, `constexpr`, guarded
`SRC_SMD_CL_PRINTER_PRIN1_HPP`, with the canonical prolog.

**It is a catamorphism.** `docs/cpp-rules.md` is not negotiable here: "a
recursion over a tree is a catamorphism … not by hand-written recursive
descent." Use `foundation::cata_short`, whose carrier is the rendered text
and whose failure channel is buffer overflow. A hand-written recursive
`print_node` is a defect in this repository, not a style preference.

Suggested shape — the carrier is a fixed-capacity character buffer, so the
algebra concatenates children's already-rendered text:

```cpp
/// Maximum characters one rendered datum may occupy.
inline constexpr int max_print_chars = 512;

using print_text = foundation::static_vector<char, max_print_chars>;

template <int MaxNodes, int MaxList, class SymbolTable>
[[nodiscard]] constexpr auto prin1(reader::datum_tree<MaxNodes, MaxList> const &tree,
                                   SymbolTable const &symbols)
    -> foundation::result<print_text>;
```

Note the cost this shape carries: `cata_short` materialises one carrier per
node, so the fold holds `MaxNodes * max_print_chars` bytes. At the test
sizes below that is 32 KB and fine. **Record this as a provisional decision**
in the architecture doc — what was actually needed when it was made, and
that a consumer wanting a 256-node tree at full width is what would justify
revisiting it. An abstraction nobody dared touch and one nobody needed to
touch look identical from outside.

The algebra dispatches the leaf variant with `std::visit` and the branch tag
with a `switch`. That is allowed: `std::visit` "may dispatch the
alternatives of a single node inside an algebra; it must not drive the
recursion."

### 2. The rendering, and what is deliberately not in it

**In scope.** These are pinned by the oracle in part 3 and by unit tests:

| datum | rendering | note |
|---|---|---|
| `datum_fixnum` | decimal digits, `-` prefix when negative | |
| `datum_symbol` | the interned name verbatim | already upcased |
| `datum_keyword` | the interned name verbatim | already carries the colon |
| `datum_string` | `"…"`, with `\` before `"` and before `\` | |
| `datum_character` | `#\` then the character | see the trap below |
| `datum_tower` | the stored spelling | see the trap below |
| `list`, non-empty | `(` elements separated by one space `)` | |
| `list`, empty | `NIL` | **not** `()`; SBCL prints `NIL` |
| `vector` | `#(` elements separated by one space `)` | |
| `quote` | `(QUOTE ` child `)` | see the trap below |
| `function` | `(FUNCTION ` child `)` | |

**Out of scope, and say so in the header's doc comment.** A full ANSI
printer is not wanted; a canonical structural rendering sufficient for
differential comparison is:

- `|…|` multiple-escape quoting of symbol names. `cl` upcases unescaped names
  (DIV-0001) so the common case round-trips; a name needing escapes does not,
  and the printer does not pretend otherwise.
- `*print-circle*`, `*print-level*`, `*print-length*`, packages, readtable
  case other than upcase, and pretty-printer layout.
- `backquote`, `unquote`, `unquote_splice`. Render them as `` ` ``, `,` and
  `,@` prefixes so the printer is total, but they are **excluded from every
  oracle comparison**: SBCL 2.2.9 prints these as `SB-INT:QUASIQUOTE` and
  `#S(SB-IMPL::COMMA …)`, which is implementation-specific and which ANSI
  permits. Excluding them is principled, not a dodge.

### 3. Three traps, all confirmed against SBCL 2.2.9.debian on this machine

These were measured, not guessed. Getting them wrong is the likely failure
of this step.

**Quote is a list, not an apostrophe, unless the pretty printer is on.**
With `*print-pretty*` nil, SBCL prints `'x` as `(QUOTE X)` and `#'car` as
`(FUNCTION CAR)`. With it on, `'X` and `#'CAR`. Pin `*print-pretty*` to
`nil` in the oracle call and render the list forms; that is the
ANSI-defined reading of the syntax and does not depend on a printer
variable's default.

**The empty list prints as `NIL`.** `(prin1 (read-from-string "()"))` gives
`NIL`. A `list` branch with zero children renders `NIL`.

**A tower renders its spelling, and SBCL renders a value.** `datum_tower`
stores the token's spelling and radix (decision D19: readable before
executable). SBCL prints `#x1f` as `31`, `2/4` as `1/2` and `1.50` as `1.5`.
The printer is right to print the spelling and the oracle is right to print
the value; they simply do not agree except on canonical decimal spellings.
So the comparison corpus uses only canonical decimal towers, and A5's step
file carries that restriction forward. Do not "fix" the printer to
normalise — normalising is the evaluator's job and this reader has no
numeric tower yet.

One more, smaller: SBCL 2.2.9 prints the space character as `#\` followed by
a literal space, not `#\Space`, while `#\Newline` and `#\Tab` do print by
name. Keep `#\Space` out of the compared corpus and note it; matching one
SBCL build's character-name table is not what this printer is for.

### 4. `src/smd/cl/printer/CMakeLists.txt`

Model it on `src/smd/cl/reader/CMakeLists.txt`. An `INTERFACE` library
`cl.printer` with a `FILE_SET HEADERS` named `cl_printer_headers`, linking
`cl.foundation cl.reader cl.symbol`; a `cl_printer_test` executable under
`if(SCHEMEPOC_ENABLE_TESTING)` with `catch_discover_tests`. Add
`add_subdirectory(printer)` to `src/smd/cl/CMakeLists.txt` after `reader`.

`cl.printer` is **not** added to the top-level `beman_install_library`
export list. A2 shrank that list to `smd.fixpoint` alone and exporting the
live trees is `docs/backlog/BL-0005-…`, scheduled after Phase B — this step
builds and tests `cl.printer` in the normal tree like everything else, it
does not install it.

### 5. `src/smd/cl/printer/prin1.test.cpp`

Write these **first**. Double-include the header, then the bootstrap
`REQUIRE(true)` test, per `docs/cpp-rules.md`. Then `static_assert`-backed
`constexpr` cases for every row of the table above, plus buffer-overflow
diagnosis. `constexpr` contracts get compile-time tests alongside the
runtime ones — both, not either.

### 6. The first oracle comparison

Add to `src/smd/cl/conformance/sbcl_oracle.hpp` one new `inline` function
beside the existing two — **do not modify** `sbcl_prin1` or
`find_sbcl_version`, which A5's corpus differential will use too:

```cpp
/// Reads @p source with SBCL's own reader and returns what `prin1` prints for
/// the resulting object, with the pretty printer off so the quote family
/// renders as the list forms ANSI defines rather than as reader syntax.
[[nodiscard]] inline auto sbcl_read_print(std::string_view source)
    -> std::optional<std::string>;
```

It builds `(let ((*print-pretty* nil)) (prin1 (read-from-string "…")))`,
escaping `source` for both the shell (reuse `detail::shell_quote`) and the
Lisp string literal (backslash and double quote), and runs it through
`detail::run_shell_capture` exactly as `sbcl_prin1` does.

Then add `src/smd/cl/conformance/reader_differential.test.cpp`: for **eight
to twelve** source strings, read with `cl::reader::read`, render with
`printer::prin1`, and compare against `sbcl_read_print` of the same string.
Cover a fixnum, a symbol, a keyword, a string with an embedded quote, a
character, a flat list, a nested list, the empty list, a vector, `'x`, and
`1+` — the last is DIV-0003's whole-token classification, and this is the
first time an outside implementation has ever confirmed it.

**It must SKIP, not fail, when SBCL is absent.** Guard every test case on
`find_sbcl_version()` and call Catch2's `SKIP` exactly as
`conformance/sbcl_differential.test.cpp` does. A missing oracle is a fact
about the environment, not a defect, and this test is a member of the
default suite. SBCL 2.2.9.debian is on `PATH` in this environment, so you
will see it run.

Add the new test to `cl_conformance_test`'s `target_sources`, and add
`cl.printer` to `cl.conformance`'s `target_link_libraries`.

### 7. Anchors and the architecture doc

Land `uuidgen` anchors around the printer's algebra and around
`sbcl_read_print`, and name them in your handoff. Add a subsection under
`docs/compiler_architecture.org` § "The Common Lisp Rebuild: =smd::cl=" that
records, in place: that the printer is a `cata_short` and why; the
carrier-size decision, marked provisional as above; and the three traps in
part 3, which are durable facts about the oracle rather than about this
step.

## Declared file scope

```
src/smd/cl/printer/prin1.hpp                             (new)
src/smd/cl/printer/prin1.test.cpp                        (new)
src/smd/cl/printer/CMakeLists.txt                        (new)
src/smd/cl/CMakeLists.txt                                (add_subdirectory)
src/smd/cl/conformance/sbcl_oracle.hpp                   (one new function)
src/smd/cl/conformance/reader_differential.test.cpp      (new)
src/smd/cl/conformance/CMakeLists.txt                    (link + test source)
docs/compiler_architecture.org                           (new subsection)
```

## Verify GREEN after

```sh
make test-matrix > /tmp/verify-A4-after.log 2>&1; echo "exit=$?"
grep -E '=== test-matrix|tests passed|Total Test time' /tmp/verify-A4-after.log
wc -c /tmp/verify-A4-after.log
make lint
./scripts/verify-transclusions.sh
```

Both legs green, with a test count **above** your inbound baseline (you added
tests and removed none). `make lint` and the transclusion check both exit 0.

## Spot checks

```sh
grep -rn "cata_short\|cata<" src/smd/cl/printer/prin1.hpp     # the fold, not recursion
grep -rn "print_node\|recurse" src/smd/cl/printer/prin1.hpp   # must find nothing
grep -c "static_assert" src/smd/cl/printer/prin1.test.cpp     # compile-time cases exist
grep -n "SKIP" src/smd/cl/conformance/reader_differential.test.cpp
./.build/*/*/cl_conformance_test "*ReaderDifferential*" 2>/dev/null | tail -5
```

The last one should report the SBCL version it ran against, not a skip.

## Commit and merge back

```sh
git add -A
git commit -F - <<'EOF'
cl: a printer, so the reader can face an outside oracle

A3 deleted smdlisp along with reader/oracle_compare.test.cpp's other
half, and between A3 and this step the reader had no differential
oracle at all -- a gap this plan opened on purpose, because the owner
weighed removing the dead trees above sequencing their replacement
first.

printer::prin1 is a cata_short over the datum tree, not a recursive
descent: the carrier is the rendered text and the failure channel is
buffer overflow. It renders the subset a differential needs and says
in its own doc comment what it does not render, because a full ANSI
printer is a different and much larger thing.

It arrives with its first oracle comparison rather than ahead of one.
A printer whose only witness is a test written against its own output
is the evidence D16 rejects, so sbcl_read_print lands beside it and
eleven source strings are read, printed and compared against SBCL
2.2.9.

Three of SBCL's answers were surprising enough to record in the
architecture doc: the quote family prints as (QUOTE X) once the
pretty printer is off, the empty list prints as NIL, and a tower
prints its value where this reader prints its spelling -- so the
compared corpus holds canonical decimal spellings only.
EOF

git checkout cl-retire-trees
git merge --no-ff step-a4-cl-printer
```

## Record measurements

After the merge, before cleanup — the numbers and the worktree still exist.

```sh
cat >> /home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/metrics.jsonl <<EOF
{"step":"A4","lane":null,"outcome":"green","wall_seconds":<end-start>,"attempts":<n>,
 "verify":{"command":"make test-matrix","exit_code":0,"wall_seconds":<measured>,
 "log_bytes":$(wc -c < /tmp/verify-A4-after.log),"summary_lines_read":<n>},
 "diff":$(git diff --shortstat <A3 merge commit>..HEAD | \
   awk '{printf "{\"files_changed\":%s,\"insertions\":%s,\"deletions\":%s}",$1,$4,$6}'),
 "out_of_scope":[],"note":""}
EOF
```

One line, valid JSON — collapse the whitespace. Substitute real measured
numbers; do not estimate any of them.

## Cleanup

```sh
cd /home/sdowney/src/steve-downey/compile-time-scheme/main
git worktree remove ../step-a4-cl-printer
```

Mark A4 done in
`/home/sdowney/src/steve-downey/compile-time-scheme/main/tmp/plan/checklist.md`.

## Handoff

Read `tmp/plan/step-A5.md`, then write `tmp/plan/handoff-A5.md` (≤ ~150
lines). A5 builds the reader differential out into a real corpus and records
what `conformance/sbcl_differential.test.cpp` actually validates. Tell it:
the exact signature and semantics of `sbcl_read_print`; the anchor names you
landed; every SBCL answer that surprised you beyond the three above; and
anything about `printer::prin1`'s shape that A5 would otherwise have to
rediscover. Reference architecture facts by anchor rather than restating
them.
