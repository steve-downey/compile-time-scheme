<div class="abstract" id="org7d8a66f">
<p>
Phase 6 marks the transition from an interpreter to an actual compiler.
The crucial functional mechanism that bridges runtime behavior into the C++ type system is the materialization of the Closure.
</p>

</div>


# What is a Closure?

In functional programming, when a lambda is parsed inside a function, it doesn't just enclose variables functionally; it fundamentally captures the environment state surrounding it at definition time (Sussman, Gerald Jay and Steele, Guy L., 1975). A **Closure** is therefore a coupling of code (the AST node/behavior) and data (the captured lexical scope variables).

```scheme
(define (make-adder x)
  (lambda (y) (+ x y)))
```

Here, the `lambda` must capture `x` to form a closure before it can be returned and invoked later.


# Materializing Closures in Native C++

Many dynamic language environments construct heavy, opaque callable types with runtime type-erasure (e.g., `std::move_only_function` or polymorphic classes) to represent closures. Those create abstraction penalties, block optimizations like inlining, and are often strictly forbidden or painfully annoying to utilize effectively in a `constexpr` context due to dynamic allocation constraints.

Because I operate structurally at compile time and already execute in CPS format, I leverage the C++ type system to define exact representations. The \`closure\` structure models this explicitly:

```cpp
template <typename Core>
struct closure {
    Core const
        *node; ///< Non-owning pointer to the lambda node in the core arena.
    constexpr_box<env<Core, 16>> captured; ///< Captured lexical environment.

    friend constexpr auto operator==(closure<Core> const &lhs,
                                     closure<Core> const &rhs) -> bool {
        // Simple structural equality for test purposes.
        return lhs.node == rhs.node;
    }
};
```

Notice how \`closure\` strictly bundles a non-owning pointer to the lambda AST node in the arena, alongside a \`constexpr\_box\` copy of its enclosing environment. This fully isolates its runtime state statically.

We map this natively into the core evaluated \`value\` type alongside our built-ins, standard symbols, and literals:

```cpp
template <typename Core>
using value = std::variant<int, bool, builtin, closure<Core>, symbol,
                           foreign_function<Core>>;
```

This effectively converts dynamic Functional AST bindings into exact, blazing-fast primitive C++ aggregate data types. The Scheme runtime behavior collapses entirely into the C++ type system without relying on dynamically typed heaps.


# Negative Compile Tests

Because closures enforce rigorous static paths, I enhance the C++ compiler's reporting. If my environment encounters an unbound variable error, or if closure evaluation fails due to arity mismatches, it yields an explicit \`false\` return at compile time.

I write negative compile-time tests using standard `static_assert` lambdas, enabling the C++ compiler to act natively as a lint and error-reporting tool for my Scheme source text. An expression containing an unbound variable, for instance, is proven mechanically to fail execution directly at compilation:

```cpp
static_assert([] {
    auto r = closure::compile_to_closure("(+ x 1)"sv);
    if (!r.has_value())
        return true; // elaboration or eval error
    auto env0 =
        smd::smdscheme::closure::default_env<elaborator::core_type<32, 16>,
                                             16>();
    auto vr = r.value()(env0);
    return !vr.has_value(); // unbound variable error at runtime
}());
```

Here, compilation successfully occurs **only if** the evaluation of the badly formed Scheme resolves properly into the internal Unbound Variable error state (causing \`!vr.has\_value()\` to return true, which satisfies the C++ compilation check).


# Conclusion

By emitting statically defined closure types representing my CPS states, my project ceases to be merely a toy compile-time interpreter. It functions directly as a compiler, bootstrapping Scheme representations into equivalent, fully optimized native C++ behavior.


# References

Sussman, Gerald Jay and Steele, Guy L. (1975). *Scheme: An interpreter for extended lambda calculus*, MIT AI.
