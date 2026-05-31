<div class="abstract" id="orgfb0d475">
<p>
By Phase 4, my compile-time compiler could parse and elaborate code, but recursive execution directly blew up the C++ compiler's internal call stack.
Phase 5 fixes this by introducing Continuation-Passing Style (CPS), fundamentally shifting how I view function execution.
</p>

</div>


# What is Continuation-Passing Style?

In typical direct-style programming, a function computes a value and ~return~s it to its caller, utilizing the hardware (or compiler) stack to track where to return.

```c++
// Direct Style
int add(int a, int b) {
    return a + b;
}
int result = add(2, add(3, 4));
```

In Continuation-Passing Style, no function ever returns. Every function explicitly accepts an extra argument—a callback—representing "what to do next." This callback is known as the **Continuation**.

```c++
// Continuation-Passing Style (conceptual)
void add_cps(int a, int b, auto continuation) {
    continuation(a + b);
}

add_cps(3, 4, [](int partial_sum) {
    add_cps(2, partial_sum, [](int final_result) {
        // ... done
    });
});
```


# Defunctionalization and Stack Control

In a `constexpr` environment, I cannot dynamically allocate these callback lambdas on the heap. I must **defunctionalize** them. Defunctionalization transforms high-level functions (like my lambdas) into concrete data structures (structs/variants) combined with an `apply` loop.

Instead of calling a function on the C++ stack, an evaluation step yields a specific continuation object: "Here is the next step to execute, and here is its data." A top-level trampolining loop processes these continuations.

```c++
while (!is_finished(current_continuation)) {
    current_continuation = step(current_continuation);
}
```


# Tail-Call Optimization (TCO)

Because Scheme mandates Tail-Call Optimization (meaning infinite recursion without stack overflow is valid loop behavior), CPS provides the perfect mechanical underpinning. Tail calls in CPS are physically just jumps. I merely pass the current continuation without extending it. The trampolining loop handles the iteration perfectly in constant memory space.


# Conclusion

Through CPS and defunctionalization, my Scheme AST is transformed from nested operations fighting for C++ call-stack dominance into an explicit state machine perfectly suited for my fixed-capacity `constexpr` arenas. I am now capable of executing complex Scheme algorithms reliably during C++ compilation without exploding standard parsing limits.


# References

-   Danvy, O. (2006). "Defunctionalization at Work." Proceedings of the 3rd ACM SIGPLAN conference on Principles and practice of declarative programming.
-   Steele, G. L. (1978). "Rabbit: A Compiler for Scheme." MIT AI Lab.
-   Reynolds, J. C. (1972). "Definitional interpreters for higher-order programming languages." Proceedings of the ACM annual conference.
