- [Overview](#org56ba4e6)
- [Blog Posts](#orgdab7f41)
  - [[Phase 0 — Introduction and Motivation](phase-0-intro.md)](#org5b0ce5f)
  - [[Phase 1 — Foundation](phase-1-foundation.md)](#orgd610df0)
  - [[Phase 2 — Front End](phase-2-front-end.md)](#org45d29d3)
  - [[Phase 3 — Reader](phase-3-reader.md)](#org05545f9)
  - [[Phase 4 — Elaboration](phase-4-elaboration.md)](#org6734865)
  - [[Phase 5 — Fixpoint Trees](phase-5-fixpoint-trees.md)](#org0b082bc)
  - [[Phase 6 — Closures and Values](phase-6-closures.md)](#orga1a8a4a)
  - [[Phase 7 — Mendler Interpretation](phase-7-mendler.md)](#orgd466c65)
  - [[Phase 8 — Sender-Based Evaluation](phase-8-senders.md)](#org76d930b)
  - [[Phase 9 — Visualizing Execution](phase-9-graphs.md)](#orgbc2cf69)
  - [[Phase 10 — Constexpr Pipeline](phase-10-constexpr.md)](#org0a2ebea)
  - [[Phase 11 — Real World Integration](phase-11-real-world.md)](#org36e347e)
  - [[Phase 12 — CPS](phase-12-cps.md)](#org853de35)
  - [[Phase 13 — Conclusion](phase-13-conclusion.md)](#org0ef9a77)
- [Table of Contents](#org8c98b1c)



<a id="org56ba4e6"></a>

# Overview

This is the complete blog series documenting the implementation of a Scheme-light compiler in C++26, compiling entirely at compile-time inside the C++ constant evaluator. Each phase builds on the previous work, from zero-allocation parsing through fixpoint trees to Mendler-style evaluation and sender-based asynchronous execution.


<a id="orgdab7f41"></a>

# Blog Posts


<a id="org5b0ce5f"></a>

## [Phase 0 — Introduction and Motivation](phase-0-intro.md)

The overall design philosophy: expanding C++26 `constexpr` to prove what the constant evaluator can accomplish.


<a id="orgd610df0"></a>

## [Phase 1 — Foundation](phase-1-foundation.md)

Establish the memory vocabulary: `result<T>`, `static_vector`, `Box<A>`, and the `Fix<F>` fixpoint combinator.


<a id="org45d29d3"></a>

## [Phase 2 — Front End](phase-2-front-end.md)

Zero-allocation combinator parsers using immutable cursors and applicative composition.


<a id="org05545f9"></a>

## [Phase 3 — Reader](phase-3-reader.md)

Translate source text into a raw `Datum` tree, treating all input as nested shapes with no semantic judgement.


<a id="org6734865"></a>

## [Phase 4 — Elaboration](phase-4-elaboration.md)

Classify `Datum` nodes into a typed core AST covering `if`, `lambda`, `let`, `let*`, `define`, and `quote`.


<a id="org0b082bc"></a>

## [Phase 5 — Fixpoint Trees](phase-5-fixpoint-trees.md)

Define `Fix<CompF>` — a heap-pointer computation tree using open-recursive algebraic types and the fixpoint combinator.


<a id="orga1a8a4a"></a>

## [Phase 6 — Closures and Values](phase-6-closures.md)

Define the runtime value domain: numbers, booleans, closures, and environments that bind names to values at evaluation time.


<a id="orgd466c65"></a>

## [Phase 7 — Mendler Interpretation](phase-7-mendler.md)

Implement `mendler_run` using a genuine `mendler_para` combinator — a Mendler-style paramorphism over `Fix<CompF>` that is synchronous and fully `constexpr`-capable.


<a id="org76d930b"></a>

## [Phase 8 — Sender-Based Evaluation](phase-8-senders.md)

Implement `sender_mendler_run`, which rewrites evaluation as a graph of Beman Execution senders with `when_all` for argument-list parallelism.


<a id="orgbc2cf69"></a>

## [Phase 9 — Visualizing Execution](phase-9-graphs.md)

Use C++26 reflection to walk the nested sender type structure at compile time and emit Graphviz DOT output of the execution graph.


<a id="org0a2ebea"></a>

## [Phase 10 — Constexpr Pipeline](phase-10-constexpr.md)

Wire all phases together into a single `constexpr` pipeline and demonstrate evaluation results baked into the binary.


<a id="org36e347e"></a>

## [Phase 11 — Real World Integration](phase-11-real-world.md)

Show how to embed the compiler in a real C++ program: pre-compiled closures, runtime environment injection, and FFI callbacks.


<a id="org853de35"></a>

## [Phase 12 — CPS](phase-12-cps.md)

Explore Continuation-Passing Style as an alternative evaluation backend for the closure evaluator.


<a id="org0ef9a77"></a>

## [Phase 13 — Conclusion](phase-13-conclusion.md)

Reflect on what the climb taught me about C++26 `constexpr` limits, fixpoint types, and the Mendler recursion scheme.


<a id="org8c98b1c"></a>

# Table of Contents


# Table of Contents

1.  [Overview](#org56ba4e6)
2.  [Blog Posts](#orgdab7f41)
3.  [Table of Contents](#org8c98b1c)
