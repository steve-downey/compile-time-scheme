- [Overview](#orgb770e41)
- [Blog Posts](#org5277adf)
  - [[Phase 0 — Introduction and Motivation](phase-0-intro.md)](#org8f22768)
  - [[Phase 1 — Foundation](phase-1-foundation.md)](#org29faa5d)
  - [[Phase 2 — Front End](phase-2-front-end.md)](#org635978b)
  - [[Phase 3 — Reader](phase-3-reader.md)](#orgb7de5bb)
  - [[Phase 4 — Elaboration](phase-4-elaboration.md)](#org8e65c4f)
  - [[Phase 5 — Fixpoint Trees](phase-5-fixpoint-trees.md)](#orgfaddded)
  - [[Phase 6 — Closures and Values](phase-6-closures.md)](#orgf765780)
  - [[Phase 7 — Mendler Interpretation](phase-7-mendler.md)](#org6d3c446)
  - [[Phase 8 — Sender-Based Evaluation](phase-8-senders.md)](#orgfa9f1c5)
  - [[Phase 9 — Visualizing Execution](phase-9-graphs.md)](#orgd3a8af2)
  - [[Phase 10 — Constexpr Pipeline](phase-10-constexpr.md)](#orgcb9f8cc)
  - [[Phase 11 — Real World Integration](phase-11-real-world.md)](#org28de832)
  - [[Phase 12 — CPS](phase-12-cps.md)](#org7eee05e)
  - [[Phase 13 — Conclusion](phase-13-conclusion.md)](#org54ccbe3)
  - [[Phase 14 — Mutation: set!, begin, and a Store](phase-14-set-bang.md)](#org1cd3e30)
  - [[Phase 15 — Why Not call/cc: From Scheme to Common Lisp](phase-15-why-common-lisp.md)](#org90fde2f)
  - [[Phase 16 — Reading Common Lisp: Case, Keywords, and #'](phase-16-reading-common-lisp.md)](#orgb51f89f)
  - [[Phase 17 — nil, t, and Living in a Lisp-2](phase-17-nil-t-lisp2.md)](#orgac6a91a)
  - [[Phase 18 — setq, defun, progn: a Programmable Core](phase-18-setq-defun-progn.md)](#org9d51064)
  - [[Phase 19 — block, catch, unwind-protect: One-Shot Control in CPS](phase-19-one-shot-control.md)](#orgbf18026)
  - [[Phase 20 — defmacro: the Compiler Runs the Language](phase-20-defmacro.md)](#org48056d3)
  - [[Phase 21 — Common Lisp Control Flow as Sender Graphs](phase-21-sender-graphs.md)](#org54f3ea9)
  - [[Phase 22 — What We Left Out, and Why It Matters](phase-22-limitations.md)](#orgc08ad7b)
  - [[Phase 23 — Why Rebuild Rather Than Refactor](phase-23-why-rebuild.md)](#org8f9d1a9)
  - [[Phase 24 — Two Copies and a Fold That Stops](phase-24-substrate.md)](#org7c72214)
  - [[Phase 25 — Interning Symbols, and Seven Divergences That Were One](phase-25-symbols.md)](#org1bd73ad)
  - [[Phase 26 — Reading More Than It Can Run](phase-26-reader-core-ast.md)](#org5eefbe4)
  - [[Phase 27 — The Elaborator Never Recurses](phase-27-elaboration.md)](#orga555bc0)
  - [[Phase 28 — Three Channels, and a Machine Instead of a Fold](phase-28-three-channels.md)](#org9034ddf)
  - [[Phase 29 — Answering Phase 7, and One Invariant That Was Two](phase-29-answering-phase-7.md)](#orga074ad2)
- [Table of Contents](#org0eaf4c0)



<a id="orgb770e41"></a>

# Overview

This is the complete blog series documenting the implementation of a Scheme-light compiler in C++26, compiling entirely at compile-time inside the C++ constant evaluator. Each phase builds on the previous work, from zero-allocation parsing through fixpoint trees to Mendler-style evaluation and sender-based asynchronous execution.


<a id="org5277adf"></a>

# Blog Posts


<a id="org8f22768"></a>

## [Phase 0 — Introduction and Motivation](phase-0-intro.md)

The overall design philosophy: expanding C++26 `constexpr` to prove what the constant evaluator can accomplish.


<a id="org29faa5d"></a>

## [Phase 1 — Foundation](phase-1-foundation.md)

Establish the memory vocabulary: `result<T>`, `static_vector`, `Box<A>`, and the `Fix<F>` fixpoint combinator.


<a id="org635978b"></a>

## [Phase 2 — Front End](phase-2-front-end.md)

Zero-allocation combinator parsers using immutable cursors and applicative composition.


<a id="orgb7de5bb"></a>

## [Phase 3 — Reader](phase-3-reader.md)

Translate source text into a raw `Datum` tree, treating all input as nested shapes with no semantic judgement.


<a id="org8e65c4f"></a>

## [Phase 4 — Elaboration](phase-4-elaboration.md)

Classify `Datum` nodes into a typed core AST covering `if`, `lambda`, `let`, `let*`, `define`, and `quote`.


<a id="orgfaddded"></a>

## [Phase 5 — Fixpoint Trees](phase-5-fixpoint-trees.md)

Define `Fix<CompF>` — a heap-pointer computation tree using open-recursive algebraic types and the fixpoint combinator.


<a id="orgf765780"></a>

## [Phase 6 — Closures and Values](phase-6-closures.md)

Define the runtime value domain: numbers, booleans, closures, and environments that bind names to values at evaluation time.


<a id="org6d3c446"></a>

## [Phase 7 — Mendler Interpretation](phase-7-mendler.md)

Implement `mendler_run` using a genuine `mendler_para` combinator — a Mendler-style paramorphism over `Fix<CompF>` that is synchronous and fully `constexpr`-capable.


<a id="orgfa9f1c5"></a>

## [Phase 8 — Sender-Based Evaluation](phase-8-senders.md)

Implement `sender_mendler_run`, which rewrites evaluation as a graph of Beman Execution senders with `when_all` for argument-list parallelism.


<a id="orgd3a8af2"></a>

## [Phase 9 — Visualizing Execution](phase-9-graphs.md)

Use C++26 reflection to walk the nested sender type structure at compile time and emit Graphviz DOT output of the execution graph.


<a id="orgcb9f8cc"></a>

## [Phase 10 — Constexpr Pipeline](phase-10-constexpr.md)

Wire all phases together into a single `constexpr` pipeline and demonstrate evaluation results baked into the binary.


<a id="org28de832"></a>

## [Phase 11 — Real World Integration](phase-11-real-world.md)

Show how to embed the compiler in a real C++ program: pre-compiled closures, runtime environment injection, and FFI callbacks.


<a id="org7eee05e"></a>

## [Phase 12 — CPS](phase-12-cps.md)

Explore Continuation-Passing Style as an alternative evaluation backend for the closure evaluator.


<a id="org54ccbe3"></a>

## [Phase 13 — Conclusion](phase-13-conclusion.md)

Reflect on what the climb taught me about C++26 `constexpr` limits, fixpoint types, and the Mendler recursion scheme.


<a id="org1cd3e30"></a>

## [Phase 14 — Mutation: set!, begin, and a Store](phase-14-set-bang.md)

Come back down from the summit for `set!`: a shared store of mutable cells, an environment that binds names to locations, and the `begin` sequencing it takes to observe an effect.


<a id="org90fde2f"></a>

## [Phase 15 — Why Not call/cc: From Scheme to Common Lisp](phase-15-why-common-lisp.md)

*DRAFT — pending author revision.* Why `call/cc` stops here: a sender's one-shot completion contract cannot express a multishot continuation, Kiselyov's independent case against `call/cc`, and why Common Lisp's dynamic-extent control operators are exactly the discipline senders already enforce.


<a id="orgb51f89f"></a>

## [Phase 16 — Reading Common Lisp: Case, Keywords, and #'](phase-16-reading-common-lisp.md)

*DRAFT — pending author revision.* The `smdlisp` reader: case folding to uppercase at read time, keywords as a distinct datum kind, `;` comments as intertoken space, the maximal-munch fix behind DIV-0003 (the `1+` bug), and `#'` lowering to its own datum kind instead of a synthesized `(function x)` list.


<a id="orgac6a91a"></a>

## [Phase 17 — nil, t, and Living in a Lisp-2](phase-17-nil-t-lisp2.md)

*DRAFT — pending author revision.* The direct evaluator: one `is_true` function instead of Scheme's per-site `#f` encoding, a Lisp-2 environment where variable and function lookup never touch, `funcall` and `#'` as real call semantics, and the closure-capture-ownership question resolved with an arena instead of an owning pointer.


<a id="org9d51064"></a>

## [Phase 18 — setq, defun, progn: a Programmable Core](phase-18-setq-defun-progn.md)

*DRAFT — pending author revision.* `setq` returns the assigned value and `defun~/~defvar~/~defparameter` return the bound name, both departures from Scheme; the store from Phase 14 adapted for ANSI CL's return conventions; a mutable environment reference threaded through both the direct evaluator and a new continuation-passing backend; and a datum-arena lifetime bug caught at compile time while building a one-argument `compile_to_closure`.


<a id="orgbf18026"></a>

## [Phase 19 — block, catch, unwind-protect: One-Shot Control in CPS](phase-19-one-shot-control.md)

*DRAFT — pending author revision.* The one-shot nonlocal exits Phase 15 argued for, now built in both evaluators: `block` and `return-from` resolved lexically by name, `catch` and `throw` resolved dynamically by an evaluated tag, and `unwind-protect` running its cleanups on every way out. The lexical/dynamic distinction lands as two data structures with two lifetimes — an append-only slab of exit records a closure may capture, and a reused-slot stack of catch frames nothing can. Plus DIV-0011: an uncaught `throw` here runs cleanups that ANSI CL says should not run.


<a id="org48056d3"></a>

## [Phase 20 — defmacro: the Compiler Runs the Language](phase-20-defmacro.md)

*DRAFT — pending author revision.* Object-language macros: a `defmacro`-defined `my-when` is a Lisp lambda that `smdlisp` compiles with its own elaborator and runs with its own evaluator during the expansion pass — the compiler running the language it compiles, at compile time. The new machinery is a datum⇄value reification pair; the merge test is that `my-when` is `when`, whether written with `list~/~cons` or with a backquote template, plus an expansion-budget diagnostic for a macro that expands into itself.


<a id="org54f3ea9"></a>

## [Phase 21 — Common Lisp Control Flow as Sender Graphs](phase-21-sender-graphs.md)

*DRAFT — pending author revision.* The Common Lisp core evaluated over Beman Execution senders. The closure backends push a value, a diagnosed error, a `return-from` unwind and a `throw` unwind down one `result<value>` wire and tell them apart with sentinel messages compared by pointer identity; a sender has three channels natively, so the sentinels are deleted instead of ported. `unwind-protect` becomes an ordinary sender adapter whose three completion functions all funnel into one cleanup call, while dynamic-binding restore refuses to move out of ordinary C++ control flow at all. Plus a P2996 graph dump, and DIV-0015: this backend has no `constexpr` twin.


<a id="orgc08ad7b"></a>

## [Phase 22 — What We Left Out, and Why It Matters](phase-22-limitations.md)

*DRAFT — pending author revision.* The reflective finale. Most of decision D10's out-of-scope list — strings, floats, CLOS, `format`, `loop` — is incidental to Phase 15's thesis about one-shot control; two gaps are not: the sender backend doesn't cover multiple values (DIV-0017/0018), and a nonlocal exit carries only its primary value even where every other exit path carries all of them (DIV-0019). Plus five sharper structural limits worth a paragraph each, including DIV-0015's warning not to call all three backends compile-time evaluators.


<a id="org8f9d1a9"></a>

## [Phase 23 — Why Rebuild Rather Than Refactor](phase-23-why-rebuild.md)

*DRAFT — pending author revision.* The pivot works, and this is the argument for building it again anyway. Twenty divergence records, each locally correct and honestly written the same day, turn out on a single reading to be seven bills for one missing abstraction: a symbol is a `std::string_view` and so has nowhere to keep anything. Plus the freeze whose reason had evaporated, a test suite that had become a ratchet rather than a safety net, and a style document that was ratified and never adopted. Nothing is built; everything here is an argument, and arguments about software are cheap.


<a id="org7c72214"></a>

## [Phase 24 — Two Copies and a Fold That Stops](phase-24-substrate.md)

*DRAFT — pending author revision.* The new tree's `foundation/` starts as the reviewed union of two independent copies of the same code, and the merge is convincing mostly because there was nothing to merge — for five files the entire difference is the include guard, the namespace, and a comment saying it was adapted by copy. New in neither copy: a left fold that stops on the first failure, which `std::ranges::fold_left` can't do, and the Foldable and Traversable typeclasses the coding rules had required all along. The substrate has no behaviour of its own, so the law tests are the evidence.


<a id="org1bd73ad"></a>

## [Phase 25 — Interning Symbols, and Seven Divergences That Were One](phase-25-symbols.md)

*DRAFT — pending author revision.* The keystone. A symbol becomes an entry in a table with a name and three independently writable slots, identity becomes id comparison, and the table owns its characters so a compiled program stops pointing into the reader's arena. Six of the seven divergences close by construction and a seventh partly — in a component with no callers, since this tree has no reader and no evaluator yet. Also settles a question the plan left open: the table rides along into the running program.


<a id="org5eefbe4"></a>

## [Phase 26 — Reading More Than It Can Run](phase-26-reader-core-ast.md)

*DRAFT — pending author revision.* A readtable-shaped reader that implements full ANSI atom syntax before the evaluator can run any of it: strings, characters, `#(...)` vectors and every numeric-tower spelling read without complaint, and a literal with no machine representation yet is carried as its spelling. The datum tree and the core AST become instantiations of one container, so the Foldable and Traversable instances are written once and law-tested once. Plus an oracle test against the frozen pivot reader, three cases excluded because the oracle is the wrong answer there, and a GCC trunk misfold that the compile-time half of a twin test could not see.


<a id="orga555bc0"></a>

## [Phase 27 — The Elaborator Never Recurses](phase-27-elaboration.md)

*DRAFT — pending author revision.* A tree built children-before-parent has node indices in topological order, so ascending index order is a bottom-up catamorphism and descending order is top-down propagation. Elaboration becomes three index loops — atoms by `traverse`, roles by a descending pass, emission by a fold that stops on the first error — with no stack, no visitor that calls itself, and no bound on nesting depth. Plus the Lisp-2 rule as a consequence rather than a special case, and the one new node kind that decision D18's own exception licenses.


<a id="org9034ddf"></a>

## [Phase 28 — Three Channels, and a Machine Instead of a Fold](phase-28-three-channels.md)

*DRAFT — pending author revision.* An evaluated form finishes with a value, with a diagnosed error, or with an unwind in flight, and decision D13 makes those three alternatives of one type, so the pivot's sentinel messages compared by pointer identity are deleted instead of ported. `unwind-protect` is what the split costs, and it is rebuilt as a continuation frame that intercepts all three channels. Evaluation is the one traversal here the tree's own folds can't carry, so the evaluator is a small-step machine with defunctionalized continuations and its single loop in `foundation::trampoline` — which makes recursion depth and non-termination diagnosed capacities rather than compiler behaviour. Plus DIV-0009 closed: a recursive `defun` counts a list inside a `static_assert`.


<a id="orga074ad2"></a>

## [Phase 29 — Answering Phase 7, and One Invariant That Was Two](phase-29-answering-phase-7.md)

*DRAFT — pending author revision.* No step of the rebuild landed here. Phase 28 claimed evaluation is the one traversal that can't be a fold, and phase 7 of this series had already evaluated `if` inside a Mendler fold; what actually rules the fold out is the representation, since a columnar tree has no pointer to chase and no descent for a recurse-knob to choose. The machine turns out to be a scheme anyway — an unfold, with `step` as the coalgebra — and the reference doc behind phases 5 through 8 gets a status header, because its warning that a CPS trampoline linearizes independent arguments inverts on a column. Re-reading the argument against the code found the defect: `tagged_tree` was carrying two invariants as one, and children-before-parent, which every scheme depends on, was documented in a comment and checked nowhere.


<a id="org0eaf4c0"></a>

# Table of Contents


# Table of Contents

1.  [Overview](#orgb770e41)
2.  [Blog Posts](#org5277adf)
3.  [Table of Contents](#org0eaf4c0)
