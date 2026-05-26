# Building a Compile-Time Scheme in C++26: A Bottom-Up Tutorial

*Note: This tutorial also functions as a chronological, bottom-up outline of our compiler construction plan. It is a living document. When reality diverges from the plan, these docs MUST be updated to accurately reflect the true state of the codebase.*

Welcome to the SchemePoC compiler tutorial! Most C++ developers use compilers every day, but diving into compiler *internals*—especially one operating entirely at compile-time (`constexpr`) with functional programming techniques—can feel impenetrable.

This tutorial flips the perspective. Instead of presenting a finished monolith, it walks you from the absolute bottom (reading individual characters) to the very top (executing functional closures asynchronously), explaining the *why* of each phase.

---

## Phase 1: Foundation and Utilities

Before parsing a language, we need robust plumbing.
- **Implementation:** Setting up deterministic styles (`codestyle.org`), governance (`AGENTS.md`), and core C++ utilities (like `static_vector` and a monadic `result` type).

**The Why:** Compilers need strict error handling and memory management. In `constexpr` C++26, traditional heap allocation (like standard `std::vector`s carrying data into runtime) is prohibited from persisting. We establish zero-allocation tools like `static_vector` early to ensure everything we build can survive the compile-time execution environment boundaries.

---

## Phase 2: The Front-End (Lexing & Parser Combinators)

How do we turn raw strings like `"(+ 1 2)"` into something structural?
- **Implementation:** Building an input cursor, and implementing "Parser Combinators" (`functor`, `applicative`, `alternative`, `lexeme` operations).

**The Why:** Most textbook compilers either teach "recursive descent" parsing—writing a giant web of `while` and `if` statements that consume characters—or spend huge amounts of time on LL(1), LALR, the theory of context-free grammars, the Chomsky hierarchy, and parser/lexer generators. Recursive descent is widely used in real production compilers because deep context matters for error and warning reporting. However, for a deliberately simple language like Scheme, it's overkill. (Fun fact: S-expressions were originally intended to be a compiler IR, but the intended M-expression user syntax was abandoned early on). At the same time, we don't want the build-system complexity of a separate parser generator. Instead, we use [Parser Combinators](https://en.wikipedia.org/wiki/Parser_combinator) (popularized by Haskell libraries like [Parsec](https://hackage.haskell.org/package/parsec)).

In this model, a "parser" is simply a callable C++ object. You snap together small parsers to build larger ones. For instance, `parse_digit` combined with `parse_many` constructs a `parse_integer` parser. This keeps the code algebraic, highly testable, and cleanly composable without messy tracking of global state.

---

## Phase 3: The Reader and the `Datum` Tree

Lisp and Scheme are *homoiconic* languages; the code is written in the exact same format as the language's native data structures (nested lists).
- **Implementation:** Building the reader atom model (symbols, booleans, integers), constructing a fixed-capacity `Datum` tree, and parsing lists and quotes.

**The Why:** A common mistake is trying to figure out what code "means" at the exact moment you parse it. We don't do that. The Reader's *only* job is to convert strings into a tree of data (`DatumF`). It doesn't know that `(if #t 1 2)` is a conditional branch; it just sees a list of four items. By isolating syntax ingestion, the Reader remains extremely simple. To honor C++26 `constexpr` allocation limits, the tree is a "flat arena"—a fixed array of nodes referencing one another via integer indexes (`node_id`).

---

## Phase 4: Elaboration & Semantic Analysis

Now we teach the compiler what the code *means*.
- **Implementation:** Designing the `Core` language model. The Elaborator translates the raw `Datum` data tree into a `Core` semantic AST. We handle special forms (`if`, `quote`, `lambda`), recognize variables, and build a "direct evaluator" (`eval_direct`) to test basic arithmetic.

**The Why:** **Elaboration** is compiler jargon for semantic binding. The Elaborator walks the dumb list of atoms, recognizes that `if` is in the first position, and translates that specific `Datum` node into a strongly-typed `core_if` structural node. By separating this step, we centralize all of our syntax validation (e.g. "Did they pass too many arguments to `if`?"). If there's an error, it is caught here, natively, before execution ever begins.

---

## Phase 5: The Middle-End: Continuation-Passing Style

This is the central semantic pivot.
- **Implementation:** Introducing the Fixpoint playground, establishing a Continuation-Passing Style (CPS) engine, and defunctionalizing it.

**The Why:** How do you execute the AST? The naïve (`eval_direct`) approach uses standard C++ function calls holding the execution state on the C++ stack. A deeply nested Scheme program will cause a C++ stack overflow.

Instead, we use [Continuation-Passing Style (CPS)](https://en.wikipedia.org/wiki/Continuation-passing_style). Instead of functions ever using `return X;`, every expression takes a callback argument called a **continuation** (meaning "what to do next with the result").
When you add `1 + 2`, you don't return `3`. You pass `3` into the next function in line.

This model explicitly maps out control flow in data, preventing uncontrolled stack growth. It also sets up native support for advanced functional features like Tail-Call Optimization (TCO), because every sequence is merely a chained jump rather than a stack push.

---

## Phase 6: The Back-End & Closure Materialization

How do we actually run this?
- **Implementation:** Materializing closures over the CPS program. Implementing `lambda` syntax, runtime values, function application, capturing lexical scope, and negative compile tests.

**The Why:** A [Closure](https://en.wikipedia.org/wiki/Closure_(computer_programming)) in functional programming is just a code block tightly strapped to the contextual variables it needs to run. In C++, this is essentially a `struct` with some stored fields and an `operator()`.

Rather than using heavy polymorphic wrappers like `std::move_only_function` (which introduce type-erasure and abstraction taxes), we emit standard, statically-defined C++ wrapper classes generated by standard template expansion. This converts our functional Scheme AST into 100% equivalent, blazing-fast native C++ code without needing an intermediate virtual machine byte-code.

---

## Phase 7: Future Horizons Achieved: Async, Reflection, and Godbolt

Where did we go after the core language worked?
- **Implementation:** Godbolt deployment scripts natively translating multi-file tree dependency inclusion into single compiler explorer links, integrating Beman `std::execution` algorithms, building a scalable Sender backend directly upon the CPS program using C++26 standard co-routines, and injecting C++26 Reflection (`std::meta::info`) spikes to materialize structural aggregates on demand.

**The Why:** Because our core engine uses CPS (calling "what to do next"), it mapped *perfectly* onto asynchronous C++ code. Calling a continuation is essentially identical to chaining a `.then()` in standard C++ Async (`stdexec` / Send/Receiver models / Beman Tasks). When a Scheme user executes code, our Sender backend allows C++ to invisibly route the workloads asynchronously beneath the execution engine without any performance hit.

Finally, C++26 Reflection allows us to define aggregate structures internally on the fly. We dynamically compile environments and specific type layouts at compile time by iterating over data structures, handing the C++ runtime an incredibly optimized, natively compiled binary executable derived perfectly from Scheme text where dynamically parsed environments map to pure C++ POD structs directly in memory.

---
*Happy Hacking!*
