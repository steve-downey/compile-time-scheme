<div class="abstract" id="org0808e81">
<p>
By Phase 4, my compile-time compiler could parse and elaborate code into a typed <code>core_type</code> AST, but recursive execution deeply stacked the C++ compiler's internal call limits.
This phase introduces Continuation-Passing Style (CPS (Reynolds, John C., 1972)) at the C++ template layer as an alternative evaluation backend, enabling native Tail-Call Optimization (TCO) inside the C++ compiler itself.
The CPS backend materializes lambda nodes in the <code>core_type</code> arena into concrete closure values through a continuation-threaded dispatch.
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

In Continuation-Passing Style, functions don't just return. They accept an extra argument—a callback—representing "what to do next." This callback is known as the **Continuation**.

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


# Modeling the \`cps\_code\` Signature in C++26

To represent this statically at compile time without relying on dynamically allocated \`std::function\` closures, I use C++ templating. Our \`cps\_code\` wraps any callable object representing Scheme code.

```cpp
template <class F>
struct cps_code {
    F f;

    /// Evaluates the CPS expression in @p env, invoking continuation @p k.
    template <class Env, class K>
    constexpr auto operator()(Env const &env, K k) const {
        return f(env, k);
    }
};
```

Each CPS form takes an environment (\`env\`) containing variables and a continuation function (\`k\`). When execution finishes, it calls \`k(result)\` instead of returning it directly (or passes it out naturally back through the call).


# Dispatching Core Nodes in CPS

Instead of rewriting an entire interpreter loop within C++ structures natively (which would require a manual trampoline or defunctionalization (Danvy, Olivier and Nielsen, Lasse R., 2001) to avoid deep stack frames), I leverage the C++ compiler's own native optimizer. Over the `core_type` AST from Phase 4—with its `arena_box` handles, `core_if`, `core_lambda`, and `core_application` nodes—I built `cps_dispatch`. The result type of every dispatch is `foundation::result<closure::value<Core>>`: the `result<T>` error-handling type from Phase 1, parameterized on the closure value type defined in Phase 6.

```cpp
template <int MaxNodes, int MaxList, class Cont, class Env, class K>
constexpr auto cps_dispatch(
    elaborator::core_type<MaxNodes, MaxList> const &node,
    const foundation::tree_arena<elaborator::core_type<MaxNodes, MaxList>,
                                 MaxNodes> &arena,
    Cont const &cont, Env const &env, K const &k)
    -> foundation::result<
        closure::value<elaborator::core_type<MaxNodes, MaxList>>> {
    using Core = elaborator::core_type<MaxNodes, MaxList>;

    using Val = closure::value<Core>;
    using Res = foundation::result<Val>;

    return std::visit(
        smd::fixpoint::overloaded{
            [&](elaborator::core_integer const &ci) -> Res {
                // ceecf448-bff3-4b76-99cc-8c11ac4140f3
                auto r = cont(Val{ci.value});
                if (!r.has_value())
                    return r;
                return k(r.value());
```

Notice how \`cps\_dispatch\` chains continuations via \`Cont\` and \`K\` template parameters. Eager sub-evaluation (such as determining node conditions or processing function arguments before an application) passes \`identity\_k\` continuations which simply return the result directly to the C++ call stack.

However, for capturing a lambda scope, the \`cps\_dispatch\` returns control directly through the continuation parameter \`cont\`:

```cpp
Val v{closure::closure<Core>{
    &node, closure::constexpr_box<closure::env<Core, 16>>{
               new closure::env<Core, 16>{env}}}};
auto r = cont(v);
if (!r.has_value())
    return r;
return k(r.value());
```


# Relying on the C++ Compiler for TCO

Because Scheme mandates Tail-Call Optimization (meaning infinite recursion without stack overflow is valid loop behavior), CPS architectures like the Rabbit compiler (Steele, Guy L., 1978) historically represented continuations as heap-allocated closures (a function pointer paired with a captured environment) to avoid consuming the C stack.

However, since our `constexpr` paths are strictly evaluated at compile-time by C++ parsers like GCC, I delegate the job of transforming tail-recursive calls back to the native C++ optimizer. Instead of manually building a trampolining loop to evaluate defunctionalized continuations, my \`cps\_dispatch\` aligns its final AST execution steps (like executing the body of the lambda) natively in the C++ return position.

```cpp
return cps_dispatch<MaxNodes, MaxList>(
        arena.get(lam.body), arena, cont, new_env, k);
```

By forwarding \`cont\` and \`k\` into the recursive instantiation of \`cps\_dispatch(arena.get(lam.body)&hellip;)\` inside the \`return\` statement, GCC's constexpr evaluator can collapse the compile-time execution frame. A recursive Scheme call executing this branch avoids consuming extra evaluation stack depth, enabling deep Scheme tail recursion within the C++ constant evaluator.


# Conclusion

By combining Continuation-Passing Style templates with native C++ Tail Call-Optimized return forms, this alternative backend maintains state securely through execution paths without resorting to complex heap-allocated C++ data structures or manual interpreter loops.

The closure value type that `cps_dispatch` produces is the same `closure::value<Core>` domain defined in Phase 6 — the CPS backend is an alternative evaluator over the same value representation that the Mendler interpreter uses.


# References

Danvy, Olivier and Nielsen, Lasse R. (2001). *Defunctionalization at Work*.

Reynolds, John C. (1972). *Definitional interpreters for higher-order programming languages*.

Steele, Guy L. (1978). *Rabbit: A Compiler for Scheme*, MIT AI Lab.
