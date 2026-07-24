<div class="abstract" id="orgfb86def">
<p>
The summit is reached. A Scheme-light compiler parses, elaborates,
tree-transforms, and evaluates entirely at C++26 compile time. The
sender-based backend makes structural parallelism visible. What did I learn?
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 12 - CPS ←](phase-12-cps.md)

</nav>


# The View from the Summit

Every phase of this project was a design problem dressed as a language limit. No persistent heap. No virtual dispatch. No allocator in `constexpr`. Each constraint forced a cleaner abstraction, and the accumulation of those abstractions turned out to be a functional compiler running inside the C++ constant evaluator.

This final post reviews each phase and draws out the lessons for what comes next.


# Architecture in Review


## Phase 1 &#x2014; Foundation

I establish the vocabulary: `result<T>` for error propagation without exceptions, `static_vector` for fixed-capacity sequences backed by `std::array`, `Box<A>` for owning heap indirection in `constexpr` contexts, and `Fix<F>` for the open-recursive fixpoint combinator. These four types carry the entire pipeline.

`static_vector` is the easy one &#x2014; a `std::array` with a size counter, no allocator, fully constexpr-friendly by design. `Box<A>` is the interesting case: raw `new` / `delete` inside a `constexpr` destructor, valid since C++20 as long as every allocation is freed before the compiler observes the result. `Fix<F>` turns an open-recursive functor into a proper recursive type by tying the knot with a heap pointer.


## Phase 2 &#x2014; Front End

The parser combinators use immutable cursors: a string view plus an offset. Combining two parsers advances the cursor; failure propagates through `result`. No allocation happens at parse time &#x2014; the combinator structure is static.

The applicative pattern (McBride, Conor and Paterson, Ross, 2008) drives this. A parser is a `Cursor → result<T, Cursor>` function. `map` lifts pure functions over the result type. `ap` threads the cursor through a sequence. `many` folds a parser until it fails.


## Phase 3 &#x2014; Reader

The reader is the first phase that produces data structures. It reads S-expressions into a `Datum` tree: atoms, symbols, integers, booleans, and nested lists. The tree lives in an arena &#x2014; a flat slab of pre-allocated nodes addressed by handle rather than pointer. The arena is the workaround for the constraint that persistent heap allocations cannot cross out of a `constexpr` evaluation: arena nodes are stack-allocated inside the evaluation and freed when the evaluation ends.

The reader asks no semantic questions. It treats `(if x y z)` and `(foo bar)` identically &#x2014; four-element lists. Semantic recognition is the next phase's job.


## Phase 4 &#x2014; Elaboration

The elaborator traverses the `Datum` tree and classifies nodes into a typed core AST. `if` becomes `core::if_expr`. `lambda` becomes `core::lambda`. Variable references become `core::var`. Applications become `core::apply`. Quote, `define`, `let`, and `let*` are recognized by head symbol.

`let` desugars immediately: `(let ((x e)) body)` becomes `((lambda (x) body) e)`. `let*` desugars to nested single-binding lets, each with its own lambda scope. No new IR nodes are required &#x2014; the interpreter never sees `let` at all.


## Phase 5 &#x2014; Fixpoint Trees

The elaborated core AST is arena-based and handle-indexed. To evaluate it, I translate it into `Fix<CompF>` &#x2014; a heap-pointer computation tree using the open-recursive fixpoint combinator (Meijer, Erik and Fokkinga, Maarten and Paterson, Ross, 1991).

`CompF` is the open-recursive functor:

```c++
// src/smd/smdscheme/sender/comp_tree.hpp
template <int MaxList>
struct comp_f_factory {
    template <typename A>
    using type = std::variant<
        comp_pure,
        comp_lookup,
        comp_if<A>,
        comp_lambda<A, MaxList>,
        comp_apply<A, MaxList>>;
};
```

`Fix<CompF>` ties the recursion knot: each node's children are `Box<Fix<CompF>>` values. The translation from core AST to `Fix<CompF>` is the `core_to_comp` pass. From this point, the arena is no longer needed.


## Phase 6 &#x2014; Closures and Values

The value domain is `closure::value<Core>`: an `int`, a `bool`, a symbol, a closure, a builtin, or a foreign function. A closure holds a non-owning pointer to its lambda node in the core tree and a captured copy of the environment (a `static_vector` of name/value bindings, held in a `constexpr_box`).

