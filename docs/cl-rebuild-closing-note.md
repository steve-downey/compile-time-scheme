# The Common Lisp rebuild: closing note

Written at the close of step R8, the plan's last step.
Retires `step-brief-r8.md`.
This is not a new step brief — there is no R9 — it is the plan's own account of what it set out to do (`docs/cl-rebuild-plan.md` §1) against what actually landed, and what is still open.

## What §1 asked for

Three complaints justified rebuilding `smdlisp` rather than refactoring it in place:

1. **The `smdscheme` freeze was blocking real fixes for a reason that no longer existed** (D11) — four divergences (DIV-0011, DIV-0012, DIV-0016, DIV-0021) were compromises made only because D1 forbade touching `smdscheme`, after the blog moved to tag-pinned transclusion and D1's own rationale evaporated.
2. **The test suite had become a ratchet** — roughly 26 tests pinned divergent behaviour, and DIV-0021/DIV-0018 admitted outright that closing the bug would cost a test deletion, which made "add a test" and "fix a bug" pull in opposite directions.
3. **`docs/CODING_RULES.md` was already ratified and never followed** — `fold_map` as the semantic centre, `traverse` as the minimal Traversable operation, typeclass instances by variable template, law tests before performance tests. `smdlisp` had none of it: 79 raw index loops, 23-arm `std::visit` switches, no Foldable/Traversable instance for either AST.

## What landed, R1 through R8

- **R1** — the substrate (`src/smd/cl/foundation`): the reviewed union of the `smdscheme` and `compile-time-forth` copies, plus the short-circuiting fold `std::ranges::fold_left` does not provide, plus the typeclass instances `docs/CODING_RULES.md` requires, with law tests from the start.
- **R2** — the interned symbol table (D12), the keystone every later step depends on.
- **R3** — the reader and the core AST as one `tagged_tree`: children-before-parent as its own base functor, Foldable/Traversable/recursion schemes present from the first commit rather than retrofitted.
- **R4** — the elaborator as three named schemes (`traverse`, a scan, a paramorphism) rather than hand-written recursion, closing the "same missing abstraction billed seven times" complaint from §1's own tally.
- **R5** — one evaluator, three channels (D13): a small-step abstract machine, not a fold, with recursive `defun` — DIV-0009, the headline defect — working at the end of the step.
- **R6** — conformance measured against external authority (D16): the `ansi-test` subset and an SBCL differential oracle, so correctness stopped being "this implementation agrees with itself."
- **R7** — the sender backend (D17), a second backend over the same core tree that drives the whole machine rather than forking its frame.
- **R8** — this step. See below.

Measured against §1's three complaints: the freeze is retired and the four divergences it was blocking are closed or reclassified; the rebuild's own test suite has zero tests pinning a `defect` (D16's rule, checked by `docs/divergences/README.md`'s classification table) — the ratchet did not carry over; and `docs/CODING_RULES.md` is now the shape of `src/smd/cl/**`, not a document sitting beside code that ignores it. §1's brief is met.

## What R8 actually did, honestly

`docs/cl-rebuild-plan.md` §5 named the kit's contents in advance — `foundation/` (nine files) plus `parser/` (four files) — and framed extraction as happening "once Common Lisp is the third working client."
Verifying that claim against the actual R8-era code, rather than the R0-era plan, changed the scope in two ways, and a third correction landed after the first commit, from the repository owner rather than from re-reading the plan:

