- [Overview](#org8dceb8e)
- [Blog Posts](#org0f6b5af)
  - [[Phase 0 — Introduction and Motivation](phase-0-intro.md)](#org8f9dab3)
  - [[Phase 1 — Foundation](phase-1-foundation.md)](#org381b753)
  - [[Phase 2 — Front End](phase-2-front-end.md)](#orgc16c212)
  - [[Phase 3 — Reader](phase-3-reader.md)](#orgc32c9af)
  - [[Phase 4 — Elaboration](phase-4-elaboration.md)](#orgb14d860)
  - [[Phase 5 — Fixpoint Trees](phase-5-fixpoint-trees.md)](#org84e7f98)
  - [[Phase 6 — Closures and Values](phase-6-closures.md)](#org0eaf039)
  - [[Phase 7 — Mendler Interpretation](phase-7-mendler.md)](#orgd3eef43)
  - [[Phase 8 — Sender-Based Evaluation](phase-8-senders.md)](#org72779cd)
  - [[Phase 9 — Visualizing Execution](phase-9-graphs.md)](#orgfe063f8)
  - [[Phase 10 — Constexpr Pipeline](phase-10-constexpr.md)](#orgaddee26)
  - [[Phase 11 — Real World Integration](phase-11-real-world.md)](#orgef2469d)
  - [[Phase 12 — CPS](phase-12-cps.md)](#org7169791)
  - [[Phase 13 — Conclusion](phase-13-conclusion.md)](#orgb9b2daa)
- [Table of Contents](#org740510a)



<a id="org8dceb8e"></a>

# Overview

This is the complete blog series documenting the implementation of a Scheme-light compiler in C++26, compiling entirely at compile-time inside the C++ constant evaluator. Each phase builds on the previous work, from zero-allocation parsing through fixpoint trees to Mendler-style evaluation and sender-based asynchronous execution.


<a id="org0f6b5af"></a>

# Blog Posts


<a id="org8f9dab3"></a>

## [Phase 0 — Introduction and Motivation](phase-0-intro.md)

The overall design philosophy: expanding C++26 `constexpr` to prove what the constant evaluator can accomplish.


<a id="org381b753"></a>

## [Phase 1 — Foundation](phase-1-foundation.md)

Establish the memory vocabulary: `result<T>`, `static_vector`, `Box<A>`, and the `Fix<F>` fixpoint combinator.


<a id="orgc16c212"></a>

## [Phase 2 — Front End](phase-2-front-end.md)

Zero-allocation combinator parsers using immutable cursors and applicative composition.


<a id="orgc32c9af"></a>

## [Phase 3 — Reader](phase-3-reader.md)

Translate source text into a raw `Datum` tree, treating all input as nested shapes with no semantic judgement.


<a id="orgb14d860"></a>

## [Phase 4 — Elaboration](phase-4-elaboration.md)

Classify `Datum` nodes into a typed core AST covering `if`, `lambda`, `let`, `let*`, `define`, and `quote`.


<a id="org84e7f98"></a>

## [Phase 5 — Fixpoint Trees](phase-5-fixpoint-trees.md)

Define `Fix<CompF>` — a heap-pointer computation tree using open-recursive algebraic types and the fixpoint combinator.


<a id="org0eaf039"></a>

## [Phase 6 — Closures and Values](phase-6-closures.md)

Define the runtime value domain: numbers, booleans, closures, and environments that bind names to values at evaluation time.


<a id="orgd3eef43"></a>

## [Phase 7 — Mendler Interpretation](phase-7-mendler.md)

Implement `mendler_run`, a Mendler-style fold over `Fix<CompF>` that is synchronous and fully `constexpr`-capable.


<a id="org72779cd"></a>

## [Phase 8 — Sender-Based Evaluation](phase-8-senders.md)

Implement `sender_mendler_run`, which rewrites evaluation as a graph of Beman Execution senders with `when_all` for argument-list parallelism.


<a id="orgfe063f8"></a>

## [Phase 9 — Visualizing Execution](phase-9-graphs.md)

Use C++26 reflection to walk the nested sender type structure at compile time and emit Graphviz DOT output of the execution graph.


<a id="orgaddee26"></a>

## [Phase 10 — Constexpr Pipeline](phase-10-constexpr.md)

Wire all phases together into a single `constexpr` pipeline and demonstrate evaluation results baked into the binary.


<a id="orgef2469d"></a>

## [Phase 11 — Real World Integration](phase-11-real-world.md)

Show how to embed the compiler in a real C++ program: pre-compiled closures, runtime environment injection, and FFI callbacks.


<a id="org7169791"></a>

## [Phase 12 — CPS](phase-12-cps.md)

Explore Continuation-Passing Style as an alternative evaluation backend for the closure evaluator.


<a id="orgb9b2daa"></a>

## [Phase 13 — Conclusion](phase-13-conclusion.md)

Reflect on what the climb taught me about C++26 `constexpr` limits, fixpoint types, and the Mendler recursion scheme.


<a id="org740510a"></a>

# Table of Contents


# Table of Contents

1.  [Overview](#org8dceb8e)
2.  [Blog Posts](#org0f6b5af)
3.  [Table of Contents](#org740510a)
