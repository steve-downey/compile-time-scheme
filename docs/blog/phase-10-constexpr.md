<div class="abstract" id="orgeea2aa4">
<p>
The full pipeline evaluates at compile time. A <code>static_assert</code>
proves that parsing, elaborating, tree-converting, and interpreting a Scheme
expression produces the correct value &#x2014; all before the compiler emits a
single byte of machine code.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 9 - Visualizing Execution ←](phase-9-graphs.md)

</nav>


# Constexpr Pipeline

Everything I have built so far &#x2014; the reader, the elaborator, the `Fix<CompF>` tree, the Mendler interpreter &#x2014; was written with `constexpr` in mind. This phase is where that pays off. I write a `constexpr` wrapper around the whole pipeline and use `static_assert` to prove it works at compile time.


## The One-Liner Proof

The test file `src/smd/smdscheme/sender/fixpoint_program.test.cpp` contains this:

```cpp
constexpr auto constexpr_eval(std::string_view src) -> Res {
    auto program_r = fixpoint_eval::compile_fixpoint<32, 16>(src);
    if (!program_r.has_value())
        return Res{program_r.error()};
    auto env = closure::default_env<CompT, 16>();
    return program_r.value()(env);
}
```

```cpp
static_assert(std::get<int>(constexpr_eval("42").value()) == 42);
static_assert(std::get<bool>(constexpr_eval("#t").value()) == true);
static_assert(std::get<int>(constexpr_eval("(+ 1 2)").value()) == 3);
static_assert(std::get<int>(constexpr_eval("(+ 1 (* 2 3))").value()) == 7);
static_assert(std::get<int>(constexpr_eval("(if #t 1 2)").value()) == 1);
static_assert(std::get<int>(constexpr_eval("(if #f 1 2)").value()) == 2);
static_assert(
    std::get<int>(constexpr_eval("((lambda (x) (+ x 1)) 41)").value()) == 42);
static_assert(std::get<int>(constexpr_eval("(let ((x 1)) x)").value()) == 1);
static_assert(
    std::get<int>(constexpr_eval("(let ((x 1) (y 2)) (+ x y))").value()) == 3);
static_assert(std::get<int>(constexpr_eval("(let* ((x 1) (y (+ x 1))) (+ x y))")
                                .value()) == 3);
```

These are not unit tests. They are compiler-enforced invariants. If any of them is wrong, the translation unit does not compile. The `constexpr_eval` function is identical to the runtime `eval` function in the same file &#x2014; the only difference is the `constexpr` keyword on the declaration.

`compile_fixpoint` chains the full pipeline: read a datum, elaborate it to a core AST, convert the core AST to a `Fix<CompF>` computation tree, and return a `fixpoint_program` that can be called with an environment. All of that runs inside the compiler.


## What Makes It Work

Every type in the pipeline is constexpr-capable, but for different reasons.

`static_vector` is the easy case. It stores elements in a `std::array` &#x2014; inline stack storage, no heap, no allocator:

```c++
// src/smd/smdscheme/foundation/static_vector.hpp
template <class T, int Capacity>
class static_vector {
  private:
    std::array<T, Capacity> storage_{};
    int size_{};
};
```

There is nothing to object to here. `std::array` is constexpr-friendly by design, and a fixed-capacity array with a size counter has no moving parts that the compiler cannot reason about statically.

`result<T>` wraps a `std::variant<T, parse_error>`. Variants are constexpr-capable since C++17. Again, no issue.

`Box<A>` is the interesting case.


## Box as Constexpr Workaround

`Box<A>` is a nullable owning pointer built on raw `new` and `delete`:

```c++
// src/smd/fixpoint/box.hpp
template <typename A>
struct Box {
    A *ptr = nullptr;

    constexpr ~Box() { delete ptr; }

    constexpr Box(Box const &other)
        : ptr(other.ptr ? new A(*other.ptr) : nullptr) {}
    // ...
};
```

Since C++20, `new` and `delete` are permitted in `constexpr` functions, subject to one constraint: all allocations made during a constant evaluation must be freed before that evaluation ends. The compiler tracks every allocation and rejects any evaluation that leaks heap memory into a compile-time constant.

