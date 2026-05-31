<div class="abstract" id="org8765249">
<p>
I am building a Scheme-light compiler that parses, elaborates, and executes Lisp code entirely during C++26 compilation.
But why go to such extreme lengths to force a dynamic language into a rigid <code>constexpr</code> context?
Because I want to see if the mountain can be climbed.
</p>

</div>


# The Motivation

> "Nobody climbs mountains for scientific reasons. Science is used to raise money for the expeditions, but you really climb for the hell of it." — Edmund Hillary

This project is a proof of concept targeting C++26 on GCC16. It implements a fully staged compiler pipeline entirely inside the C++ compile-time evaluation engine. I am transforming a source text string into a syntax tree, elaborating it into an abstract semantic core, converting it to Continuation-Passing Style (CPS), and executing it using closures and sender/receiver backends. And it does all of this strictly before the compiler ever emits a single byte of executable machine code.

Why? I am not building this because the software industry desperately needs a compile-time Scheme embedded in C++. I am building this to expand my own understanding of modern C++ static analysis and template meta-programming.


# Exploring `constexpr` Boundaries

Modern C++ has steadily expanded what can be accomplished at compile time. What started as simple constant variable expressions in C++11 has grown into a nearly Turing-complete sub-language. By C++26, the `constexpr` environment is capable of remarkably non-trivial computation.

I am testing the absolute limits of this environment. Building a functional Scheme compiler forces me to confront and solve the severe restrictions of compile-time C++. It bars me from utilizing dynamic memory allocations that outlive a single constant evaluation, forcing me to eschew standard heap pointers. It lacks support for traditional virtual polymorphism, demanding alternative strategies for dynamic dispatch. It limits recursion depths and prohibits casting tricks.

To survive this, I must model zero-allocation combinator parsers using immutable cursors. I am forced to represent complex recursive Abstract Syntax Trees using flat memory arenas and handle-based indirection. I have to orchestrate open recursion and fixed-point combinators just to represent heterogeneous lists.


# The Ascent: A Phase-by-Phase Guide

To tackle this complexity, the architecture is broken down into distinct, heavily isolated phases. Each addresses a specific limitation of C++ compile-time evaluation.


## Phase 1: Foundation

I establish the fundamental vocabulary for memory management. The core problem is that recursive structures mathematically require indirection, and standard C++ handles this via heap pointers which are forbidden across `constexpr` bounds. I solve this using a fixed-size contiguous memory block.

> -   **Arena:** A pre-allocated, flat block of memory used to store nodes sequentially. By referencing elements via integer indices instead of raw pointers, dynamic allocation is circumvented.
> -   **Open Recursion:** A technique for building recursive data types by accepting the recursive type as a parameter, allowing fixed-point combinators to tie the recursive knot.


## Phase 2: Front End

Next, I build robust, zero-allocation textual combinator parsers. Rather than using complex state machines or generative tools like Bison, smaller functions compose to match source characters dynamically.

> -   **Combinator Parser:** A higher-order function that accepts simple parsers as input and composes them into more complex parsers, enabling declarative grammar definitions directly in C++.


## Phase 3: Reader

I implement a Reader that translates human text directly into the raw `Datum` tree. It asks no semantic questions and treats all input merely as nested shapes.

> -   **Homoiconic:** A property of languages like Lisp where the primary representation of programs is also a data structure in a primitive type of the language itself ("Code is Data").


## Phase 4: Elaboration

With the raw shape of the code established, the Elaborator parses that generic structure. It matches patterns and validates language constraints to yield an Elaborated Core tree.

> -   **Elaborator:** The compiler pass responsible for resolving generic syntax trees (`Datum`) into concrete, semantically verified internal representations (`Core` AST).


## Phase 5: Continuation-Passing Style (CPS)

The pipeline then drops the AST into a drastically different representation. Standard recursive evaluation overflows the rigid call stack depths permitted by compile-time limits. Defunctionalization and continuations flatten the control flow entirely.

> -   **Continuation-Passing Style (CPS):** A style of programming in which control is passed explicitly. Operations do not return; instead, they call an explicit continuation function with their result.


## Phase 6: Closures

With the AST dropped into a linear flow, I materialize functions and their variable environments to execute the Lisp script correctly through a direct compiler backend.

> -   **Closure:** A function object that has a preserved referencing environment, capturing and carrying the scoped variables active when it was declared.


## Phase 7: Senders

Beyond simple synchronous execution, I port the execution engine to rely on the modern C++ asynchronous model, compiling the CPS program flow into parallel, composable sender nodes.

> -   **Senders and Receivers:** An execution model (often associated with `std::execution` or Beman Execution) that defines asynchronous operations as chains of senders passing values or errors to receivers.


## Phase 8: Graphs

Finally, using the structured data generated by the backend, I extract visual flow graphs of the compile-time execution pipeline to prove what the compiler has actually accomplished under the hood.


# For the Hell of It

This blog series will document each of these phases, detailing the workarounds, the breakthroughs, and the C++26 features that make it possible. I am mapping the frontiers of C++ language constraints. I am climbing this mountain simply because it is there.


# References

-   Hillary, E. [Edmund Hillary Quotes](https://www.goodreads.com/author/quotes/183217.Edmund_Hillary). Goodreads.
