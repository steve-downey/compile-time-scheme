- [Overview](#orgc217160)
- [Blog Posts](#orgd516d2e)
  - [[Phase 0 — Introduction and Motivation](phase-0-intro.md)](#orgcff0f43)
  - [[Phase 1 — Foundation](phase-1-foundation.md)](#orge953012)
  - [[Phase 2 — Front End](phase-2-front-end.md)](#orgc222ac5)
  - [[Phase 3 — Reader](phase-3-reader.md)](#orgc9f1411)
  - [[Phase 4 — Elaboration](phase-4-elaboration.md)](#orga98ae88)
  - [[Phase 5 — Fixpoint Trees](phase-5-fixpoint-trees.md)](#org3f69cba)
  - [[Phase 6 — Closures and Values](phase-6-closures.md)](#orgd2bfcde)
  - [[Phase 7 — Mendler Interpretation](phase-7-mendler.md)](#org40ad51b)
  - [[Phase 8 — Sender-Based Evaluation](phase-8-senders.md)](#org1e8bb14)
  - [[Phase 9 — Visualizing Execution](phase-9-graphs.md)](#org2cd1de1)
  - [[Phase 10 — Constexpr Pipeline](phase-10-constexpr.md)](#org3729949)
  - [[Phase 11 — Real World Integration](phase-11-real-world.md)](#orgc97cce8)
  - [[Phase 12 — CPS](phase-12-cps.md)](#org05e66e8)
  - [[Phase 13 — Conclusion](phase-13-conclusion.md)](#orgb4499ae)
  - [[Phase 14 — Mutation: set!, begin, and a Store](phase-14-set-bang.md)](#org607c7ef)
  - [[Phase 15 — Why Not call/cc: From Scheme to Common Lisp](phase-15-why-common-lisp.md)](#org6da7a06)
  - [[Phase 16 — Reading Common Lisp: Case, Keywords, and #'](phase-16-reading-common-lisp.md)](#orgf0bc14f)
- [Table of Contents](#org7c6d251)



<a id="orgc217160"></a>

# Overview

This is the complete blog series documenting the implementation of a Scheme-light compiler in C++26, compiling entirely at compile-time inside the C++ constant evaluator. Each phase builds on the previous work, from zero-allocation parsing through fixpoint trees to Mendler-style evaluation and sender-based asynchronous execution.


<a id="orgd516d2e"></a>

# Blog Posts


<a id="orgcff0f43"></a>

## [Phase 0 — Introduction and Motivation](phase-0-intro.md)

The overall design philosophy: expanding C++26 `constexpr` to prove what the constant evaluator can accomplish.


<a id="orge953012"></a>

## [Phase 1 — Foundation](phase-1-foundation.md)

Establish the memory vocabulary: `result<T>`, `static_vector`, `Box<A>`, and the `Fix<F>` fixpoint combinator.


<a id="orgc222ac5"></a>

## [Phase 2 — Front End](phase-2-front-end.md)

Zero-allocation combinator parsers using immutable cursors and applicative composition.


<a id="orgc9f1411"></a>

## [Phase 3 — Reader](phase-3-reader.md)

Translate source text into a raw `Datum` tree, treating all input as nested shapes with no semantic judgement.


<a id="orga98ae88"></a>

## [Phase 4 — Elaboration](phase-4-elaboration.md)

Classify `Datum` nodes into a typed core AST covering `if`, `lambda`, `let`, `let*`, `define`, and `quote`.


<a id="org3f69cba"></a>

## [Phase 5 — Fixpoint Trees](phase-5-fixpoint-trees.md)

Define `Fix<CompF>` — a heap-pointer computation tree using open-recursive algebraic types and the fixpoint combinator.


<a id="orgd2bfcde"></a>

## [Phase 6 — Closures and Values](phase-6-closures.md)

Define the runtime value domain: numbers, booleans, closures, and environments that bind names to values at evaluation time.


<a id="org40ad51b"></a>

## [Phase 7 — Mendler Interpretation](phase-7-mendler.md)

Implement `mendler_run` using a genuine `mendler_para` combinator — a Mendler-style paramorphism over `Fix<CompF>` that is synchronous and fully `constexpr`-capable.


<a id="org1e8bb14"></a>

## [Phase 8 — Sender-Based Evaluation](phase-8-senders.md)

Implement `sender_mendler_run`, which rewrites evaluation as a graph of Beman Execution senders with `when_all` for argument-list parallelism.


<a id="org2cd1de1"></a>

## [Phase 9 — Visualizing Execution](phase-9-graphs.md)

Use C++26 reflection to walk the nested sender type structure at compile time and emit Graphviz DOT output of the execution graph.


<a id="org3729949"></a>

## [Phase 10 — Constexpr Pipeline](phase-10-constexpr.md)

Wire all phases together into a single `constexpr` pipeline and demonstrate evaluation results baked into the binary.


<a id="orgc97cce8"></a>

## [Phase 11 — Real World Integration](phase-11-real-world.md)

Show how to embed the compiler in a real C++ program: pre-compiled closures, runtime environment injection, and FFI callbacks.


<a id="org05e66e8"></a>

## [Phase 12 — CPS](phase-12-cps.md)

Explore Continuation-Passing Style as an alternative evaluation backend for the closure evaluator.


<a id="orgb4499ae"></a>

## [Phase 13 — Conclusion](phase-13-conclusion.md)

Reflect on what the climb taught me about C++26 `constexpr` limits, fixpoint types, and the Mendler recursion scheme.


<a id="org607c7ef"></a>

## [Phase 14 — Mutation: set!, begin, and a Store](phase-14-set-bang.md)

Come back down from the summit for `set!`: a shared store of mutable cells, an environment that binds names to locations, and the `begin` sequencing it takes to observe an effect.


<a id="org6da7a06"></a>

## [Phase 15 — Why Not call/cc: From Scheme to Common Lisp](phase-15-why-common-lisp.md)

*DRAFT — pending author revision.* Why `call/cc` stops here: a sender's one-shot completion contract cannot express a multishot continuation, Kiselyov's independent case against `call/cc`, and why Common Lisp's dynamic-extent control operators are exactly the discipline senders already enforce.


<a id="orgf0bc14f"></a>

## [Phase 16 — Reading Common Lisp: Case, Keywords, and #'](phase-16-reading-common-lisp.md)

*DRAFT — pending author revision.* The `smdlisp` reader: case folding to uppercase at read time, keywords as a distinct datum kind, `;` comments as intertoken space, the maximal-munch fix behind DIV-0003 (the `1+` bug), and `#'` lowering to its own datum kind instead of a synthesized `(function x)` list.


<a id="org7c6d251"></a>

# Table of Contents


# Table of Contents

1.  [Overview](#orgc217160)
2.  [Blog Posts](#orgd516d2e)
3.  [Table of Contents](#org7c6d251)