In the Mendler interpreter, `Box` pointers exist inside the computation tree and inside closures. Those closures are created during evaluation and freed when the result is produced. The final value returned from `constexpr_eval` is a `result<closure::value<CompT>>`. For these example programs the payload is always an `int`, a `bool`, or a `closure::symbol` &#x2014; none of which contain heap pointers. (The `value` variant can also hold a `closure`, which does own a `Box`; but each example here reduces to a scalar or symbol.) Every `Box` allocated during evaluation is freed before the `static_assert` observes the result. The transient allocation rule is satisfied.

The natural C++26 type for owned indirection is `std::indirect` (Coe, Jonathan and others, 2024). It has the right value semantics and is designed for exactly this use case. The obstacle is its explicit default constructor: `static_vector` initializes its backing `std::array<T, Capacity>` with value initialization, which requires a non-explicit default constructor. `Box` provides that &#x2014; its default leaves `ptr = nullptr`, which is the right representation for an unused slot. When `std::indirect` gains a non-explicit default constructor, or when consteval allocation lands and eliminates the need for the nullable-but-empty representation, `Box` can be retired.


## static\_vector for Arguments

Function call arguments in `comp_apply` are stored in a `static_vector<Box<CompT>, MaxList>`:

```c++
// src/smd/smdscheme/sender/comp_tree.hpp
template <typename A, int MaxList>
struct comp_apply {
    Box<A> func;
    foundation::static_vector<Box<A>, MaxList> args;
};
```

`std::vector` would also be constexpr-capable since C++20, but its allocator machinery is more complex and its size is dynamic in a way the compiler has to track more carefully. `static_vector` makes the capacity bound explicit in the type &#x2014; `MaxList` is a template parameter &#x2014; which keeps the tree type entirely self-describing and avoids any allocator-related complications in constexpr contexts.


## What Cannot Cross the Boundary

The transient allocation rule explains a subtlety: I cannot store a `fixpoint_program` as a `constexpr` variable.

```c++
// This does NOT compile
constexpr auto prog = fixpoint_eval::compile_fixpoint<32, 16>("(+ 1 2)");
```

`fixpoint_program` contains a `Comp<MaxList>` by value, and `Comp<MaxList>` is `Fix<CompF>` which contains `Box` nodes. Those `Box` nodes hold heap pointers. If `prog` were a `constexpr` variable, those pointers would persist past the end of the constant evaluation &#x2014; violating the transient allocation rule.

What I can do is evaluate the program and store the result. The result is a `std::variant` of scalars and symbols. No heap pointers escape:

```c++
// This DOES compile
static_assert(std::get<int>(constexpr_eval("(+ 1 2)").value()) == 3);
```

The consteval allocation proposal (future C++ work) would lift this restriction by allowing non-transient heap allocations to exist in compile-time constants. Until then, the program lives only for the duration of the evaluation.


## The Sender Interpreter Is Not Constexpr

The sender-based interpreter from the previous phase is not constexpr. `sync_wait` involves OS-level synchronization primitives &#x2014; mutexes, condition variables &#x2014; that have no meaning at compile time. The constexpr pipeline uses `mendler_run` directly, not `sender_mendler_run`.

This is not a deficiency. The two evaluators serve different purposes. `mendler_run` is the pure functional interpreter that the compiler can reason about completely. `sender_mendler_run` is the runtime evaluator that exposes structural parallelism to a scheduler. They share the same `Fix<CompF>` tree representation; only the traversal strategy differs.


## The Payoff

The `static_assert` battery proves the compiler is doing real work. Not just type-checking &#x2014; **evaluating**. It parses the string `"((lambda (x) (+ x 1)) 41)"`, recognizes the lambda application, builds the environment, applies the argument `41` to the parameter `x`, evaluates the body `(+ x 1)` in the extended environment, and confirms the result is `42`. All of this before `main` has a chance to run.

The point of targeting C++26 is to make the evaluator itself a compile-time computation.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [Next: Phase 11 - Real World Integration →](phase-11-real-world.md)

</nav>


# References

P3019R13 (2024). **std::indirect and std::polymorphic**, ISO C++ Committee.
