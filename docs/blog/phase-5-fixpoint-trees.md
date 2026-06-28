<div class="abstract" id="orga008ace">
<p>
I represent the computation tree as <code>Fix&lt;CompF&gt;</code> — a type-level fixed-point
combinator that ties the recursive knot. Each node is a variant layer
parameterized by its children, enabling generic folds and transformations
over the tree structure.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 4 - Elaboration ←](phase-4-elaboration.md)

</nav>


# From Arena to Fixed Point

The core AST from the elaborator is arena-based: every child node is an integer handle into a `tree_arena`, and you need the arena to dereference those handles. That is fine for parsing and elaboration, where everything is `constexpr` and the arena lives on the stack. But the interpreter needs to walk trees freely, thread environments through recursive calls, and return closures that capture sub-trees. Carrying the arena everywhere would couple the interpreter to the parser's memory model.

So I introduce a second intermediate representation: `Fix<CompF>`. This is a self-contained tree that uses `Box` (an owning pointer) for children instead of arena handles. Once built, it needs no external context for traversal. The `core_to_comp` function converts from the first representation to the second.


# The Type-Level Fixed-Point Combinator

Before I can define a recursive tree type, I need a way to tie the recursive knot at the type level. The technique is the fixed-point combinator `Fix<F>`:

```cpp
template <template <typename> class F>
struct Fix {
    F<Fix<F>> inner;
};
```

This satisfies the isomorphism `Fix<F> ≅ F<Fix<F>>`. A value of type `Fix<F>` holds one layer of `F`, where the recursive positions are themselves `Fix<F>`. There is no infinite type: the template instantiation terminates because `Fix<F>` is defined before it is used inside `F<Fix<F>>`.

Two zero-cost boundary operations complete the interface:

```cpp
/** Wrap one layer of @p F into the fixed-point type. */
template <template <typename> class F>
constexpr auto wrap_fix(F<Fix<F>> layer) -> Fix<F> {
    return Fix<F>{std::move(layer)};
}

/** Unwrap one layer from a fixed-point value, exposing F<Fix<F>>. */
template <template <typename> class F>
constexpr auto unwrap_fix(const Fix<F> &fixed) -> const F<Fix<F>> & {
    return fixed.inner;
}
```

`wrap_fix` injects one layer into the fixed point. `unwrap_fix` exposes the layer for pattern matching. They are the iso-recursive isomorphism boundaries (Bird, Richard S. and de Moor, Oege, 1997).

Inside `F`, recursive children are stored as `Box<Fix<F>>` rather than `Fix<F>` by value. A plain `Fix<F>` in a `std::variant` member would require knowing its own size before it is defined — the classic recursive type problem. `Box<A>` is a constexpr-capable owning pointer:

```c++
// src/smd/fixpoint/box.hpp
template <typename A>
struct Box {
    A *ptr = nullptr;
    // copy, move, destructor all constexpr
    constexpr auto operator*() const -> A & { return *ptr; }
};

template <typename A, typename... Args>
constexpr auto make_box(Args &&...args) -> Box<A> {
    return Box<A>(new A(std::forward<Args>(args)...));
}
```

`Box` uses `new` / `delete`, which are `constexpr` in C++20 for transient allocations. The natural C++26 type for this role is `std::indirect` (Coe, Jonathan and others, 2024), but its explicit default constructor blocks aggregate initialization inside `static_vector`, and full `constexpr` support is not yet available in GCC 16. `Box` avoids both constraints.


# The CompF Functor

The `comp_f_factory` template defines the open-recursive variant that becomes `Fix<CompF>` once the knot is tied. Each alternative in the variant is either a leaf (no children, does not use the type parameter `A`) or an internal node (children stored as `Box<A>`):

