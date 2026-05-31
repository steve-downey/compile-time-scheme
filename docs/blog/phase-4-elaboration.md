<div class="abstract" id="orgd42bcc4">
<p>
With our program read into a pure nested list of symbols, integers, and booleans, it is time to assign meaning.
Phase 4 introduces the <b>Elaborator</b>, generating a strict, explicitly-typed Semantic Abstract Syntax Tree known as the <code>Core</code> language.
</p>

</div>


# Semantics over Syntax

While the Reader handles syntax, the Elaborator handles semantics. It traverses the `Datum` tree, inspects the layout of lists, binds variables to lexical scopes, and verifies that special forms are used correctly.

For example, when the Elaborator encounters a list whose first element is the symbol `'if`:

```scheme
(if condition true_branch false_branch)
```

It transforms this `Datum` list into a `core_if` node.

```c++
struct core_if {
    core_node_id condition_id;
    core_node_id true_branch_id;
    core_node_id false_branch_id;
};
```


# Validation

This separation of concerns means that all syntax validation is centralized in the Elaborator.

-   Did the user pass three arguments to `if`?
-   Are the arguments to a `(lambda (x y) ...)` form actually variable symbols?
-   Is a variable symbol referencing an item that exists in the current lexical environment?

If there are malformed structures, the Elaborator rejects them. Because this occurs natively at compile-time in C++, invalid Scheme code results directly in a C++ compiler error before execution begins. You find out your syntax is wrong before the program even officially exists.


# Variable Binding and De Bruijn Indices

One of the more complex tasks in elaboration is mapping variable usage to actual variable definitions in closures. In many naive interpreters, variables are stored as strings in a Hash Map environment at runtime. This is terrible for performance.

A faster, more formal approach (which we map toward here) uses positional indices based on scope depth, similar to De Bruijn indexing. The Elaborator can "resolve" a variable symbol to know exactly which lexical block it belongs to, entirely stripping the need for expensive, string-based lookups during code execution.


# Direct Evaluation vs. Compilation

To verify that our `Core` AST is correctly elaborated, we build an `eval_direct` function. This is a traditional tree-walk interpreter that evaluates nodes recursively.

```c++
constexpr Value eval_direct(core_node_id id, Environment env) {
    auto node = get_node(id);
    if (auto* i = std::get_if<core_if>(&node)) {
         if (eval_direct(i->condition_id, env).is_truthy()) {
             return eval_direct(i->true_branch_id, env);
         } else {
             return eval_direct(i->false_branch_id, env);
         }
    }
    // ...
}
```

While this proves the elaborated semantics are correct, evaluating deep recursive structures this way directly utilizes the C++ call stack. Deep recursion will trigger a C++ compiler stack overflow, even at compile-time. There is compiling your code, and then there is crashing Clang. We need a way to decouple Scheme's control flow from C++'s native call stack. This leads directly to our middle-end architecture.


# References

-   Queinnec, C. (1996). "Lisp in Small Pieces." Cambridge University Press.
-   De Bruijn, N. G. (1972). "Lambda calculus notation with nameless dummies, a tool for automatic formula manipulation, with application to the Church-Rosser theorem." Indagationes Mathematicae.