The environment is copied on `lambda` entry &#x2014; functional, not imperative. The builtin arithmetic operators (just `+` and `*`) are `builtin` values holding an operation enum (`builtin_op`); a separate `foreign_function` alternative holds a native function pointer for the FFI callbacks of Phase 11.


## Phase 7 &#x2014; Mendler Interpretation

`mendler_run` is a genuine Mendler-style paramorphism over `Fix<CompF>`. The Mendler scheme (Mendler, Nax Paul, 1987) gives the algebra explicit control over recursion by threading the recursive call as an argument. The project provides two generic combinators (`mendler_fold` and `mendler_para`) in `src/smd/fixpoint/recursion_schemes.hpp`; the interpreter uses `mendler_para` because closures need the original `Fix` node:

```c++
// src/smd/smdscheme/sender/fixpoint_eval.hpp
auto mendler_run(Comp<MaxList> const& comp, Env const& env) -> Res {
    auto const algebra = [](auto const& recurse, Env const& env,
                            CompT const& node, CompF const& layer) -> Res {
        return std::visit(overloaded{
            [&](comp_if<CompT> const& if_) -> Res {
                auto cond = recurse(*if_.cond, env);
                ...
            },
            [&](comp_lambda<CompT, MaxList> const&) -> Res {
                return Val{closure{&node, ...}};  // uses node from para
            },
            ...
        }, layer);
    };
    return mendler_para<Res, CompF>(algebra, env, comp);
}
```

A standard catamorphism would first fold all children, then pass the results to the algebra &#x2014; too uniform for language interpretation, where `if` must evaluate only one branch and `lambda` must not evaluate its body at creation time. The algebra receives `recurse` as a parameter and calls it selectively, with whatever context it chooses.


## Phase 8 &#x2014; Sender-Based Evaluation

`sender_mendler_run` has the same structure as `mendler_run` but wraps each sub-evaluation as a deferred sender. For binary builtin application, the two argument senders are combined with `when_all`:

```c++
// src/smd/smdscheme/sender/sender_mendler_eval.hpp
auto eval_arg0 = sender_v::then(sender_v::just(0), [&](int) {
    return sender_mendler_run(*args[0], env);
});
auto eval_arg1 = sender_v::then(sender_v::just(0), [&](int) {
    return sender_mendler_run(*args[1], env);
});
return sender_v::then(sender_v::when_all(eval_arg0, eval_arg1), apply_builtin);
```

With `sync_wait` as the scheduler, execution is inline-sequential. A thread-pool scheduler would parallelize argument evaluation without changing the evaluation code. The sender graph makes structural independence visible to the framework (Dominiak, Michał and others, 2024).


## Phase 9 &#x2014; Constexpr Pipeline

`constexpr_eval` chains the full pipeline &#x2014; reader, elaborator, `core_to_comp`, `mendler_run` &#x2014; and `static_assert` proves it works:

```c++
// src/smd/smdscheme/sender/fixpoint_program.test.cpp
static_assert(std::get<int>(constexpr_eval("(+ 1 (* 2 3))").value()) == 7);
static_assert(
    std::get<int>(constexpr_eval("((lambda (x) (+ x 1)) 41)").value()) == 42);
static_assert(std::get<int>(constexpr_eval("(let* ((x 1) (y (+ x 1))) (+ x y))")
                                .value()) == 3);
```

These are compiler-enforced invariants, not unit tests. If any is wrong, the translation unit does not compile.


# Lessons Learned


## `constexpr` is Turing-complete, but allocation is the boundary

Transient allocations work &#x2014; any `Box` created during evaluation must be freed before the compiler observes the result. Persistent allocations do not. A `fixpoint_program` cannot be stored as a `constexpr` variable because its `Box` nodes would escape the transient allocation scope. The evaluation result &#x2014; a variant of scalars &#x2014; can be stored.

The consteval allocation proposal (future C++ work) would lift this restriction. Until then, the program lives only for the duration of its evaluation.


## `Fix<F>` + Mendler gives control without losing structure

