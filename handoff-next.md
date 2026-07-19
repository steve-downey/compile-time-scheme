# Next step: Common Lisp pivot, Step L10 (CL core model and baseline elaborator)

> **2026-07-18 update:** L6 (this worktree, `cl-pivot/l6-datum-reader`) is done — see `handoff.md`'s
> "Step L6: CL datum reader landed" section. L9 (Lisp-2 environment) landed and merged into `main` earlier
> today (`main` is at commit `50f2819`, "Merge step L9: Lisp-2 environment"; `wt-l9` has already been removed
> as a worktree). Per `docs/cl-pivot-plan.md` section 9, **Step L10 depends on L6 and L9 and both are now
> satisfied** once this branch merges to `main`. L10 is the next unchecked step in `checklist.md`.
> The rest of this file is written for whoever picks up L10.

## What L10 needs from L6 (this step)

L10 builds `src/smd/smdlisp/elaborator/{elaborated_core.hpp,elaborate.hpp}`, consuming the datum tree this step
produced. Read `handoff.md`'s "Step L6: CL datum reader landed" section in full before starting; the essentials:

- `smd::smdlisp::reader::datum_type<MaxNodes, MaxList>` is a `smdscheme::foundation::fix`-based arena tree over
  **six** alternatives: `datum_integer`, `datum_symbol`, `datum_keyword`, `datum_list<R, MaxNodes, MaxList>`,
  `datum_quote<R, MaxNodes>`, `datum_function<R, MaxNodes>`. `datum_symbol`/`datum_keyword` hold a `folded_name`
  (from `reader/atom.hpp`) with a `.view()` accessor, not a bare `string_view`.
- `datum_keyword` is a **separate variant alternative** from `datum_symbol`, not a naming convention — L10's
  elaborator can use `std::holds_alternative<datum_keyword>(...)` directly instead of inspecting a spelling for
  a leading colon.
- `datum_function<R, MaxNodes>{ arena_box<R, MaxNodes> target; }` is what `#'x` reads as. **It is not a list.**
  L10's special-operator table needs a `function` case (per the plan's "Special operators recognized here:
  `quote`, `if`, `progn`, `let`, `let*`, `lambda`, `function`") that pattern-matches on `datum_function`
  directly, in addition to (or instead of, depending on how L10 wants to unify `(function f)` written as a
  list with `#'f` written as sharpsign-quote — the plan's gap analysis table treats `function` as one special
  operator covering both spellings, so L10 will likely need to accept *both* a `datum_function` node and a
  `(function f)`-shaped `datum_list` and elaborate them the same way).
- Lists are proper only — no dotted pairs (D6). `datum_list::elements` is a `static_vector<arena_box<R,
  MaxNodes>, MaxList>`; iterate it directly for form elements (head symbol, args, body forms, etc.).
- Atom classification already happened in the reader; **do not** re-derive "is this a keyword" or "is this the
  symbol T/NIL" from spelling in the elaborator either, if it can instead pattern-match on the datum kind
  (`datum_keyword` vs `datum_symbol`) that the reader already produced. `t` and `nil` are *not* distinguished at
  the datum layer — they are ordinary `datum_symbol` nodes spelled `T`/`NIL`. Per decision D3, giving `NIL` (and
  by extension `T`) their special meaning is explicitly an elaborator/value-layer job, not a reader job — this
  step deliberately left that alone, matching L5's atom reader, which made the same choice for the same reason
  (see L5's handoff entry).
- `read_datum<MaxNodes, MaxList>(cursor, tree_arena&)` is the entry point; call it the same way
  `smdscheme::elaborator::elaborate.test.cpp`'s `elab()` helper calls `smdscheme::reader::read_datum` (read
  `src/smd/smdscheme/elaborator/elaborate.test.cpp` for the exact calling pattern L10's own tests should mirror
  — same `tree_arena<datum_type<N,M>, N>` + `tree_arena<Core, N>` two-arena setup).

## Known open item: DIV-0004 (orgit transclusion path), not blocking L10

`docs/divergences/DIV-0004-orgit-transclusion-worktree-path.md` (filed this step, status `open`) records that
`docs/blog/phase-16-reading-common-lisp.org`'s six `#+transclude:` links point at
`orgit:~/src/compile-time-scheme/wt-l6::...` rather than the usual `orgit:~/src/compile-time-scheme/main::...`,
because the anchored text does not exist in `main` until this branch merges. **After this branch merges to
`main` and before `wt-l6` is deleted as a worktree**, someone (the orchestrator, or whoever lands the merge)
should re-point those six links to `main` — mechanical `sed`, no content change — since `wt-l6`'s worktree may
not exist by the time someone next runs `make blog-md` on `main`. The rendered `.md` is already correct and
already committed; only the `.org` source's re-render path is at risk. This will recur for every later step
with both new code and a blog deliverable (L11, L13, L15, L19, L21, L24) unless the orchestrator adopts a
standing convention for it.

## Known cross-cutting item: `closure/pairs.*` clang-format drift, not blocking L10 but worth a standalone fix

Also recorded in `handoff.md`'s L6 section: `src/smd/smdlisp/closure/pairs.hpp` and `pairs.test.cpp` are
reformatted by the pinned `clang-format v21.1.2` pre-commit hook in this environment even with **zero** content
changes — confirmed by reverting to `main`'s committed content and re-running the hook, which reformats them
again, deterministically. This is the same class of issue `main` commit `b011d52` ("Apply clang-format/gersemi
formatting to merged smdlisp files") already fixed for `closure/value.{hpp,test.cpp}` right after L9 landed;
`pairs.{hpp,test.cpp}` looks like it needs the identical standalone fix. L6 did not touch `closure/**` (out of
lane) and did not fix this — flagging it here so whoever next touches `closure/` (or does a formatting-only
pass on `main`, per the project's "formatting fixes land directly on `main`, separately from feature work"
policy in `docs/cl-pivot-plan.md` section 2 and `docs/codestyle.org`'s Agentic Instructions) picks it up.

## Standing constraints (apply to L10 and everything after)

- `src/smd/smdscheme/**` is frozen for semantic changes (D1); blog phases 5-12 transclude live code from it by
  UUID anchor. Read-only reference, never edit.
- New work goes in `src/smd/smdlisp/`, namespace `smd::smdlisp`, one sub-namespace per directory. L10 adds
  `src/smd/smdlisp/elaborator/`.
- Keep C++26/GCC16 baseline; tests use Catch2; mirror file prolog / include guard / canonical-include
  conventions already used throughout `src/smd/smdlisp/`.
- File a divergence doc under `docs/divergences/` (`TEMPLATE.md` skeleton, next free number is **DIV-0005**)
  for anything done differently than the plan specifies, or any knowing ANSI CL deviation.
- Before handoff: `make compile`, `make test`, `make lint` (and `make blog-md` if the step has a blog
  deliverable — L10 does not; L11 does, per the phase table).
- Do not continue past the assigned step into later steps unless blocked; if blocked, document the blocker
  here instead.
- Work in a fresh worktree off current `main` (which now includes L0-L9 plus, once this branch merges, L6);
  do not reuse `wt-l6` or `wt-l9` (both should be considered disposable/already-merged).
