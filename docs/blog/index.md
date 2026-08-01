- [Overview](#org4486dd7)
- [Blog Posts](#orgac7256d)
  - [[Phase 0 — Introduction and Motivation](phase-0-intro.md)](#orgc066339)
  - [[Phase 1 — Foundation](phase-1-foundation.md)](#orgea51317)
  - [[Phase 2 — Front End](phase-2-front-end.md)](#org3d552ae)
  - [[Phase 3 — Reader](phase-3-reader.md)](#org44b28f9)
  - [[Phase 4 — Elaboration](phase-4-elaboration.md)](#org6cb320f)
  - [[Phase 5 — Fixpoint Trees](phase-5-fixpoint-trees.md)](#orgb272846)
  - [[Phase 6 — Closures and Values](phase-6-closures.md)](#org6bc4daf)
  - [[Phase 7 — Mendler Interpretation](phase-7-mendler.md)](#org345c9ed)
  - [[Phase 8 — Sender-Based Evaluation](phase-8-senders.md)](#org0ba21cc)
  - [[Phase 9 — Visualizing Execution](phase-9-graphs.md)](#org0f3e642)
  - [[Phase 10 — Constexpr Pipeline](phase-10-constexpr.md)](#orgfa45fd6)
  - [[Phase 11 — Real World Integration](phase-11-real-world.md)](#org07f45fa)
  - [[Phase 12 — CPS](phase-12-cps.md)](#org43637da)
  - [[Phase 13 — Conclusion](phase-13-conclusion.md)](#org9e86ea2)
  - [[Phase 14 — Mutation: set!, begin, and a Store](phase-14-set-bang.md)](#orgd2f15d4)
  - [[Phase 15 — Why Not call/cc: From Scheme to Common Lisp](phase-15-why-common-lisp.md)](#org8f8a7cb)
  - [[Phase 16 — Reading Common Lisp: Case, Keywords, and #'](phase-16-reading-common-lisp.md)](#org0a85894)
  - [[Phase 17 — nil, t, and Living in a Lisp-2](phase-17-nil-t-lisp2.md)](#org9a88743)
  - [[Phase 18 — setq, defun, progn: a Programmable Core](phase-18-setq-defun-progn.md)](#org6e954bc)
  - [[Phase 19 — block, catch, unwind-protect: One-Shot Control in CPS](phase-19-one-shot-control.md)](#org108718d)
  - [[Phase 20 — defmacro: the Compiler Runs the Language](phase-20-defmacro.md)](#org7d1c522)
  - [[Phase 21 — Common Lisp Control Flow as Sender Graphs](phase-21-sender-graphs.md)](#org22dc0ad)
  - [[Phase 22 — What We Left Out, and Why It Matters](phase-22-limitations.md)](#org287a733)
- [Table of Contents](#orgaf09130)



<a id="org4486dd7"></a>

# Overview

This is the complete blog series documenting the implementation of a Scheme-light compiler in C++26, compiling entirely at compile-time inside the C++ constant evaluator. Each phase builds on the previous work, from zero-allocation parsing through fixpoint trees to Mendler-style evaluation and sender-based asynchronous execution.


<a id="orgac7256d"></a>

# Blog Posts


<a id="orgc066339"></a>

## [Phase 0 — Introduction and Motivation](phase-0-intro.md)

The overall design philosophy: expanding C++26 `constexpr` to prove what the constant evaluator can accomplish.


<a id="orgea51317"></a>

## [Phase 1 — Foundation](phase-1-foundation.md)

Establish the memory vocabulary: `result<T>`, `static_vector`, `Box<A>`, and the `Fix<F>` fixpoint combinator.


<a id="org3d552ae"></a>

## [Phase 2 — Front End](phase-2-front-end.md)

Zero-allocation combinator parsers using immutable cursors and applicative composition.


<a id="org44b28f9"></a>

## [Phase 3 — Reader](phase-3-reader.md)

Translate source text into a raw `Datum` tree, treating all input as nested shapes with no semantic judgement.


<a id="org6cb320f"></a>

## [Phase 4 — Elaboration](phase-4-elaboration.md)

Classify `Datum` nodes into a typed core AST covering `if`, `lambda`, `let`, `let*`, `define`, and `quote`.


<a id="orgb272846"></a>

## [Phase 5 — Fixpoint Trees](phase-5-fixpoint-trees.md)

Define `Fix<CompF>` — a heap-pointer computation tree using open-recursive algebraic types and the fixpoint combinator.


<a id="org6bc4daf"></a>

## [Phase 6 — Closures and Values](phase-6-closures.md)

Define the runtime value domain: numbers, booleans, closures, and environments that bind names to values at evaluation time.


<a id="org345c9ed"></a>

## [Phase 7 — Mendler Interpretation](phase-7-mendler.md)

Implement `mendler_run` using a genuine `mendler_para` combinator — a Mendler-style paramorphism over `Fix<CompF>` that is synchronous and fully `constexpr`-capable.


<a id="org0ba21cc"></a>

## [Phase 8 — Sender-Based Evaluation](phase-8-senders.md)

Implement `sender_mendler_run`, which rewrites evaluation as a graph of Beman Execution senders with `when_all` for argument-list parallelism.


<a id="org0f3e642"></a>

## [Phase 9 — Visualizing Execution](phase-9-graphs.md)

Use C++26 reflection to walk the nested sender type structure at compile time and emit Graphviz DOT output of the execution graph.


<a id="orgfa45fd6"></a>

## [Phase 10 — Constexpr Pipeline](phase-10-constexpr.md)

Wire all phases together into a single `constexpr` pipeline and demonstrate evaluation results baked into the binary.


<a id="org07f45fa"></a>

## [Phase 11 — Real World Integration](phase-11-real-world.md)

Show how to embed the compiler in a real C++ program: pre-compiled closures, runtime environment injection, and FFI callbacks.


<a id="org43637da"></a>

## [Phase 12 — CPS](phase-12-cps.md)

Explore Continuation-Passing Style as an alternative evaluation backend for the closure evaluator.


<a id="org9e86ea2"></a>

## [Phase 13 — Conclusion](phase-13-conclusion.md)

Reflect on what the climb taught me about C++26 `constexpr` limits, fixpoint types, and the Mendler recursion scheme.


<a id="orgd2f15d4"></a>

## [Phase 14 — Mutation: set!, begin, and a Store](phase-14-set-bang.md)

Come back down from the summit for `set!`: a shared store of mutable cells, an environment that binds names to locations, and the `begin` sequencing it takes to observe an effect.


<a id="org8f8a7cb"></a>

## [Phase 15 — Why Not call/cc: From Scheme to Common Lisp](phase-15-why-common-lisp.md)

*DRAFT — pending author revision.* Why `call/cc` stops here: a sender's one-shot completion contract cannot express a multishot continuation, Kiselyov's independent case against `call/cc`, and why Common Lisp's dynamic-extent control operators are exactly the discipline senders already enforce.


<a id="org0a85894"></a>

## [Phase 16 — Reading Common Lisp: Case, Keywords, and #'](phase-16-reading-common-lisp.md)

*DRAFT — pending author revision.* The `smdlisp` reader: case folding to uppercase at read time, keywords as a distinct datum kind, `;` comments as intertoken space, the maximal-munch fix behind DIV-0003 (the `1+` bug), and `#'` lowering to its own datum kind instead of a synthesized `(function x)` list.


<a id="org9a88743"></a>

## [Phase 17 — nil, t, and Living in a Lisp-2](phase-17-nil-t-lisp2.md)

*DRAFT — pending author revision.* The direct evaluator: one `is_true` function instead of Scheme's per-site `#f` encoding, a Lisp-2 environment where variable and function lookup never touch, `funcall` and `#'` as real call semantics, and the closure-capture-ownership question resolved with an arena instead of an owning pointer.


<a id="org6e954bc"></a>

## [Phase 18 — setq, defun, progn: a Programmable Core](phase-18-setq-defun-progn.md)

*DRAFT — pending author revision.* `setq` returns the assigned value and `defun~/~defvar~/~defparameter` return the bound name, both departures from Scheme; the store from Phase 14 adapted for ANSI CL's return conventions; a mutable environment reference threaded through both the direct evaluator and a new continuation-passing backend; and a datum-arena lifetime bug caught at compile time while building a one-argument `compile_to_closure`.


<a id="org108718d"></a>

## [Phase 19 — block, catch, unwind-protect: One-Shot Control in CPS](phase-19-one-shot-control.md)

*DRAFT — pending author revision.* The one-shot nonlocal exits Phase 15 argued for, now built in both evaluators: `block` and `return-from` resolved lexically by name, `catch` and `throw` resolved dynamically by an evaluated tag, and `unwind-protect` running its cleanups on every way out. The lexical/dynamic distinction lands as two data structures with two lifetimes — an append-only slab of exit records a closure may capture, and a reused-slot stack of catch frames nothing can. Plus DIV-0011: an uncaught `throw` here runs cleanups that ANSI CL says should not run.


<a id="org7d1c522"></a>

## [Phase 20 — defmacro: the Compiler Runs the Language](phase-20-defmacro.md)

*DRAFT — pending author revision.* Object-language macros: a `defmacro`-defined `my-when` is a Lisp lambda that `smdlisp` compiles with its own elaborator and runs with its own evaluator during the expansion pass — the compiler running the language it compiles, at compile time. The new machinery is a datum⇄value reification pair; the merge test is that `my-when` is `when`, whether written with `list~/~cons` or with a backquote template, plus an expansion-budget diagnostic for a macro that expands into itself.


<a id="org22dc0ad"></a>

## [Phase 21 — Common Lisp Control Flow as Sender Graphs](phase-21-sender-graphs.md)

*DRAFT — pending author revision.* The Common Lisp core evaluated over Beman Execution senders. The closure backends push a value, a diagnosed error, a `return-from` unwind and a `throw` unwind down one `result<value>` wire and tell them apart with sentinel messages compared by pointer identity; a sender has three channels natively, so the sentinels are deleted instead of ported. `unwind-protect` becomes an ordinary sender adapter whose three completion functions all funnel into one cleanup call, while dynamic-binding restore refuses to move out of ordinary C++ control flow at all. Plus a P2996 graph dump, and DIV-0015: this backend has no `constexpr` twin.


<a id="org287a733"></a>

## [Phase 22 — What We Left Out, and Why It Matters](phase-22-limitations.md)

*DRAFT — pending author revision.* The reflective finale. Most of decision D10's out-of-scope list — strings, floats, CLOS, `format`, `loop` — is incidental to Phase 15's thesis about one-shot control; two gaps are not: the sender backend doesn't cover multiple values (DIV-0017/0018), and a nonlocal exit carries only its primary value even where every other exit path carries all of them (DIV-0019). Plus five sharper structural limits worth a paragraph each, including DIV-0015's warning not to call all three backends compile-time evaluators.


<a id="orgaf09130"></a>

# Table of Contents


# Table of Contents

1.  [Overview](#org4486dd7)
2.  [Blog Posts](#orgac7256d)
3.  [Table of Contents](#orgaf09130)
