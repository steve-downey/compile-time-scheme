<div class="abstract" id="orgb94e24a">
<p>
By lowering our Scheme AST into heavily typed <code>std::execution</code> Senders, we gained immense performance.
But we also created an introspective nightmare.
The resulting Sender AST is a massive blob of deeply nested template permutations.
How do we prove what the C++ compiler is actually building without deciphering thousands of lines of compiler error backtraces?
</p>

</div>


# The Reflection Bridge

In Phase 8, we build an Applicative DOT traverser. Leveraging C++26 Reflection (`std::meta::info`), we introspect the generated execution typings and project them into the Graphviz dot-language for visual rendering.

C++26 Reflection (P2996) allows C++ programs to imperatively query the structural definitions of types during constant evaluation.

Instead of writing nightmarish recursive template metaprogramming tricks with partial specializations, we write a straightforward loop that interacts via `<meta>`. Given a Sender's `std::meta::info`, we extract its identifier (say, `"just"`, `"when_all"`, or `"then"`) and parse its template arguments perfectly.

```c++
template <typename TreeType>
constexpr TreeType build_scheme_tree(std::meta::info sender_type, int& id_counter) {
    SchemeNodeData current;
    current.node_id = id_counter++;
    current.sender_algo = std::string(std::meta::identifier_of(sender_type));

    // If it's a function application trigger
    if (current.sender_algo == "then") {
        current.scheme_context = "Function Application";
        auto args = std::meta::template_arguments_of(sender_type);
        if (args.size() >= 2) {
            auto predecessor = build_scheme_tree<TreeType>(args[0], id_counter);
            // Link edge
            current.child_ids.push_back(predecessor.data.node_id);
        }
    }
    // ...
}
```

This recursively unpacks the deeply typed sequence and maps it onto a homogeneously typed `SchemeTree` topology, annotating both the Sender algorithm and its originating Scheme context.


# Graph Translation via Applicatives

Once we possess a homogeneous C++ tree, we turn once again to Category Theory to render the Graphviz dot-language diagram.

Using the `Applicative` and `Traversable` typeclasses, we construct a `StringWriter` Applicative. The traversable interface allows us to map functionally over the tree structure, appending string state uniformly without managing a monolithic concatenation process manually.

```c++
// The traversable sweeps the string writer across all SchemeNodeData points,
// accumulating DOT nodes and edges.
```


# Conclusion

Using the reflection bridge to populate our runtime topology completely removes the black-box nature of compile-time meta-programming. The C++26 reflection framework allows us to natively render the exact asynchronous execution pipeline (`std::execution`) constructed for any Scheme program. Running the output directly through `dot -Tpng -o plan.png` grants immediate, visual feedback of how functional continuations map to low-level native scheduling operations.


# References

-   Brown, D. et al. (2024). "P2996: Reflection for C++26". C++ Standards Committee Papers.
-   Graphviz DOT Language, <https://graphviz.org/doc/info/lang.html>
-   "Applicative Programming with Effects", McBride & Paterson (2008).