A standard fold is too uniform for language interpretation. `if` must evaluate only one branch. `lambda` must not evaluate its body until applied. Mendler recursion threads the recursive call as an argument, so the algebra decides when to recurse. The tree stays structurally visible &#x2014; no continuation-passing, no CPS transformation, no trampoline. The interpreter is a single function over a sum type (Bird, Richard S. and de Moor, Oege, 1997).


## Senders compose but type erasure is the bottleneck

`when_all` works for fixed-arity arguments &#x2014; binary builtins. Runtime-arity argument lists (closures, variadic forms) cannot be handled with a variadic `when_all` without type erasure. `any_sender` or coroutines add complexity the proof-of-concept avoids. Sequential evaluation is the current answer for variable-arity cases.


## Desugaring is the right level for `let` / `let*`

No new IR nodes. No new interpreter cases. The elaborator rewrites `let` and `let*` into `lambda` + application before the interpreter ever sees them. `let*` nests single-binding lambdas so each binding scope sees earlier bindings. The interpreter stays simple; the elaborator absorbs the complexity.


# What `std::indirect` Will Fix

The natural C++26 type for owned indirection is `std::indirect` (Coe, Jonathan and others, 2024). The obstacle is its explicit default constructor. `static_vector` value-initializes its backing `std::array<T, Capacity>`, which requires a non-explicit default. `Box` provides that &#x2014; its default sets `ptr = nullptr`. When `std::indirect` gains a non-explicit default constructor, or gains full constexpr support, `Box` can be retired and the pipeline simplifies.


# Future Work

**call/cc**: First-class continuations. In a Mendler interpreter, this means reifying the "recurse" function as a value. The CPS structure is already implicit in the recursive call pattern (Steele, Guy L., 1977) (Appel, Andrew W., 1992).

**set!**: Mutable bindings require a store &#x2014; an indirection from environment names to mutable locations. A boxing transformation pass can introduce this before the interpreter runs.

**Hygienic macros (syntax-rules)**: Operate on datum trees before elaboration. The sets-of-scopes model (Flatt, Matthew, 2016) handles renaming correctly without the defects of earlier hygienic approaches.

**Numerical tower**: Exact/inexact types. Best handled as a separate constexpr-bignum library linked into the value domain.

**Standard prelude precompilation**: Parse `prelude.scm` at build time, serialize the arena, splice into user programs. The arena representation already supports this in principle.


# The Mountain Metaphor, One Last Time

The mountain metaphor has driven this series, and it holds here too. Nobody climbs Everest because the summit needs to be reached. They climb to find out what they can do at altitude.

C++26 `constexpr` is capable of a non-trivial functional compiler. The tools &#x2014; `Fix<F>`, Mendler folds, sender composition &#x2014; are general-purpose. They apply beyond Scheme. The techniques for zero-allocation parsing, arena-based ASTs, open-recursive computation trees, and Mendler-style interpretation are each useful independently of the compile-time constraint.

The proof-of-concept proved what it set out to prove: the compiler can do real evaluative work, not just type-checking. The path is marked. Where it goes next is a different climb.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md)

</nav>


# References

Steele, Guy L. (1977). **Lambda: The Ultimate GOTO**, MIT AI Memo 443.

Appel, Andrew W. (1992). **Compiling with Continuations**, Cambridge University Press.

Mendler, N. P. (1991). **Recursive Types and Type Constraints**, PhD thesis, Cornell University.

Bird, Richard S. and de Moor, Oege (1997). **The Algebra of Programming**, Prentice Hall.

Meijer, Erik, Fokkinga, Maarten, and Paterson, Ross (1991). **Functional Programming with Bananas, Lenses, Envelopes and Barbed Wire**, FPCA.

Niebler, Eric et al. (2020). **P2300: std::execution**, ISO C++ Committee Papers.

McBride, Conor and Paterson, Ross (2008). **Applicative Programming with Effects**, JFP.

Abelson, Harold and Sussman, Gerald Jay (1996). **Structure and Interpretation of Computer Programs**, 2nd ed., MIT Press.

McCarthy, John (1960). **Recursive Functions of Symbolic Expressions and Their Computation by Machine**, CACM.

Flatt, Matthew (2016). **Binding as Sets of Scopes**, POPL.

P3019R13 (2024). **std::indirect and std::polymorphic**, ISO C++ Committee.

Abel, Andreas and Pientka, Brigitte (2012). **Wellfounded Recursion with Copatterns**, ICFP.
