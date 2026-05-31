<div class="abstract" id="org15a5df0">
<p>
To truly compile our Scheme dialect into blazing-fast native code, we need an execution model.
Phase 7 bridges the gap between functional Scheme evaluation and modern C++ asynchronous workloads.
We achieve this by lowering our CPS Abstract Syntax Tree into deeply nested <b>Typed Senders</b>.
</p>

</div>


# The P2300 Sender/Receiver Model

C++26 brings standard asynchronous execution models (`std::execution`). The fundamental elements are:

-   **Senders**: Types describing units of work that will produce values (or errors).
-   **Receivers**: Callbacks that consume those values.
-   **Schedulers**: Contexts dictating *where* the work happens (e.g., thread pools).

Because our Scheme compiler relies on Continuation-Passing Style—where every operation simply says "do work, then pass the result to this callback"—it matches perfectly with the Sender/Receiver architecture. A Scheme continuation is functionally isomorphic to a C++ Receiver.


# Generating Typed Senders

When the Elaborator has finished building the structural AST, the backend systematically translates it into native C++ sender algorithms.

For instance, consider the fundamental operations:

-   A Scheme atom or literal `(quote 5)` translates directly into `std::execution::just(5)`, representing a sender that instantly completes with the value 5.
-   Complex parallel argument evaluations like evaluating the arguments to a function `(+ (f 1) (g 2))` translate into a `std::execution::when_all(eval_f1, eval_g2)`, spawning an async pipeline that awaits both results.
-   Chaining evaluations, like applying the `+` operation after the arguments are evaluated, translate into `std::execution::then(..., apply_plus)`.

```c++
// An idealized view of the generated sender type
auto scheme_execution_pipeline =
    std::execution::then(
        std::execution::when_all(
            std::execution::just(1),
            std::execution::just(2)
        ),
        [](int a, int b) { return a + b; }
    );
```


# Zero Abstraction Overhead

What is genuinely remarkable about this stage is the compilation efficiency. These typed senders are completely resolved at compile-time. We are passing deeply nested templates into the C++ compiler.

Where typical dynamic language interpreters rely on heavy switch statements and virtual dispatch (creating cache misses and blocking optimization), our Scheme program compiles into a single, fully-known type topology. The standard C++ optimizer natively unrolls and inlines the `just`, `when_all`, and `then` structures. A computationally complex Scheme algorithm collapses down into tight, bare-metal C++ instructions running securely across thread pools.


# Conclusion

By lowering Scheme code into strongly-typed `std::execution` Senders, our compiler achieves what few others can: seamless integration with C++ asynchronous networking and task-parallel thread pool designs, executing Scheme functionally but with the footprint of hand-written C++ concurrency primitives.


# References

-   Niebler, E. et al. (2020). "P2300: std::execution". C++ Standards Committee Papers.
-   Beman Project \`std::execution\` library execution contexts.
