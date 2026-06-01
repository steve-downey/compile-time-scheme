<div class="abstract" id="org1173943">
<p>
With my program successfully read into a pure nested list of symbols, integers, and booleans during the Reader phase, it is time to assign semantic meaning.
Phase 4 introduces the <b>Elaborator</b>, generating a strict, explicitly-typed Semantic Abstract Syntax Tree known as the <code>core</code> language.
This post walks through the structural differences between raw syntax and semantics, how special forms are mapped, and building a direct evaluator to verify everything works before lowering to continuations.
</p>

</div>


# Semantics over Syntax

While the Reader handles syntax, the Elaborator handles semantics. The reader produces a generic tree of lists, atoms, and symbols. It does not know if `(if #t 1 2)` is a function call, a variable definition, or a control flow construct—it just sees a list of four items. The Elaborator traverses this `datum` tree, inspects the layout of lists, binds variables to lexical scopes, and verifies that special forms are used correctly to construct the `core` AST.

For example, when the Elaborator encounters a list whose first element is the symbol `if`, it maps it to a `core_if` node. In C++, this is defined as follows:

```cpp
template <typename R, int MaxNodes>
struct core_if {
    foundation::arena_box<R, MaxNodes> condition; ///< The test expression.
    foundation::arena_box<R, MaxNodes>
        consequent; ///< Branch taken when truthy.
    foundation::arena_box<R, MaxNodes>
        alternative; ///< Branch taken when false.
};
```

Note how explicit the types are. Instead of arbitrary nested lists, the `core_if` node strictly mandates a condition, a consequent, and an alternative. These branches are handles (`arena_box`) to other nodes allocated in the AST arena, preventing deep recursion overhead.


# The Core Types

The Scheme language semantics are expressed cleanly in our strongly-typed \`core\` AST. Elaborated nodes represent the fundamental building blocks of standard Scheme expressions:


## Literals (`core_integer`, `core_boolean`, `core_symbol`)

Atoms evaluated simply to themselves or looked up in the current environment:

-   `core_integer`: Stores an \`int\`, evaluating verbatim.
-   `core_boolean`: Stores a \`bool\`, driving logical conditionals.
-   `core_symbol`: A string representing a variable binding. During execution, it triggers an environment lookup to find the corresponding value.


## `core_quote`

Any form evaluating to raw data instead of executing. The `quote` form suppresses evaluation. An elaborated `core_quote` simply holds an internally variant value that bypasses compilation of its contents.


## Special Forms

The Elaborator ensures special forms adhere strictly to their shape:


### `core_if`

Represents branching control flow. It ensures execution evaluates only the required branch based on truthiness of the condition. Scheme specifically treats \`#f\` as false and everything else as truthy.


### `core_lambda`

The heart of Scheme functions. It holds a list of explicit parameter names and exactly one body expression. This will later map to closures by saving its surrounding lexical environment.


### `core_define`

Parsed to bind a value expression to a name in the global/current environment. While currently only parsed into the AST without full backend environment mutation tracking, it enforces the syntactic rule that a valid symbol must accompany the assignment.


### `core_application`

A function call. Holds the function expression block and an array of explicitly evaluated arguments. This differentiates function application from macro or special form macros by evaluating arguments greedily.


# Validation and Elaboration

This separation of concerns means that all syntax validation is centralized in the Elaborator.

-   Did the user pass exactly three arguments to `if`?
-   Are the arguments to a `(lambda (x y) ...)` form actually valid variable symbols?

If there are malformed structures, the Elaborator rejects them. Because this occurs natively at compile-time in C++, invalid Scheme code results directly in a C++ compiler error before execution begins. You find out your syntax is wrong before the program even officially exists.

Here is the exact code that elaborates an `if` form from a list of tokens:

```cpp
if (lst.elements.size() != 4)
        return foundation::parse_error{{}, "if: expected 3 arguments"};

auto cond_r = elaborate_node<MaxNodes, MaxList>(
        datum_arena.get(lst.elements[1]), datum_arena, core_arena);
if (!cond_r.has_value())
        return cond_r;

auto cons_r = elaborate_node<MaxNodes, MaxList>(
        datum_arena.get(lst.elements[2]), datum_arena, core_arena);
if (!cons_r.has_value())
        return cons_r;

