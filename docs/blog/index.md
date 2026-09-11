- [Overview](#org970c11e)
- [Blog Posts](#orgb5b58f5)
  - [[Phase 0 — Introduction and Motivation](phase-0-intro.md)](#org9ee4df4)
  - [[Phase 1 — Foundation](phase-1-foundation.md)](#org4425f96)
  - [[Phase 2 — Front End](phase-2-front-end.md)](#org5d6eba5)
  - [[Phase 3 — Reader](phase-3-reader.md)](#orgd6ea206)
  - [[Phase 4 — Elaboration](phase-4-elaboration.md)](#orgf6afd6c)
  - [[Phase 5 — Fixpoint Trees](phase-5-fixpoint-trees.md)](#orga8a5535)
  - [[Phase 6 — Closures and Values](phase-6-closures.md)](#orged6b266)
  - [[Phase 7 — Mendler Interpretation](phase-7-mendler.md)](#org34b4a1b)
  - [[Phase 8 — Sender-Based Evaluation](phase-8-senders.md)](#org422597e)
  - [[Phase 9 — Visualizing Execution](phase-9-graphs.md)](#org04da5d9)
  - [[Phase 10 — Constexpr Pipeline](phase-10-constexpr.md)](#orgfd62eea)
  - [[Phase 11 — Real World Integration](phase-11-real-world.md)](#org9965db6)
  - [[Phase 12 — CPS](phase-12-cps.md)](#org3e694e4)
  - [[Phase 13 — Conclusion](phase-13-conclusion.md)](#orgad31dd7)
  - [[Phase 14 — Mutation: set!, begin, and a Store](phase-14-set-bang.md)](#org23e2806)
  - [[Phase 15 — Why Not call/cc: From Scheme to Common Lisp](phase-15-why-common-lisp.md)](#org5f43f8e)
  - [[Phase 16 — Reading Common Lisp: Case, Keywords, and #'](phase-16-reading-common-lisp.md)](#org42d0859)
  - [[Phase 17 — nil, t, and Living in a Lisp-2](phase-17-nil-t-lisp2.md)](#org91c114a)
  - [[Phase 18 — setq, defun, progn: a Programmable Core](phase-18-setq-defun-progn.md)](#orgbc9341b)
  - [[Phase 19 — block, catch, unwind-protect: One-Shot Control in CPS](phase-19-one-shot-control.md)](#org4dfab3d)
  - [[Phase 20 — defmacro: the Compiler Runs the Language](phase-20-defmacro.md)](#org6e1dc71)
  - [[Phase 21 — Common Lisp Control Flow as Sender Graphs](phase-21-sender-graphs.md)](#org175781b)
  - [[Phase 22 — What We Left Out, and Why It Matters](phase-22-limitations.md)](#org05b475b)
  - [[Phase 23 — Why Rebuild Rather Than Refactor](phase-23-why-rebuild.md)](#orge46ebc6)
  - [[Phase 24 — Two Copies and a Fold That Stops](phase-24-substrate.md)](#org3c04272)
  - [[Phase 25 — Interning Symbols, and Seven Divergences That Were One](phase-25-symbols.md)](#org5185565)
  - [[Phase 26 — Reading More Than It Can Run](phase-26-reader-core-ast.md)](#org8079ba7)
  - [[Phase 27 — The Elaborator Never Recurses](phase-27-elaboration.md)](#org921cadf)
  - [[Phase 28 — Three Channels, and a Machine Instead of a Fold](phase-28-three-channels.md)](#org638c907)
  - [[Phase 29 — Answering Phase 7, and One Invariant That Was Two](phase-29-answering-phase-7.md)](#org84a8dbf)
  - [[Phase 30 — Asking Someone Else Whether the Answers Are Right](phase-30-asking-someone-else.md)](#org15f31cf)
  - [[Phase 31 — Driving the Machine, Not Forking It](phase-31-driving-the-machine.md)](#org1b6f45f)
  - [[Phase 32 — Extracting the Kit, and a Bar That Measured the Wrong Thing](phase-32-extracting-the-kit.md)](#org520e936)
  - [[Phase 33 — Nothing Left to Do, and an Oracle Worth Only Its Questions](phase-33-no-next-step.md)](#org4d39e8c)
  - [[Phase 34 — Freezing Two Iterations, and Two Links Nothing Checks](phase-34-freezing-the-iterations.md)](#orge70194c)
  - [[Phase 35 — Cutting the Consumers, and a Green Build That Checks Nothing](phase-35-cutting-the-consumers.md)](#org84baa6c)
  - [[Phase 36 — Deleting Both Front Ends, and the Run That Stopped Before Its First Edit](phase-36-deleting-both-front-ends.md)](#org7b78f42)
  - [[Phase 37 — A Printer, and Why It Could Not Land Alone](phase-37-a-printer-and-its-oracle.md)](#orgc2800c8)
  - [[Phase 38 — Forty-Six Strings, and a Spot Check That Could Not Run](phase-38-forty-six-strings.md)](#org5ed904d)
- [Table of Contents](#org0e228ac)



<a id="org970c11e"></a>

# Overview

This is the complete blog series documenting the implementation of a Scheme-light compiler in C++26, compiling entirely at compile-time inside the C++ constant evaluator. Each phase builds on the previous work, from zero-allocation parsing through fixpoint trees to Mendler-style evaluation and sender-based asynchronous execution.


<a id="orgb5b58f5"></a>

# Blog Posts


<a id="org9ee4df4"></a>

## [Phase 0 — Introduction and Motivation](phase-0-intro.md)

The overall design philosophy: expanding C++26 `constexpr` to prove what the constant evaluator can accomplish.


<a id="org4425f96"></a>

## [Phase 1 — Foundation](phase-1-foundation.md)

Establish the memory vocabulary: `result<T>`, `static_vector`, `Box<A>`, and the `Fix<F>` fixpoint combinator.


<a id="org5d6eba5"></a>

## [Phase 2 — Front End](phase-2-front-end.md)

Zero-allocation combinator parsers using immutable cursors and applicative composition.


<a id="orgd6ea206"></a>

## [Phase 3 — Reader](phase-3-reader.md)

Translate source text into a raw `Datum` tree, treating all input as nested shapes with no semantic judgement.


<a id="orgf6afd6c"></a>

## [Phase 4 — Elaboration](phase-4-elaboration.md)

Classify `Datum` nodes into a typed core AST covering `if`, `lambda`, `let`, `let*`, `define`, and `quote`.


<a id="orga8a5535"></a>

## [Phase 5 — Fixpoint Trees](phase-5-fixpoint-trees.md)

Define `Fix<CompF>` — a heap-pointer computation tree using open-recursive algebraic types and the fixpoint combinator.


<a id="orged6b266"></a>

## [Phase 6 — Closures and Values](phase-6-closures.md)

Define the runtime value domain: numbers, booleans, closures, and environments that bind names to values at evaluation time.


<a id="org34b4a1b"></a>

## [Phase 7 — Mendler Interpretation](phase-7-mendler.md)

Implement `mendler_run` using a genuine `mendler_para` combinator — a Mendler-style paramorphism over `Fix<CompF>` that is synchronous and fully `constexpr`-capable.


<a id="org422597e"></a>

## [Phase 8 — Sender-Based Evaluation](phase-8-senders.md)

Implement `sender_mendler_run`, which rewrites evaluation as a graph of Beman Execution senders with `when_all` for argument-list parallelism.


<a id="org04da5d9"></a>

## [Phase 9 — Visualizing Execution](phase-9-graphs.md)

Use C++26 reflection to walk the nested sender type structure at compile time and emit Graphviz DOT output of the execution graph.


<a id="orgfd62eea"></a>

## [Phase 10 — Constexpr Pipeline](phase-10-constexpr.md)

Wire all phases together into a single `constexpr` pipeline and demonstrate evaluation results baked into the binary.


<a id="org9965db6"></a>

## [Phase 11 — Real World Integration](phase-11-real-world.md)

Show how to embed the compiler in a real C++ program: pre-compiled closures, runtime environment injection, and FFI callbacks.


<a id="org3e694e4"></a>

## [Phase 12 — CPS](phase-12-cps.md)

Explore Continuation-Passing Style as an alternative evaluation backend for the closure evaluator.


<a id="orgad31dd7"></a>

## [Phase 13 — Conclusion](phase-13-conclusion.md)

Reflect on what the climb taught me about C++26 `constexpr` limits, fixpoint types, and the Mendler recursion scheme.


<a id="org23e2806"></a>

## [Phase 14 — Mutation: set!, begin, and a Store](phase-14-set-bang.md)

Come back down from the summit for `set!`: a shared store of mutable cells, an environment that binds names to locations, and the `begin` sequencing it takes to observe an effect.


<a id="org5f43f8e"></a>

## [Phase 15 — Why Not call/cc: From Scheme to Common Lisp](phase-15-why-common-lisp.md)

*DRAFT — pending author revision.* Why `call/cc` stops here: a sender's one-shot completion contract cannot express a multishot continuation, Kiselyov's independent case against `call/cc`, and why Common Lisp's dynamic-extent control operators are exactly the discipline senders already enforce.


<a id="org42d0859"></a>

## [Phase 16 — Reading Common Lisp: Case, Keywords, and #'](phase-16-reading-common-lisp.md)

*DRAFT — pending author revision.* The `smdlisp` reader: case folding to uppercase at read time, keywords as a distinct datum kind, `;` comments as intertoken space, the maximal-munch fix behind DIV-0003 (the `1+` bug), and `#'` lowering to its own datum kind instead of a synthesized `(function x)` list.


<a id="org91c114a"></a>

## [Phase 17 — nil, t, and Living in a Lisp-2](phase-17-nil-t-lisp2.md)

*DRAFT — pending author revision.* The direct evaluator: one `is_true` function instead of Scheme's per-site `#f` encoding, a Lisp-2 environment where variable and function lookup never touch, `funcall` and `#'` as real call semantics, and the closure-capture-ownership question resolved with an arena instead of an owning pointer.


<a id="orgbc9341b"></a>

## [Phase 18 — setq, defun, progn: a Programmable Core](phase-18-setq-defun-progn.md)

*DRAFT — pending author revision.* `setq` returns the assigned value and `defun~/~defvar~/~defparameter` return the bound name, both departures from Scheme; the store from Phase 14 adapted for ANSI CL's return conventions; a mutable environment reference threaded through both the direct evaluator and a new continuation-passing backend; and a datum-arena lifetime bug caught at compile time while building a one-argument `compile_to_closure`.


<a id="org4dfab3d"></a>

## [Phase 19 — block, catch, unwind-protect: One-Shot Control in CPS](phase-19-one-shot-control.md)

*DRAFT — pending author revision.* The one-shot nonlocal exits Phase 15 argued for, now built in both evaluators: `block` and `return-from` resolved lexically by name, `catch` and `throw` resolved dynamically by an evaluated tag, and `unwind-protect` running its cleanups on every way out. The lexical/dynamic distinction lands as two data structures with two lifetimes — an append-only slab of exit records a closure may capture, and a reused-slot stack of catch frames nothing can. Plus DIV-0011: an uncaught `throw` here runs cleanups that ANSI CL says should not run.


<a id="org6e1dc71"></a>

## [Phase 20 — defmacro: the Compiler Runs the Language](phase-20-defmacro.md)

*DRAFT — pending author revision.* Object-language macros: a `defmacro`-defined `my-when` is a Lisp lambda that `smdlisp` compiles with its own elaborator and runs with its own evaluator during the expansion pass — the compiler running the language it compiles, at compile time. The new machinery is a datum⇄value reification pair; the merge test is that `my-when` is `when`, whether written with `list~/~cons` or with a backquote template, plus an expansion-budget diagnostic for a macro that expands into itself.


<a id="org175781b"></a>

## [Phase 21 — Common Lisp Control Flow as Sender Graphs](phase-21-sender-graphs.md)

*DRAFT — pending author revision.* The Common Lisp core evaluated over Beman Execution senders. The closure backends push a value, a diagnosed error, a `return-from` unwind and a `throw` unwind down one `result<value>` wire and tell them apart with sentinel messages compared by pointer identity; a sender has three channels natively, so the sentinels are deleted instead of ported. `unwind-protect` becomes an ordinary sender adapter whose three completion functions all funnel into one cleanup call, while dynamic-binding restore refuses to move out of ordinary C++ control flow at all. Plus a P2996 graph dump, and DIV-0015: this backend has no `constexpr` twin.


<a id="org05b475b"></a>

## [Phase 22 — What We Left Out, and Why It Matters](phase-22-limitations.md)

*DRAFT — pending author revision.* The reflective finale. Most of decision D10's out-of-scope list — strings, floats, CLOS, `format`, `loop` — is incidental to Phase 15's thesis about one-shot control; two gaps are not: the sender backend doesn't cover multiple values (DIV-0017/0018), and a nonlocal exit carries only its primary value even where every other exit path carries all of them (DIV-0019). Plus five sharper structural limits worth a paragraph each, including DIV-0015's warning not to call all three backends compile-time evaluators.


<a id="orge46ebc6"></a>

## [Phase 23 — Why Rebuild Rather Than Refactor](phase-23-why-rebuild.md)

*DRAFT — pending author revision.* The pivot works, and this is the argument for building it again anyway. Twenty divergence records, each locally correct and honestly written the same day, turn out on a single reading to be seven bills for one missing abstraction: a symbol is a `std::string_view` and so has nowhere to keep anything. Plus the freeze whose reason had evaporated, a test suite that had become a ratchet rather than a safety net, and a style document that was ratified and never adopted. Nothing is built; everything here is an argument, and arguments about software are cheap.


<a id="org3c04272"></a>

## [Phase 24 — Two Copies and a Fold That Stops](phase-24-substrate.md)

*DRAFT — pending author revision.* The new tree's `foundation/` starts as the reviewed union of two independent copies of the same code, and the merge is convincing mostly because there was nothing to merge — for five files the entire difference is the include guard, the namespace, and a comment saying it was adapted by copy. New in neither copy: a left fold that stops on the first failure, which `std::ranges::fold_left` can't do, and the Foldable and Traversable typeclasses the coding rules had required all along. The substrate has no behaviour of its own, so the law tests are the evidence.


<a id="org5185565"></a>

## [Phase 25 — Interning Symbols, and Seven Divergences That Were One](phase-25-symbols.md)

*DRAFT — pending author revision.* The keystone. A symbol becomes an entry in a table with a name and three independently writable slots, identity becomes id comparison, and the table owns its characters so a compiled program stops pointing into the reader's arena. Six of the seven divergences close by construction and a seventh partly — in a component with no callers, since this tree has no reader and no evaluator yet. Also settles a question the plan left open: the table rides along into the running program.


<a id="org8079ba7"></a>

## [Phase 26 — Reading More Than It Can Run](phase-26-reader-core-ast.md)

*DRAFT — pending author revision.* A readtable-shaped reader that implements full ANSI atom syntax before the evaluator can run any of it: strings, characters, `#(...)` vectors and every numeric-tower spelling read without complaint, and a literal with no machine representation yet is carried as its spelling. The datum tree and the core AST become instantiations of one container, so the Foldable and Traversable instances are written once and law-tested once. Plus an oracle test against the frozen pivot reader, three cases excluded because the oracle is the wrong answer there, and a GCC trunk misfold that the compile-time half of a twin test could not see.


<a id="org921cadf"></a>

## [Phase 27 — The Elaborator Never Recurses](phase-27-elaboration.md)

*DRAFT — pending author revision.* A tree built children-before-parent has node indices in topological order, so ascending index order is a bottom-up catamorphism and descending order is top-down propagation. Elaboration becomes three index loops — atoms by `traverse`, roles by a descending pass, emission by a fold that stops on the first error — with no stack, no visitor that calls itself, and no bound on nesting depth. Plus the Lisp-2 rule as a consequence rather than a special case, and the one new node kind that decision D18's own exception licenses.


<a id="org638c907"></a>

## [Phase 28 — Three Channels, and a Machine Instead of a Fold](phase-28-three-channels.md)

*DRAFT — pending author revision.* An evaluated form finishes with a value, with a diagnosed error, or with an unwind in flight, and decision D13 makes those three alternatives of one type, so the pivot's sentinel messages compared by pointer identity are deleted instead of ported. `unwind-protect` is what the split costs, and it is rebuilt as a continuation frame that intercepts all three channels. Evaluation is the one traversal here the tree's own folds can't carry, so the evaluator is a small-step machine with defunctionalized continuations and its single loop in `foundation::trampoline` — which makes recursion depth and non-termination diagnosed capacities rather than compiler behaviour. Plus DIV-0009 closed: a recursive `defun` counts a list inside a `static_assert`.


<a id="org84a8dbf"></a>

## [Phase 29 — Answering Phase 7, and One Invariant That Was Two](phase-29-answering-phase-7.md)

*DRAFT — pending author revision.* No step of the rebuild landed here. Phase 28 claimed evaluation is the one traversal that can't be a fold, and phase 7 of this series had already evaluated `if` inside a Mendler fold; what actually rules the fold out is the representation, since a columnar tree has no pointer to chase and no descent for a recurse-knob to choose. The machine turns out to be a scheme anyway — an unfold, with `step` as the coalgebra — and the reference doc behind phases 5 through 8 gets a status header, because its warning that a CPS trampoline linearizes independent arguments inverts on a column. Re-reading the argument against the code found the defect: `tagged_tree` was carrying two invariants as one, and children-before-parent, which every scheme depends on, was documented in a comment and checked nowhere.


<a id="org15f31cf"></a>

## [Phase 30 — Asking Someone Else Whether the Answers Are Right](phase-30-asking-someone-else.md)

*DRAFT — pending author revision.* Decision D16's conformance corpus: seventy-five cases, each stating its outcome by channel — value, diagnosed error, or unwind — split between thirty-five adapted from Paul Dietz's `ansi-test` (MIT, pinned commit) and forty hand-derived from the ANSI text, because `ansi-test` leans on strings, characters and the numeric tower, none of which are in scope. Only one entry, `nil.8`, pins anything, because D16 permits a test to pin a scope-decision and never a defect; writing the other seventy-four found six new divergences. Plus `sbcl_oracle.hpp`: a differential check against a real SBCL, shelled out to and skipped rather than failed when the binary isn't on `PATH`, closing the weakest evidence this series has been carrying since phase 28.


<a id="org1b6f45f"></a>

## [Phase 31 — Driving the Machine, Not Forking It](phase-31-driving-the-machine.md)

*DRAFT — pending author revision.* Decision D17's second backend over the rebuild's core tree: `machine_sender` connects a Beman Execution sender that runs `eval::machine`'s own trampoline to completion, then dispatches the result onto `set_value`, `set_error`, and `set_stopped`, D13's three channels spent again. The tempting alternative — forking a call's argument list into sibling senders, the pivot's trick — was rejected on a concrete cost recorded in DIV-0016: the machine is single-owner state, and reproducing that trick would mean rebuilding a recursive evaluator in the one place R5 spent a whole phase removing recursion from. Nothing here is `constexpr` (DIV-0015, accepted-permanent), and the one `when_all` demonstration joins two independent whole programs, never one program's own arguments.


<a id="org520e936"></a>

## [Phase 32 — Extracting the Kit, and a Bar That Measured the Wrong Thing](phase-32-extracting-the-kit.md)

*DRAFT — pending author revision.* Step R8, the rebuild plan's last step: checking `docs/cl-rebuild-plan.md` §5's years-old kit specification against the actual code rather than re-quoting it. `parser/` turns out to have no `cl` client at all — the reader is hand-written recursive descent, not a combinator stack — so DIV-0028 leaves it unmoved while `foundation/`'s nine named files extract, three of them as supersets rather than exact copies. Seventeen files land in `smd::kit::foundation` as forwarding shims, not nine: the first pass drew the kit's boundary on whether a second implementation of `foldable~/~traversable~/~monoid~/~identity` already existed, mistaking a sibling project's unfinished refactor for evidence about this code's generality, and the repository owner overturned it before the step closed.


<a id="org4d39e8c"></a>

## [Phase 33 — Nothing Left to Do, and an Oracle Worth Only Its Questions](phase-33-no-next-step.md)

*DRAFT — pending author revision.* `AGENTS.md` says to work the next unchecked step in `checklist.md`, and after R8 there wasn't one: the file is ticked through the original phases, the pivot, the rebuild and the blog backfill, and then stops, so an agent following the project's own rules finds nothing to do. Step A0 puts the next fourteen steps there and repairs four records that had come loose the same way — two scoping notes still calling themselves unratified proposals after being merged, a phase sketch for a plan nobody will execute, and three backlog items whose gating conditions had passed unnoticed. The substantive one is decision D32, which overrules the ratified D22: parity with `smdlisp` was to be the language plan's first phase and retiring `smdlisp` its merge criterion, and the new plan deletes it three steps from here instead. D22's oracle argument was measured before being overruled and held up — seventy-five corpus entries reach `cl`'s seven special operators and nothing else, while `smdlisp` holds roughly 120 evaluated cases covering exactly the gap — so what settles it is narrower: `smdlisp` supplies source programs and never expectations, and an annotated tag supplies those for free. Plus DIV-0029, for the thing this repository had no mechanism for at all, which is how a ratified decision gets superseded.


<a id="orge70194c"></a>

## [Phase 34 — Freezing Two Iterations, and Two Links Nothing Checks](phase-34-freezing-the-iterations.md)

*DRAFT — pending author revision.* Step A1, so that step A3 can delete thirty thousand lines without breaking anything that points at them. `docs/compiler_architecture.org` is a living document whose links `scripts/verify-transclusions.sh` resolves against the worktree on purpose, and thirty-one of them pointed into the two trees about to leave trunk — thirty-one honest failures on the very next run. Both trees are frozen as annotated `iteration/*` tags and the prose moves to `docs/history/architecture-iterations.org`, repinned. The counting is the story: three passes over the same document reported 37 transclusions, then 39, then 37 again, because 39 is how many `[[file:` links there are and 37 is how many carry a `::<uuid>`. The two left over are plain hyperlinks to `docs/cl-limitations.md`, both in the section that moved, into a document one directory deeper — and nothing in this repository can tell you whether they still resolve.


<a id="org84baa6c"></a>

## [Phase 35 — Cutting the Consumers, and a Green Build That Checks Nothing](phase-35-cutting-the-consumers.md)

*DRAFT — pending author revision.* Step A2, the middle of a three-step subtraction: A1 froze the two dead front ends as tags, A2 cuts everything that points at them, A3 deletes them. The build still names both trees out loud — eleven examples link their targets by name, the top-level export list names eight of them, and `src/smd/cl/reader/oracle_compare.test.cpp` reaches into *both* trees rather than only the one an earlier pass at the plan assumed, the third reach arriving transitively with no include of its own. Twelve lines in, 1302 out. The number to distrust is `make testinstall`, which moved from exit 2 to exit 0 here and was not fixed: `find_package` now resolves a package claiming no `smd::smdscheme` target, nothing is left to link one, and zero installed tests run. Four separate documents say so, because the exit code is the only part a later reader sees without opening any of them. Plus a spot check that is a `grep` for two strings which has to come back empty, so the comments left in place of the deleted wiring can not name the trees they are about — and one of those comments got its own count wrong on the way out.


<a id="org7b78f42"></a>

## [Phase 36 — Deleting Both Front Ends, and the Run That Stopped Before Its First Edit](phase-36-deleting-both-front-ends.md)

*DRAFT — pending author revision.* Step A3, the deletion the two steps before it existed to make possible: `src/smd/smdscheme/` and `src/smd/smdlisp/` leave trunk, 173 files and 31,155 lines out, and `ctest` goes from 1105 entries to 304 on both matrix legs, both still 100% passing. It took two runs — the first verified a green baseline, reached `git rm -r`, and was refused by a local tool-permission classifier before making a single edit, and its `"outcome": "blocked"` row was kept on purpose, so the record shows the deletion took two goes and why. The decision executed here is D32, which retires a stated never-edit rule; DIV-0032 is only the divergence number reserved for the step, and conflating the two files the governance record in the wrong directory. The oracle did not go because friction won: `smdlisp` really was a better oracle than SBCL for this project's own semantic choices, and the measurement that checked that held up, so the reason is narrower — `smdlisp` can supply source programs and can not supply expectations, and a tag supplies source programs for free and forever. One document on trunk now says the deciding reason was friction, and it is the one this step wrote. The same step's note under D32 gets the `CMakeLists.txt` count wrong in the sentence that proves the tree could never have been kept, and the front page of the generated documentation turns out to have carried a code sample with a literal `...` in it that could never have compiled.


<a id="orgc2800c8"></a>

## [Phase 37 — A Printer, and Why It Could Not Land Alone](phase-37-a-printer-and-its-oracle.md)

*DRAFT — pending author revision.* Step A4 lands `smd::cl::printer::prin1` and its first oracle comparison in one commit, because a printer whose only witness is a test written from its own output is the evidence D16 refuses. A2 retired the reader's structural comparison against `smdlisp` and A3 deleted the tree it compared against, so until this step there was no differential oracle at all — a gap opened on purpose. The printer is a `foundation::cata_short` over the datum tree, carrier the rendered text of one node, and `sbcl_read_print` lands beside it: eleven source strings read here, rendered, and compared against what SBCL 2.2.9.debian's own reader and `prin1` make of the same string, all eleven agreeing on the first run. `1+` is the one worth stopping on, because DIV-0003 has been accepted-permanent since step L5 on the strength of an argument and nothing else, and now an implementation that has never seen this repository agrees. Three of SBCL's answers were measured rather than guessed: with `*print-pretty*` off `'x` prints as `(QUOTE X)`, the empty list prints as `NIL`, and a tower prints its value where this reader prints its spelling. The header documents one way to fail and the code has seven, six of which nothing the reader produces can reach; its capacity note says 32 KiB, which is the figure for a 64-node tree, where the test file declares 96 and holds 48.


<a id="org5ed904d"></a>

## [Phase 38 — Forty-Six Strings, and a Spot Check That Could Not Run](phase-38-forty-six-strings.md)

*DRAFT — pending author revision.* Step A5 broadens A4's eleven cases into forty-one source strings and a five-case error table, and closes Phase A. Twenty of the twenty-three non-error inputs the retired `oracle_compare.test.cpp` tested come across, checked against SBCL now instead of against a sibling tree in this same repository; the three that don't are the quasiquote family, which SBCL prints as `SB-INT:QUASIQUOTE` and `#S(SB-IMPL::COMMA ...)`, implementation-specific and ANSI-permitted. Twenty more are syntax that comparison could never have reached, because D10 kept strings, characters, vectors and the numeric tower out of `smdlisp` entirely. All forty-six agreed with SBCL 2.2.9.debian on the first run, one attempt, no new exclusions. What that green does not say is whether the comparison happened at all: the differential skips when `sbcl` is not on `PATH`, Catch2's `SKIP_RETURN_CODE 4` keeps the leg at 100% passing, and the oracle's version rides in an `INFO` that prints only on failure. The one spot check that would have shown the difference — 180 assertions against 1 — globs two directories down for a binary that is six down, and `2>/dev/null` swallows the error while `tail` returns 0. The capacity comment is written for pressure that isn't there, since the widest case in either table holds three elements. And DIV-0034 records that the file named `sbcl_differential.test.cpp` has never compared this project against SBCL at all, which makes it the second divergence number Phase A issued out of six reserved, the four in between having gone unused.


<a id="org0e228ac"></a>

# Table of Contents


# Table of Contents

1.  [Overview](#org970c11e)
2.  [Blog Posts](#orgb5b58f5)
3.  [Table of Contents](#org0e228ac)
