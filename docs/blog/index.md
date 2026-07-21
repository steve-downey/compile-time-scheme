- [Overview](#org7214ec1)
- [Blog Posts](#org77b1bd5)
  - [[Phase 0 — Introduction and Motivation](phase-0-intro.md)](#orge568dbf)
  - [[Phase 1 — Foundation](phase-1-foundation.md)](#org1ae3771)
  - [[Phase 2 — Front End](phase-2-front-end.md)](#org98d121f)
  - [[Phase 3 — Reader](phase-3-reader.md)](#org80cd313)
  - [[Phase 4 — Elaboration](phase-4-elaboration.md)](#org0fa43d5)
  - [[Phase 5 — Fixpoint Trees](phase-5-fixpoint-trees.md)](#org02b3b4e)
  - [[Phase 6 — Closures and Values](phase-6-closures.md)](#orgb81bb44)
  - [[Phase 7 — Mendler Interpretation](phase-7-mendler.md)](#org58aa1ff)
  - [[Phase 8 — Sender-Based Evaluation](phase-8-senders.md)](#orgcc5884e)
  - [[Phase 9 — Visualizing Execution](phase-9-graphs.md)](#org551d043)
  - [[Phase 10 — Constexpr Pipeline](phase-10-constexpr.md)](#org6260301)
  - [[Phase 11 — Real World Integration](phase-11-real-world.md)](#orgda2aca7)
  - [[Phase 12 — CPS](phase-12-cps.md)](#org6cbd8d7)
  - [[Phase 13 — Conclusion](phase-13-conclusion.md)](#org26ff2c5)
  - [[Phase 14 — Mutation: set!, begin, and a Store](phase-14-set-bang.md)](#orgf414d34)
  - [[Phase 15 — Why Not call/cc: From Scheme to Common Lisp](phase-15-why-common-lisp.md)](#org23a5cd9)
  - [[Phase 16 — Reading Common Lisp: Case, Keywords, and #'](phase-16-reading-common-lisp.md)](#orgafd1964)
  - [[Phase 17 — nil, t, and Living in a Lisp-2](phase-17-nil-t-lisp2.md)](#org32d76b0)
  - [[Phase 18 — setq, defun, progn: A Programmable Core](phase-18-setq-defun-progn.md)](#org971682d)
- [Table of Contents](#org796f884)



<a id="org7214ec1"></a>

# Overview

This is the complete blog series documenting the implementation of a Scheme-light compiler in C++26, compiling entirely at compile-time inside the C++ constant evaluator. Each phase builds on the previous work, from zero-allocation parsing through fixpoint trees to Mendler-style evaluation and sender-based asynchronous execution.


<a id="org77b1bd5"></a>

# Blog Posts


<a id="orge568dbf"></a>

## [Phase 0 — Introduction and Motivation](phase-0-intro.md)

The overall design philosophy: expanding C++26 `constexpr` to prove what the constant evaluator can accomplish.


<a id="org1ae3771"></a>

## [Phase 1 — Foundation](phase-1-foundation.md)

Establish the memory vocabulary: `result<T>`, `static_vector`, `Box<A>`, and the `Fix<F>` fixpoint combinator.


<a id="org98d121f"></a>

## [Phase 2 — Front End](phase-2-front-end.md)

Zero-allocation combinator parsers using immutable cursors and applicative composition.


<a id="org80cd313"></a>

## [Phase 3 — Reader](phase-3-reader.md)

Translate source text into a raw `Datum` tree, treating all input as nested shapes with no semantic judgement.


<a id="org0fa43d5"></a>

## [Phase 4 — Elaboration](phase-4-elaboration.md)

Classify `Datum` nodes into a typed core AST covering `if`, `lambda`, `let`, `let*`, `define`, and `quote`.


<a id="org02b3b4e"></a>

## [Phase 5 — Fixpoint Trees](phase-5-fixpoint-trees.md)

Define `Fix<CompF>` — a heap-pointer computation tree using open-recursive algebraic types and the fixpoint combinator.


<a id="orgb81bb44"></a>

## [Phase 6 — Closures and Values](phase-6-closures.md)

Define the runtime value domain: numbers, booleans, closures, and environments that bind names to values at evaluation time.


<a id="org58aa1ff"></a>

## [Phase 7 — Mendler Interpretation](phase-7-mendler.md)

Implement `mendler_run` using a genuine `mendler_para` combinator — a Mendler-style paramorphism over `Fix<CompF>` that is synchronous and fully `constexpr`-capable.


<a id="orgcc5884e"></a>

## [Phase 8 — Sender-Based Evaluation](phase-8-senders.md)

Implement `sender_mendler_run`, which rewrites evaluation as a graph of Beman Execution senders with `when_all` for argument-list parallelism.


<a id="org551d043"></a>

## [Phase 9 — Visualizing Execution](phase-9-graphs.md)

Use C++26 reflection to walk the nested sender type structure at compile time and emit Graphviz DOT output of the execution graph.


<a id="org6260301"></a>

## [Phase 10 — Constexpr Pipeline](phase-10-constexpr.md)

Wire all phases together into a single `constexpr` pipeline and demonstrate evaluation results baked into the binary.


<a id="orgda2aca7"></a>

## [Phase 11 — Real World Integration](phase-11-real-world.md)

Show how to embed the compiler in a real C++ program: pre-compiled closures, runtime environment injection, and FFI callbacks.


<a id="org6cbd8d7"></a>

## [Phase 12 — CPS](phase-12-cps.md)

Explore Continuation-Passing Style as an alternative evaluation backend for the closure evaluator.


<a id="org26ff2c5"></a>

## [Phase 13 — Conclusion](phase-13-conclusion.md)

Reflect on what the climb taught me about C++26 `constexpr` limits, fixpoint types, and the Mendler recursion scheme.


<a id="orgf414d34"></a>

## [Phase 14 — Mutation: set!, begin, and a Store](phase-14-set-bang.md)

Come back down from the summit for `set!`: a shared store of mutable cells, an environment that binds names to locations, and the `begin` sequencing it takes to observe an effect.


<a id="org23a5cd9"></a>

## [Phase 15 — Why Not call/cc: From Scheme to Common Lisp](phase-15-why-common-lisp.md)

*DRAFT — pending author revision.* Why `call/cc` stops here: a sender's one-shot completion contract cannot express a multishot continuation, Kiselyov's independent case against `call/cc`, and why Common Lisp's dynamic-extent control operators are exactly the discipline senders already enforce.


<a id="orgafd1964"></a>

## [Phase 16 — Reading Common Lisp: Case, Keywords, and #'](phase-16-reading-common-lisp.md)

*DRAFT — pending author revision.* The `smdlisp` reader: case folding to uppercase at read time, keywords as a distinct datum kind, `;` comments as intertoken space, the maximal-munch fix behind DIV-0003 (the `1+` bug), and `#'` lowering to its own datum kind instead of a synthesized `(function x)` list.


<a id="org32d76b0"></a>

## [Phase 17 — nil, t, and Living in a Lisp-2](phase-17-nil-t-lisp2.md)

*DRAFT — pending author revision.* The direct evaluator: one `is_true` function instead of Scheme's per-site `#f` encoding, a Lisp-2 environment where variable and function lookup never touch, `funcall` and `#'` as real call semantics, and the closure-capture-ownership question resolved with an arena instead of an owning pointer.


<a id="org971682d"></a>

## [Phase 18 — setq, defun, progn: A Programmable Core](phase-18-setq-defun-progn.md)

*DRAFT — pending author revision.* The CPS/closure backend: `progn` compiling to continuation chaining, why the frozen Scheme original's refusal to support `define` in expression position meant `setq~/~defun` were a real design question rather than a port, the reserve-then-patch trick for self-recursive `defun` worked out again for continuation-passing code, and a caller-owned-datum-arena lifetime bug the merge-criterion tests found along the way.


<a id="org796f884"></a>

# Table of Contents


# Table of Contents

1.  [Overview](#org7214ec1)
2.  [Blog Posts](#org77b1bd5)
3.  [Table of Contents](#org796f884)
