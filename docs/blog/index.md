- [Overview](#orgf5e6c91)
- [Blog Posts](#org05a6d0b)
  - [[Phase 0 — Introduction and Motivation](phase-0-intro.md)](#org642d615)
  - [[Phase 1 — Foundation](phase-1-foundation.md)](#orged9f7d4)
  - [[Phase 2 — Front End](phase-2-front-end.md)](#org7659082)
  - [[Phase 3 — Reader](phase-3-reader.md)](#org84845d8)
  - [[Phase 4 — Elaboration](phase-4-elaboration.md)](#orgab2af07)
  - [[Phase 5 — Fixpoint Trees](phase-5-fixpoint-trees.md)](#org758b409)
  - [[Phase 6 — Closures and Values](phase-6-closures.md)](#org45ca079)
  - [[Phase 7 — Mendler Interpretation](phase-7-mendler.md)](#orgc7f7a81)
  - [[Phase 8 — Sender-Based Evaluation](phase-8-senders.md)](#org311eefe)
  - [[Phase 9 — Visualizing Execution](phase-9-graphs.md)](#org6810d7e)
  - [[Phase 10 — Constexpr Pipeline](phase-10-constexpr.md)](#org19fee50)
  - [[Phase 11 — Real World Integration](phase-11-real-world.md)](#org969e1f3)
  - [[Phase 12 — CPS](phase-12-cps.md)](#orga24b32c)
  - [[Phase 13 — Conclusion](phase-13-conclusion.md)](#org9f68b22)
  - [[Phase 14 — Mutation: set!, begin, and a Store](phase-14-set-bang.md)](#org2fb3d9c)
  - [[Phase 15 — Why Not call/cc: From Scheme to Common Lisp](phase-15-why-common-lisp.md)](#org9bdcd91)
- [Table of Contents](#orga835586)



<a id="orgf5e6c91"></a>

# Overview

This is the complete blog series documenting the implementation of a Scheme-light compiler in C++26, compiling entirely at compile-time inside the C++ constant evaluator. Each phase builds on the previous work, from zero-allocation parsing through fixpoint trees to Mendler-style evaluation and sender-based asynchronous execution.


<a id="org05a6d0b"></a>

# Blog Posts


<a id="org642d615"></a>

## [Phase 0 — Introduction and Motivation](phase-0-intro.md)

The overall design philosophy: expanding C++26 `constexpr` to prove what the constant evaluator can accomplish.


<a id="orged9f7d4"></a>

## [Phase 1 — Foundation](phase-1-foundation.md)

Establish the memory vocabulary: `result<T>`, `static_vector`, `Box<A>`, and the `Fix<F>` fixpoint combinator.


<a id="org7659082"></a>

## [Phase 2 — Front End](phase-2-front-end.md)

Zero-allocation combinator parsers using immutable cursors and applicative composition.


<a id="org84845d8"></a>

## [Phase 3 — Reader](phase-3-reader.md)

Translate source text into a raw `Datum` tree, treating all input as nested shapes with no semantic judgement.


<a id="orgab2af07"></a>

## [Phase 4 — Elaboration](phase-4-elaboration.md)

Classify `Datum` nodes into a typed core AST covering `if`, `lambda`, `let`, `let*`, `define`, and `quote`.


<a id="org758b409"></a>

## [Phase 5 — Fixpoint Trees](phase-5-fixpoint-trees.md)

Define `Fix<CompF>` — a heap-pointer computation tree using open-recursive algebraic types and the fixpoint combinator.


<a id="org45ca079"></a>

## [Phase 6 — Closures and Values](phase-6-closures.md)

Define the runtime value domain: numbers, booleans, closures, and environments that bind names to values at evaluation time.


<a id="orgc7f7a81"></a>

## [Phase 7 — Mendler Interpretation](phase-7-mendler.md)

Implement `mendler_run` using a genuine `mendler_para` combinator — a Mendler-style paramorphism over `Fix<CompF>` that is synchronous and fully `constexpr`-capable.


<a id="org311eefe"></a>

## [Phase 8 — Sender-Based Evaluation](phase-8-senders.md)

Implement `sender_mendler_run`, which rewrites evaluation as a graph of Beman Execution senders with `when_all` for argument-list parallelism.


<a id="org6810d7e"></a>

## [Phase 9 — Visualizing Execution](phase-9-graphs.md)

Use C++26 reflection to walk the nested sender type structure at compile time and emit Graphviz DOT output of the execution graph.


<a id="org19fee50"></a>

## [Phase 10 — Constexpr Pipeline](phase-10-constexpr.md)

Wire all phases together into a single `constexpr` pipeline and demonstrate evaluation results baked into the binary.


<a id="org969e1f3"></a>

## [Phase 11 — Real World Integration](phase-11-real-world.md)

Show how to embed the compiler in a real C++ program: pre-compiled closures, runtime environment injection, and FFI callbacks.


<a id="orga24b32c"></a>

## [Phase 12 — CPS](phase-12-cps.md)

Explore Continuation-Passing Style as an alternative evaluation backend for the closure evaluator.


<a id="org9f68b22"></a>

## [Phase 13 — Conclusion](phase-13-conclusion.md)

Reflect on what the climb taught me about C++26 `constexpr` limits, fixpoint types, and the Mendler recursion scheme.


<a id="org2fb3d9c"></a>

## [Phase 14 — Mutation: set!, begin, and a Store](phase-14-set-bang.md)

Come back down from the summit for `set!`: a shared store of mutable cells, an environment that binds names to locations, and the `begin` sequencing it takes to observe an effect.


<a id="org9bdcd91"></a>

## [Phase 15 — Why Not call/cc: From Scheme to Common Lisp](phase-15-why-common-lisp.md)

*DRAFT — pending author revision.* Why `call/cc` stops here: a sender's one-shot completion contract cannot express a multishot continuation, Kiselyov's independent case against `call/cc`, and why Common Lisp's dynamic-extent control operators are exactly the discipline senders already enforce.


<a id="orga835586"></a>

# Table of Contents


# Table of Contents

1.  [Overview](#orgf5e6c91)
2.  [Blog Posts](#org05a6d0b)
3.  [Table of Contents](#orga835586)
