<div class="abstract" id="orge6f3fba">
<p>
We are building a Scheme-light compiler that parses, elaborates, and executes Lisp code entirely during C++26 compilation.
But why go to such extreme lengths to force a dynamic language into a rigid <code>constexpr</code> context?
Because we want to see if the mountain can be climbed.
</p>

</div>


# The Motivation

> "Nobody climbs mountains for scientific reasons. Science is used to raise money for the expeditions, but you really climb for the hell of it." — Edmund Hillary

This project is a proof of concept targeting C++26 on GCC16. It implements a fully staged compiler pipeline entirely inside the C++ compile-time evaluation engine. We are transforming a source text string into a syntax tree, elaborating it into an abstract semantic core, converting it to Continuation-Passing Style (CPS), and executing it using closures and sender/receiver backends. And it does all of this strictly before the compiler ever emits a single byte of executable machine code.

Why? We are not building this because the software industry desperately needs a compile-time Scheme embedded in C++. We are building this to expand our own understanding of modern C++ static analysis and template meta-programming.


# Exploring `constexpr` Boundaries

Modern C++ has steadily expanded what can be accomplished at compile time. What started as simple constant variable expressions in C++11 has grown into a nearly Turing-complete sub-language. By C++26, the `constexpr` environment is capable of remarkably non-trivial computation.

We are testing the absolute limits of this environment. Building a functional Scheme compiler forces us to confront and solve the severe restrictions of compile-time C++. It bars us from utilizing dynamic memory allocations that outlive a single constant evaluation, forcing us to eschew standard heap pointers. It lacks support for traditional virtual polymorphism, demanding alternative strategies for dynamic dispatch. It limits recursion depths and prohibits casting tricks.

To survive this, we must model zero-allocation combinator parsers using immutable cursors. We are forced to represent complex recursive Abstract Syntax Trees using flat memory arenas and handle-based indirection. We have to orchestrate open recursion and fixed-point combinators just to represent heterogeneous lists.


# The Staged Pipeline

To tackle this complexity, the architecture is broken down into distinct, heavily isolated phases. First, we establish foundational types and build robust textual combinator parsers. Next, we implement a homoiconic Reader that translates human text directly into the raw Datum tree, asking no semantic questions. Then, the Elaborator parses that generic structure, matching patterns and validating constraints to yield an Elaborated Core tree. Finally, the pipeline drops the AST into Continuation-Passing Style (CPS). This allows us to implement execution models like closures and asynchronous Beman Execution senders without overflowing the compiler's call stack.


# For the Hell of It

This blog series will document each of these phases, detailing the workarounds, the breakthroughs, and the C++26 features that make it possible. We are mapping the frontiers of C++ language constraints. We are climbing this mountain simply because it is there.


# References

-   Hillary, E. [Edmund Hillary Quotes](https://www.goodreads.com/author/quotes/183217.Edmund_Hillary). Goodreads.
