# Handoff to A5

A4 merged into `cl-retire-trees` (`--no-ff`, merge commit `17820fc`, on top of
A3's `03f3781`). `make test-matrix` went from 304 to 310 ctest entries (six
new `TEST_CASE`s: four in `cl_printer_test`, two in
`cl_conformance_test`/`reader_differential.test.cpp`), both legs still
`100% tests passed`. That is the count your own baseline-verify step should
reproduce before you touch anything.

## `sbcl_read_print`'s error reporting: already decided, follow it

Your step file asks "whichever A4 chose, follow it; if A4 did not have to
choose, choose the sentinel." A4 did have to choose, and chose the sentinel,
matching `sbcl_prin1`'s own precedent exactly: `sbcl_read_print` wraps the
read+print in `(handler-case ... (error () (princ "SBCL-ERROR")))`, so a
signalling `read-from-string` — `""`, `")"`, `"(1 2"`, `"'"`, `"("`, all five
of your `ErrorsAgree` inputs — comes back as `std::optional<std::string>`
holding the literal text `"SBCL-ERROR"`, not `nullopt`. `nullopt` is reserved
for the binary being unreachable at all (`run_shell_capture` failing), the
same split `sbcl_prin1` and `find_sbcl_version` already make. Your error
table should check `*theirs == "SBCL-ERROR"` and separately that `cl`'s own
`read` on the same input has `!has_value()`, per your step file's own
instruction not to compare message text.

Manually verified against SBCL 2.2.9.debian while building this: all five
of `""`, `")"`, `"(1 2"`, `"'"`, `"("` do produce `SBCL-ERROR` through this
path, so the sentinel is not just chosen, it is confirmed to work for every
input your error table needs.

## What `reader_differential.test.cpp` looks like right now

`check_against_sbcl(source)` is a single free function in an anonymous
namespace: reads with `cl::reader::read<test_nodes, test_list>`, renders with
`printer::prin1`, `REQUIRE`s both succeeded, and `CHECK`s the rendered text
equals `sbcl_read_print(source)`. It has no error-path variant yet — every
call currently assumes success on both sides. Your error table needs a
second helper (or a branch in this one) that asserts `cl`'s read fails and
that SBCL's answer is the sentinel, rather than comparing rendered text.

`test_nodes = 32`, `test_list = 8`, symbol table `<int, int, int, 64, 512>`.
That per-list capacity of 8 is tight for a broadened corpus — a vector or
list literal with more than 8 elements, or nesting that pushes total node
count past 32, will trip a capacity assert rather than a graceful failure
(these are `add_leaf`/`add_branch` preconditions, not diagnosed errors).
Widen both before writing your forty-to-sixty-case table, not after hitting
the assert.

## Anchors A4 landed

- `d67e6809-36c9-43c2-84cf-14f2d189aa77` — around `prin1` itself in
  `src/smd/cl/printer/prin1.hpp` (the `cata_short` call and its doc comment).
- `8224cb8a-2946-42b1-9018-bf9624f0dcdc` — around `sbcl_read_print` in
  `src/smd/cl/conformance/sbcl_oracle.hpp`.

Neither wraps a "corpus table" or an "error table" — those don't exist yet.
Per D21, place your own anchors around the two tables you add; there is
nothing of A4's to extend or reuse there.

## Durable facts already recorded, by anchor

`docs/compiler_architecture.org` § "The Common Lisp Rebuild: `smd::cl`" has
a subsection "`smd::cl::printer`: a printer arrives with its first oracle,
not ahead of one" covering: the printer is a `cata_short` and why (transcludes
`d67e6809-...`); the provisional `max_print_chars = 512` capacity decision;
and all three oracle traps from your step file (quote-as-list under
`*print-pretty* nil`, empty list as `NIL`, tower spelling vs. SBCL's
evaluated value) plus a fourth, smaller one your step file also names
(`#\Space` prints as `#\` + a literal space on SBCL 2.2.9, not `#\Space`).
Read that subsection by name; it already carries what you'd otherwise have
to re-derive.

## No oracle surprise beyond the ones your own step file already names

All eleven of A4's cases (fixnum, symbol, keyword, string with embedded
quote, character, flat list, nested list, empty list, vector, `'x`, `1+`)
matched SBCL on the first run, including `1+` reading as a symbol. Nothing
outside the three traps (plus the `#\Space` note) turned up.
