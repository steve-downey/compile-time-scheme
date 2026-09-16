# BL-0009: whether `alternative` still belongs in the kit

- **Status:** open
- **Date:** 2026-09-10
- **Origin:** a reading question — "what is `alternative` used for here?" — which turned out to have the answer "nothing". Found by grepping every identifier the header exports across `src/`; the surprise that prompted filing it is that the parser combinator work in `tmp/plan/` B1–B8 is not its client and never was.
- **Frozen-tree impact:** none. `src/smd/kit/foundation/alternative.hpp` and its test are live-tree files, and `src/smd/cl/foundation/alternative.hpp` is one of the eighteen shims [BL-0004](BL-0004-retire-the-cl-foundation-forwarding-shims.md) covers. The tree that *was* the client is frozen at `iteration/smdscheme-final` and is not proposed to change.

## What

Decide whether `smd::kit::foundation::alternative` stays in the kit, and on
what grounds. The files are `src/smd/kit/foundation/alternative.hpp`, its
`alternative.test.cpp`, their `CMakeLists.txt` entry, and the `cl` forwarding
shim.

Three outcomes are live. Keep it because B1–B8 will wire `parser<T>` into it,
which is what `compile-time-forth` already does. Keep it as a published kit
surface with no in-repo client, stated as such. Or remove it and let it come
back with its first real caller.

## Why it is not done

Because the answer depends on a B-step that has not been written yet, and
choosing now would either delete something about to be needed or entrench
something that is not.

## Evidence

- **No client in this tree.** `grep -rn "alternative_typeclass\|alternative<\|alt_fn\|empty_fn"` over `src/` returns, outside `alternative.hpp`/`alternative.test.cpp` and the `cl` shim, zero hits. Every other apparent match is `std::holds_alternative` or an unrelated `.empty()` member on `cursor`, `static_vector`, `tagged_tree`, `monoid`. The only `alternative_typeclass` specialization in the repository is `logged<T>`, a writer-like toy defined inside the test file to exercise the typeclass.
- **The original client was the writer, not the parser.** At `iteration/smdscheme-final`, `src/smd/smdscheme/sender/string_writer.hpp:170` registers `alternative_typeclass<string_writer<T>>` alongside its Functor and Applicative registrations. That is the only non-foundation registration in the frozen tree, and it is what the header's own documentation still describes — "the identity element for `alt` (e.g., an empty writer)", "concatenates writer logs". The doc comments are accurate about a client that no longer exists.
- **The parser layer never used the typeclass, in this repo.** `git grep -c alternative_typeclass iteration/smdscheme-final -- 'src/smd/smdscheme/parser/*'` returns nothing. `smdscheme`'s `parser/alt.hpp` was a free function; it was never registered. So the combinator layer here has never gone through Alternative — which is the direct answer to why B1–B8 inheriting the combinator style does not automatically make Alternative used.
- **`compile-time-forth` is where the parser wiring exists.** `src/smd/forth/parser/parser_ops.hpp:164` registers `alternative_typeclass<parser<F>>` so `foundation::alt` dispatches to `parser/alt.hpp`. That is a real client, in the other repository.
- **`forth` is not a kit consumer.** `grep -rn "smd/kit\|kit::foundation"` over that repo's `src/` and `CMakeLists.txt` returns nothing; it carries its own `src/smd/forth/foundation/alternative.hpp`, headed "Adapted by copy". Diffed against the kit's copy modulo namespace, include guard and comments: identical. The two are shared by duplication, not dependency — so `forth`'s use does not keep the kit's copy alive, and drift between them is silent.
- **The client died at A3, not at R8.** R8 extracted `alternative` into the kit under the two-independent-copies rule — `smdscheme` and `forth` had matching copies — and `docs/divergences/DIV-0028-parser-combinator-layer-has-no-cl-client.md` records that `cl` was already not a client of the combinator layer at that point. `smdscheme`'s deletion from trunk at step A3 removed `string_writer` and with it the last in-repo registration.

## Open questions

- Does a B-step actually register `parser<T>` with Alternative, or does `cl`'s combinator layer use a free `alt` the way `smdscheme`'s did? This is the question that decides the item, and it is answerable only when the B-step is written.
- Is "kit surface with no in-repo client" a state the kit is allowed to be in? DIV-0028 used exactly that condition to justify *not* extracting `parser/`, so the kit currently holds a component that fails the standard it applied to `parser/`'s exclusion. Either the standard has an exception for already-extracted components or it does not.
- Does `cl` want a writer at all? The original client was `sender/string_writer`, and `src/smd/cl/sender/` has no equivalent. If a writer returns, Alternative has a client again without any parser work.
- Interaction with the pending `empty` → `zero` rename. Renaming an unused typeclass is cheap; that cheapness is a fact about this item, not evidence about the rename's cost in general.

## Cost and risk

Removal is small and low-risk precisely because nothing calls it: delete two
files, one `CMakeLists.txt` entry, one shim, and `make test-matrix` on both
legs is the whole verification. The cost is not mechanical but archival — the
header is a worked example of the CRTP-base-plus-CPO shape the rest of the kit
uses, and it is the shape `beman.transpose` is reasoning about. Keeping it is
free in code and costs a component that reads as load-bearing but is not.

## Decision criteria

Settle it when the first B-step that touches choice in the reader is written.
If that step registers `parser<T>` with Alternative, close this as declined and
fix the writer-flavoured doc comments to describe parser choice. If it uses a
free `alt`, Alternative has now had no client in this repository across two
front ends and a rebuild, and removal is the honest outcome. Do not schedule
this ahead of that step; deciding early is what makes it the wrong call.