```cpp
/// A computation node holding a literal atom (integer, boolean, or symbol).
/// Stores atoms only — no full values or closures — to avoid circular type
/// dependencies with closure::value<Comp>.
struct comp_pure {
    std::variant<int, bool, std::string_view> atom;
};

/// A computation node that looks up a variable name in the environment.
struct comp_lookup {
    std::string_view name;
};

/// A computation node for conditional branching: condition, consequent,
/// alternative, each as a recursive child computation.
///
/// @tparam A The type of recursive children (will be instantiated as Comp).
template <typename A>
struct comp_if {
    smd::fixpoint::Box<A> cond;
    smd::fixpoint::Box<A> cons;
    smd::fixpoint::Box<A> alt;
};

/// A computation node for lambda (function) abstraction.
///
/// @tparam A        The type of recursive children.
/// @tparam MaxList  Maximum number of formal parameters.
template <typename A, int MaxList>
struct comp_lambda {
    foundation::static_vector<std::string_view, MaxList> params;
    smd::fixpoint::Box<A> body;
};

/// A computation node for function application.
///
/// @tparam A        The type of recursive children.
/// @tparam MaxList  Maximum number of arguments.
template <typename A, int MaxList>
struct comp_apply {
    smd::fixpoint::Box<A> func;
    foundation::static_vector<smd::fixpoint::Box<A>, MaxList> args;
};
```

```cpp
/// Factory template producing the open-recursive variant layer for CompF.
/// Each structural node (comp_if, comp_lambda, comp_apply) is parameterized
/// by A, the type of recursive children. Leaf nodes (comp_pure, comp_lookup)
/// have no children and do not use A.
///
/// @tparam MaxList  Maximum list/argument length.
template <int MaxList>
struct comp_f_factory {
    /// One variant layer; @p A is the recursive self-reference.
    template <typename A>
    using type = std::variant<comp_pure, comp_lookup, comp_if<A>,
                              comp_lambda<A, MaxList>, comp_apply<A, MaxList>>;
};

/// The concrete recursive computation tree type.
/// Fix<F> ties the knot: Comp = F<Comp> where F = comp_f_factory<MaxList>.
/// Children are stored as Box<Comp> (constexpr owning pointer) rather than
/// arena handles.
///
/// @tparam MaxList  Maximum list/argument length.
template <int MaxList>
using Comp = smd::fixpoint::Fix<comp_f_factory<MaxList>::template type>;
```

`comp_pure` holds only atoms — integers, booleans, and `string_view`. It does not hold full `closure::value` objects. The reason is circular type dependency: `closure::value` is parameterized by `Comp`, so if `comp_pure` held a `closure::value`, the definition of `Comp` would circularly depend on `closure::value` which circularly depends on `Comp`. Storing only atoms breaks the cycle.

`comp_lookup` stores a `string_view` into the source string, so variable references require no allocation.

The `Comp` type alias ties the knot:

```c++
// src/smd/smdscheme/sender/comp_tree.hpp
template <int MaxList>
using Comp = smd::fixpoint::Fix<comp_f_factory<MaxList>::template type>;
```

`Comp<MaxList>` is the self-contained computation tree. Its nodes are variants; its children are `Box<Comp<MaxList>>`.


# fmap\_comp: The Functor Map

For `Fix<CompF>` to support generic folds and transformations, `CompF` must be a functor — it must define a `fmap` operation that transforms children while leaving structure intact (Meijer, Erik and Fokkinga, Maarten and Paterson, Ross, 1991):

```cpp
template <int MaxList, typename F, typename A>
constexpr auto
fmap_comp(F &&f,
          typename comp_f_factory<MaxList>::template type<A> const &layer) {
    using B = std::invoke_result_t<F, A const &>;
    using ResultF = typename comp_f_factory<MaxList>::template type<B>;

    return std::visit(
        smd::fixpoint::overloaded{
            [](comp_pure const &p) -> ResultF { return p; },
            [](comp_lookup const &l) -> ResultF { return l; },
            [&f](comp_if<A> const &ci) -> ResultF {
                return comp_if<B>{smd::fixpoint::make_box<B>(f(*ci.cond)),
                                  smd::fixpoint::make_box<B>(f(*ci.cons)),
                                  smd::fixpoint::make_box<B>(f(*ci.alt))};
            },
            [&f](comp_lambda<A, MaxList> const &lam) -> ResultF {
                return comp_lambda<B, MaxList>{
                    lam.params, smd::fixpoint::make_box<B>(f(*lam.body))};
            },
            [&f](comp_apply<A, MaxList> const &app) -> ResultF {
                foundation::static_vector<smd::fixpoint::Box<B>, MaxList>
                    args_result;
                for (auto const &arg : app.args) {
                    args_result.push_back(
                        smd::fixpoint::make_box<B>(std::invoke(f, *arg)));
                }
                return comp_apply<B, MaxList>{
                    smd::fixpoint::make_box<B>(std::invoke(f, *app.func)),
                    std::move(args_result)};
            }},
        layer);
}
```