auto alt_r = elaborate_node<MaxNodes, MaxList>(
        datum_arena.get(lst.elements[3]), datum_arena, core_arena);
if (!alt_r.has_value())
        return alt_r;

return core{core_f{core_if<core, MaxNodes>{
        make_arena_box(core_arena, std::move(cond_r.value())),
        make_arena_box(core_arena, std::move(cons_r.value())),
        make_arena_box(core_arena, std::move(alt_r.value()))}}};
```

If the size of the list isn't exactly 4, it emits a `parse_error`. If it is 4, it recursively elaborates the condition, consequent, and alternative nodes before constructing the `core_if` struct inside the arena.

Notice how this handles malformed syntax explicitly as a compiler error natively:

```cpp
static_assert([] {
    using Core = smd::smdscheme::elaborator::core_type<32, 16>;
    smd::smdscheme::foundation::tree_arena<
        smd::smdscheme::reader::datum_type<32, 16>, 32>
        datum_arena;
    smd::smdscheme::foundation::tree_arena<Core, 32> core_arena;

    // Bad syntax: 'if' requires 3 arguments (condition, consequent,
    // alternative)
    auto dr = smd::smdscheme::reader::read_datum<32, 16>(
        smd::smdscheme::parser::cursor{"(if #t 1)"sv}, datum_arena);
    if (!dr.has_value())
        return false;

    auto er = smd::smdscheme::elaborator::elaborate<32, 16>(
        dr.value().value, datum_arena, core_arena);

    // Elaboration should fail naturally here
    return !er.has_value();
}());
```


# Variable Binding

One of the more complex tasks in elaboration is mapping variable usage to actual references. In a naive interpreter, variables are stored as strings in a Hash Map environment at runtime. While our current implementation utilizes a \`constexpr\` native environment with string lookups to prove correctness, a faster, more formal approach replaces string lookups with positional indices based on scope depth, known as De Bruijn indexing (De Bruijn, Nicolaas Govert, 1972). An Elaborator could theoretically resolve a variable symbol to its lexical depth directly on the struct, stripping the need for expensive string-based lookups during code execution. This is a common optimization path I intend to pursue for production-ready speeds.


# Direct Evaluation vs. Compilation

To verify that my `core` AST is correctly elaborated, I build an `eval_direct` function. This is a straightforward tree-walk interpreter over the `core` AST that evaluates the program entirely before I introduce Continuation-Passing Style (CPS) transformation.

```cpp
auto const &cif =
    std::get<elaborator::core_if<Core, MaxNodes>>(node.inner);
auto cond_r = eval_direct<MaxNodes, MaxList, MaxBindings>(
    arena.get(cif.condition), arena, environment);
if (!cond_r.has_value()) {
    return cond_r.error();
}
auto const &cond_val = cond_r.value();
if (std::holds_alternative<bool>(cond_val) &&
    !std::get<bool>(cond_val)) {
    return eval_direct<MaxNodes, MaxList, MaxBindings>(
        arena.get(cif.alternative), arena, environment);
}
return eval_direct<MaxNodes, MaxList, MaxBindings>(
    arena.get(cif.consequent), arena, environment);
```

Here, the `eval_direct` function takes the tree arena and evaluates the `if` logic directly using C++'s native conditional mechanics. It evaluates the condition node, checks for truthiness, and appropriately evaluates either the consequent or the alternative.

While this proves the elaborated semantics are valid, evaluating deep recursive structures this way directly utilizes the C++ call stack. Deep recursion will trigger a C++ compiler stack overflow, even at compile-time. There is compiling your code, and then there is crashing Clang. I need a way to decouple Scheme's control flow from C++'s native call stack. This limitation naturally pushes us toward Continuation-Passing Style (CPS) architecture, which will be the next step in our series.


# References

(Steele, Guy L. and Sussman, Gerald Jay, 1978, Queinnec, Christian, 1996)

De Bruijn, Nicolaas Govert (1972). *Lambda calculus notation with nameless dummies, a tool for automatic formula manipulation, with application to the Church-Rosser theorem*, Indagationes Mathematicae.

Queinnec, Christian (1996). *Lisp in Small Pieces*, Cambridge University Press.

Steele, Guy L. and Sussman, Gerald Jay (1978). *The Art of the Interpreter or, the Modularity Complex (Parts Zero, One, and Two)*, MIT AI Lab.
