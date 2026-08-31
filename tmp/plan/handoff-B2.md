# Handoff to B2

## Where things landed after B1's split

`read_radix_number` and `read_sharpsign` are both in
`src/smd/cl/reader/detail/sharpsign.hpp`, in that order (matches B2's step
file). `read_radix_number` is the first function in the file, right after the
prolog and includes.

`src/smd/cl/reader/detail/read_context.hpp` (new in B1) already carries, at
`smd::cl::reader::detail` scope: `using foundation::and_then;`, the
`read_context<SymbolTable, MaxNodes, MaxList>` template (members `tree`,
`symbols`, `table`; nested `child_list` alias), `add_leaf_checked<Ctx>`,
`add_branch_checked<Ctx>`, and `using symbol::intern_checked;`. `Ctx` is
still fully duck-typed here — no named concept exists yet. `default_max_nodes`
and `default_max_list` are in the same file but at the enclosing
`smd::cl::reader` scope, not `detail`, since B1's step file was explicit that
moving them into `detail` would change their qualified name. This is the file
B2's step file names for the new `reader_context` concept, `token_p`, and the
`using bind` declaration — nothing here blocks any of that; the file has no
`reader_context` concept, no `token_p`, no `bind` name yet, so B2 is adding to
a clean spot rather than reconciling with something already there.

`src/smd/cl/reader/cursor.hpp` is unchanged from before B1 — B1's declared
scope didn't touch it, and it wasn't touched. It's small (114 lines): `cursor`,
`parse_state<T>`, `advance_while`, including only
`<smd/cl/foundation/source_pos.hpp>`, `<cassert>`, `<string_view>`. B2's step
file's description of it as "already free of anything language-specific" is
accurate as verified during B1's read-through.

## A build-tooling gotcha, not a design one

`make lint`'s `clang-format` pass re-sorts a header's `#include` block
alphabetically regardless of any hand-authored grouping or ordering comment,
and — per the orchestrator's standing note — exits non-zero on the run that
does the rewriting, then passes clean on the next run over the same files.
B1 hit this on `read.hpp`'s umbrella include list: a "these are listed in
dependency order" comment became false the moment clang-format ran, because
it doesn't reorder comments to match. Rewrote the comment to describe the
mechanism (which pair is mutually recursive and why a forward declaration
fixes it) rather than claim a specific list order, since order among
self-contained, guard-protected headers doesn't matter anyway. If B2 writes
an explanatory comment above any include block in the new `kit::parser`
files, word it the same way — don't describe the list's order, describe why
the files exist.

## Anchors already in the tree from B1

`b803edcd-959d-4364-94f8-13fb256e7b9b` — around `read_node`'s definition,
now in `detail/node.hpp` (unchanged position, just a new file).
`8a2232b6-6436-4943-b35b-69e6df6faaa2` — around the `read_node` forward
declaration in `detail/read_node_fwd.hpp`.
`e6e0bb21-34e0-44cb-92ef-79f8580a1581` — around the umbrella's include list
in `read.hpp`. None of these three needs anything from B2; named here only so
B2's own new anchors (reserved: blog phase 40, DIV-0036) don't get confused
for pre-existing ones while skimming `git diff`.

## Call-site counts, unchanged

`scan_token` is called twice across the reader (once in `sharpsign.hpp` for
`read_radix_number`, once in `token_datum.hpp` for `read_token_datum`);
`classify_number` likewise twice, same two files. Both are out of scope for
all of Phase B per the standing constraint; B1 didn't touch either, and B2
converting `read_radix_number` onto `bind` should leave the `scan_token` call
itself untouched — only what wraps it changes.

## Architecture doc

B1's section is "The Common Lisp Rebuild: `smd::cl`" § "B1: the reader splits
into `detail/` headers, so later steps have somewhere to stand" in
`docs/compiler_architecture.org` (end of file, after the A5 section). It
records the eight-header layout and the forward-declaration mechanism by
anchor; B2's own new section for `smd::kit::parser` is additive after it, not
a replacement.