Leaves (`comp_pure`, `comp_lookup`) pass through unchanged. Internal nodes apply `f` to each child and rebuild the node with the transformed children. The type changes from `F<A>` to `F<B>` — the node shape is preserved, only the child type changes.

This is the `fmap` that makes `CompF` a functor in the categorical sense. With `fmap_comp` in place, generic recursion schemes — `fold_fix` (catamorphism) and `refold` (hylomorphism) — become expressible in terms of it (Bird, Richard S. and de Moor, Oege, 1997). (In this codebase such a generic fold is currently provided only for the separate `foundation::fix` type; for `Fix<CompF>` the later interpreters still walk the tree by hand.)


# core\_to\_comp: The Conversion Anamorphism

The function `core_to_comp` converts an arena-based `core_type` node into a `Comp` tree. Conceptually it is an anamorphism — an unfold that generates a recursive structure from a seed (Meijer, Erik and Fokkinga, Maarten and Paterson, Ross, 1991). In practice I use manual recursion rather than a generic `unfold_fix` because I need to propagate errors when encountering invalid nodes.

The application case shows the pattern clearly:

```c++
// src/smd/smdscheme/sender/comp_tree.hpp
[&arena](elaborator::core_application<Core, MaxNodes, MaxList> const &app)
    -> Res {
    auto func_r =
        core_to_comp<MaxNodes, MaxList>(arena.get(app.func), arena);
    if (!func_r.has_value())
        return func_r.error();

    foundation::static_vector<smd::fixpoint::Box<CompT>, MaxList> arg_boxes;
    for (auto const &arg_box : app.args) {
        auto arg_r = core_to_comp<MaxNodes, MaxList>(
            arena.get(arg_box), arena);
        if (!arg_r.has_value())
            return arg_r.error();
        arg_boxes.push_back(
            smd::fixpoint::make_box<CompT>(arg_r.value()));
    }

    return smd::fixpoint::wrap_fix<comp_f_factory<MaxList>::template type>(
        CompLayer{comp_apply<CompT, MaxList>{
            smd::fixpoint::make_box<CompT>(func_r.value()),
            std::move(arg_boxes)}});
}
```

`arena.get(app.func)` dereferences the integer handle and returns the function sub-node. That sub-node is converted recursively. On success, the result is wrapped in a `Box<CompT>` and assembled into a `comp_apply` node, which is then wrapped in `wrap_fix` to produce the final `Comp`.

`core_define` is rejected in expression context:

```c++
// src/smd/smdscheme/sender/comp_tree.hpp
[](elaborator::core_define<Core, MaxNodes> const &) -> Res {
    return foundation::parse_error{
        {}, "comp_tree: define not supported in expression context"};
}
```

`define` is a top-level declaration, not an expression. Encountering it inside a `core_to_comp` traversal means the source program is malformed.


# Why Two Representations?

The arena-based core tree and `Fix<CompF>` serve different roles.

The arena tree is optimized for `constexpr` construction. Arena handles are plain integers — no allocations, no pointers. The elaborator runs entirely at compile time, filling a fixed-size arena on the stack. The arena can be statically sized because the program text is bounded.

`Fix<CompF>` with `Box` children is optimized for traversal. A `Comp` node is self-contained: it does not need an external arena to dereference its children. The Mendler-style interpreter in the next phase walks `Comp` trees by pattern-matching on `unwrap_fix(node)` and recursing directly into `Box`-stored children. Passing only the node — no arena — keeps the interpreter's interface clean and makes closures straightforward to represent: a closure captures a `Box<Comp>` body, not a handle plus arena.

The conversion step is a one-time cost at the boundary between the front end and the evaluator.


# What Comes Next

`Fix<CompF>` is the evaluator's input. The next phase introduces the Mendler-style interpreter: a fold over `Comp` trees that threads an environment and produces `closure::value` results. The `fmap_comp` functor map defined here underpins that fold.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [Next: Phase 6 - Closures and Values →](phase-6-closures.md)

</nav>


# References

Bird, Richard S. and de Moor, Oege (1997). **The Algebra of Programming**, Prentice Hall.

Meijer, Erik, Fokkinga, Maarten, and Paterson, Ross (1991). **Functional Programming with Bananas, Lenses, Envelopes and Barbed Wire**, FPCA.

P3019R13 (2024). **std::indirect and std::polymorphic**, ISO C++ Committee. (Coe, Jonathan and others, 2024)