1. **`foundation/` extracted, `parser/` did not.** `cl` is a genuine third client of the nine `foundation/` files named in §5 (`static_vector`, `result`, `parse_error`, `source_pos`, `source_span`, `arena_box`, `functor`, `applicative`, `alternative`), verified by grep across every `cl` area. It is not a client of `parser/` at all: `src/smd/cl/reader/cursor.hpp` is a hand-written recursive-descent reader with no `parser<T>`, `alt`, or `parser_ops` anywhere underneath it. R3 chose that shape and nothing revisited it. `parser/` stays exactly where it was in `smdscheme` and `forth`, undiffed and unmoved. See DIV-0028 and the dated correction appended to plan §5.
2. **One real client, not three.** Even for `foundation/`, this repository can only actually re-home `cl` onto the kit: `smdscheme` is never edited in place (the one hard rule among the three trees), and `compile-time-forth` is a separate repository this project can read but not modify. What shipped is `smd::kit::foundation` — a real second build target (`kit.foundation`), with `cl.foundation` as its one real, working, tested client — plus the three-way diff as evidence the other two copies could adopt it unchanged if their own maintainers chose to. That is smaller than "the third working client" implies, and the gap is recorded rather than glossed: see `docs/compiler_architecture.org` § "The kit: a real second target with one real client."
3. **The kit's boundary was drawn on the wrong test, and was corrected before this step closed.** The first commit left `foldable`, `traversable`, `monoid`, and `identity` in `cl`, reasoning that neither `smdscheme` nor `compile-time-forth` had ever needed them, so they were single-implementation and had not earned kit membership by the same two-independent-copies bar the other nine passed. The repository owner overturned that: `compile-time-forth` has not had the algebraic refactoring done on it that `cl` has, because nobody has done that work yet — the absence measured a sibling project's backlog, not this code's generality. §5's actual test is "generic in shape and free of language-specific types," which all four pass outright, along with two more files the corrected standard reaches: `fold_left_short` and `trampoline`, motivated by decision D15 and by step R5's evaluator but naming neither. All six moved. `result_instances.hpp` and `static_vector_instances.hpp` — adapters that were already depending on both the moved typeclasses and the moved datatypes — moved with them for the same reason, which incidentally dissolved a namespace-specialization workaround the first commit had introduced rather than merely working around it a second time. `node_f`, `topo_fold`, `tagged_tree`, `tagged_tree_instances`, and `tagged_tree_schemes` stayed excluded throughout: they name an AST type, and decision 2 was never in question for them. The full argument, including why the two-copies test was the wrong bar to begin with, is in `docs/compiler_architecture.org` § "The kit: a real second target with one real client," rewritten in place rather than appended to, since it is a correction to a design record and not a divergence.

Mechanically, seventeen files moved to `src/smd/kit/foundation/` under `smd::kit::foundation`, and `src/smd/cl/foundation/`'s seventeen headers became forwarding shims: same include path, same namespace, same names, so no caller outside `src/smd/cl/foundation/` needed to change. The one real subtlety, found by the build rather than anticipated: any file that specializes `functor_typeclass`/`applicative_typeclass`/`foldable_typeclass`/`traversable_typeclass` for a type — whether the type itself moved to the kit (`result`, `static_vector`, `identity`) or stayed in `cl` (`tagged_tree`) — must write that specialization in the namespace the primary template actually lives in, and a `using`-declaration does not change which namespace that is. Where both the type and the typeclass moved, the specialization is ordinary same-namespace code again (`result_instances.hpp`, `static_vector_instances.hpp`, `identity.hpp`). Where the typeclass moved but the type did not (`tagged_tree_instances.hpp`), the specialization still has to open `namespace smd::kit::foundation` explicitly and name the cl-owned type by full qualification — that workaround did not go away, because `tagged_tree` is the one case it is honestly needed for.

## What remains open

From `docs/cl-rebuild-plan.md` §8, still open at the plan's close:

- **Whether `ansi-test` is usable directly, needs adaptation, or must be hand-derived from the specification.** R6 answered this empirically for the subset it reached; the general question — as tower and string semantics land per D19 — stays open for whoever extends the corpus next.
- **Whether `smdlisp` is eventually retired or kept permanently as the pivot's artefact.** Not urgent; §8 already says it must survive at least until R6 could replace it as oracle, which R6 has now done. Nothing in this plan forces a decision on deletion.
- **Whether the second, Lisp-family layer of the kit (AST, `fix`, recursion schemes) is worth extracting**, given it would have exactly one client (`cl`) until a third Lisp-family front end exists. R8 leaves this exactly as open as §8 left it. `node_f`/`topo_fold`/`tagged_tree*` stayed excluded throughout R8, but on the correct ground this time: they name an AST type, which the corrected §5 test excludes on its own terms, not because they lack a second implementation — a distinction decision 2's correction (above) exists precisely to keep from being blurred again.

What R8 adds to the open-questions list:

- **`parser/` has no `cl` client** (DIV-0028). Revisit if `cl`'s reader is ever rewritten onto the combinator style, or if a fourth front end needs `parser<T>` independently.
- **`smd::kit::foundation`'s single-client status.** The kit is real and tested, but "extracted" here means "identified, relocated, and proven portable by a three-way diff," not "adopted by multiple in-repo build targets." If `smdscheme` is ever deleted from trunk (it is slated to be, per `AGENTS.md`) rather than converged onto the kit, the kit's justifying evidence becomes historical rather than a build-graph fact — worth a line in whatever record announces that deletion.

## What could not be verified

The `smdscheme`-vs-`forth` half of the three-way diff (the claim that those two copies are 0–6 lines apart per file) was re-verified directly against both repositories in this step, not merely re-quoted from §5. The `cl`-vs-`forth` and `cl`-vs-`smdscheme` diffs were likewise re-run rather than assumed. What was not, and could not be, verified from inside this repository: whether `compile-time-forth`'s own maintainers would in fact adopt `smd::kit::foundation` if offered it, or whether `smdscheme`'s eventual deletion is scheduled. Both are decisions outside this repository's authority.
