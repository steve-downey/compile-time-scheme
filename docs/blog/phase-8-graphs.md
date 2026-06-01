<div class="abstract" id="org2d28f49">
<p>
By lowering my Scheme AST into heavily typed <code>std::execution</code> Senders, I gained improved performance.
But I also created an introspective nightmare.
The resulting Sender AST is a large set of deeply nested template permutations.
How do I prove what the C++ compiler is actually building without deciphering thousands of lines of compiler error backtraces?
</p>

</div>


# The Reflection Bridge

In Phase 8, I utilize C++26 Reflection (`std::meta::info`) alongside a custom `string_writer` Applicative (McBride, Conor and Paterson, Ross, 2008) to project the generated execution typings into the Graphviz (??, ????) dot-language for visual rendering.

C++26 Reflection (P2996) allows C++ programs to imperatively query the structural definitions of types during constant evaluation.

Instead of writing recursive template metaprogramming tricks with partial specializations, I write a straightforward loop that interacts via `<meta>` using the newly implemented `^^Sender` reflection syntax.

Because \`beman::execution\` wraps its underlying types inside \`basic\_sender<Tag, &hellip;>\`, `std::meta::identifier_of` alone isn't sufficient (it just yields "basic\_sender"). Instead, I retrieve the full signature via `std::meta::display_string_of` and parse out markers for "just\_t", "when\_all\_t", or "then\_t", allowing me to correctly classify the nodes. I extract its base class, recursively step into its template arguments, and map the sender topology into a strongly typed graph at \`consteval\` time.

```cpp
/// Classifies a sender type by the first tag name found in its display string.
///
/// Beman Execution26 senders have the form
/// @c basic_sender<Tag,Data,Child...>; the outermost tag always appears
/// first in the display string because it is @c basic_sender's first template
/// argument.  A position-based min-find is used instead of a simple string
/// search to avoid false matches from nested child tag names.
///
/// @param type A P2996 reflection of the sender type.
/// @return One of @c "just", @c "then", @c "when_all", or @c "unknown".
consteval auto classify_sender(std::meta::info type) -> std::string_view {
    std::string_view name = std::meta::display_string_of(type);
    auto jp = name.find("just_t");
    auto tp = name.find("then_t");
    auto wp = name.find("when_all_t");
    auto first = std::min({jp, tp, wp});
    if (first == std::string_view::npos)
        return "unknown";
    if (first == wp)
        return "when_all";
    if (first == tp)
        return "then";
    return "just";
}

/// Recursively builds a @ref scheme_tree by reflecting on a sender type.
///
/// Traversal strategy (no splice — GCC16 cannot use @c [:r:] on function
/// parameters at consteval time):
/// 1. Classify the outermost sender via @ref classify_sender.
/// 2. Use @c bases_of(sender_type) to get the @c product_type base class.
/// 3. Use @c type_of(base) + @c template_arguments_of to get
///    @c [Tag, Data, Child...].
/// 4. Recurse for each @c Child (args[2+]).
///
/// @tparam MaxNodes Maximum tree capacity (default 64).
/// @param  sender_type P2996 reflection of the sender type to traverse.
/// @param  tree        Output tree; nodes are appended in pre-order.
/// @return The integer id of the root node allocated for @p sender_type.
template <int MaxNodes = 64>
consteval auto build_scheme_tree(std::meta::info sender_type,
                                 scheme_tree<MaxNodes> &tree) -> int {
    scheme_node_data node{};
    node.sender_algo = classify_sender(sender_type);
    auto id = tree.allocate(node);

    auto bases = std::meta::bases_of(sender_type,
                                     std::meta::access_context::unchecked());
    if (!bases.empty()) {
        auto args =
            std::meta::template_arguments_of(std::meta::type_of(bases[0]));
        for (std::size_t i = 2; i < args.size(); ++i) {
            auto child_id = build_scheme_tree<MaxNodes>(args[i], tree);
            tree.get(id).child_ids.push_back(child_id);
        }
    }

    return id;
}
```

This recursively unpacks the deeply typed sequence and maps it onto a homogeneously typed `scheme_tree` topology, annotating the Sender algorithm properly. All of this runs strictly during compile-time.


# Graph Translation via Applicatives

Once I possess a homogeneous C++ tree, I turn once again to Category Theory to render the Graphviz dot-language diagram.

Using the `Applicative` and `Traversable` typeclasses mapped within the repository (Phase 1), I constructed a `string_writer` Applicative. The monadic and alternative interfaces allow me to map functionally over the returned string values. While the Graphviz mapping is currently driven through a straightforward indexing iteration, the types align nicely allowing pure state accumulation.

```cpp
/// Renders a @ref scheme_tree as a complete Graphviz DOT graph string.
///
/// Formats each node in index order using @ref format_scheme_node and
/// wraps the result in a @c digraph block.
///
/// @tparam MaxNodes Tree capacity.
/// @param  tree The pre-built scheme tree to render.
/// @return A @c std::string containing the full DOT source.
template <int MaxNodes>
auto format_tree(scheme_tree<MaxNodes> const &tree) -> std::string {
    std::string dot = "digraph SchemeExecutionPlan {\n";
    dot += "  node [shape=Mrecord, style=filled, fillcolor=lightcyan];\n";

    for (int i = 0; i < static_cast<int>(tree.nodes.size()); ++i) {
        dot += format_scheme_node(tree.get(i)).log;
    }

    dot += "}\n";
    return dot;
}

/// Reflects on @p Sender at compile time, builds a @ref scheme_tree, then
/// emits Graphviz DOT to @p out at runtime.
///
/// The tree is built in a @c constexpr lambda so the reflection happens once
/// per @p Sender specialization.  At runtime only the string rendering runs.
///
/// Usage:
/// @code
///   using S = decltype(sender_v::then(sender_v::just(1),
///                                     [](int x){ return x + 1; }));
///   sender::dump_scheme_plan<S>(std::cout);
/// @endcode
///
/// @tparam Sender   The sender type whose execution plan is to be shown.
/// @tparam MaxNodes Maximum tree capacity (default 64).
/// @param  out      Output stream; defaults to @c std::cout.
template <typename Sender, int MaxNodes = 64>
void dump_scheme_plan(std::ostream &out = std::cout) {
    constexpr auto tree = [] {
        scheme_tree<MaxNodes> t{};
        auto root = build_scheme_tree<MaxNodes>(^^Sender, t);
        t.root_id = root;
        return t;
    }();

    out << format_tree(tree);
}
```

The `dump_scheme_plan` function encapsulates the compile-time reflection traversal into a `constexpr` lambda, cleanly executing the heavy \`meta\` machinery strictly during translation.


# Conclusion

Using the reflection bridge to populate my runtime topology completely removes the black-box nature of compile-time meta-programming. The C++26 reflection framework allows me to natively render the exact asynchronous execution pipeline (`std::execution`) constructed for any Scheme program.

Running the output directly through `dot -Tpng -o plan.png` grants immediate, visual feedback of how functional continuations map to low-level native scheduling operations.


# References

McBride, Conor and Paterson, Ross (2008). *Applicative Programming with Effects*, Journal of Functional Programming.

(). *Graphviz DOT Language*.
