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

Because \`beman::execution\` wraps its underlying types inside \`basic\_sender<Tag, &hellip;>\`, the reflection bridge walks ~template_arguments_of~ to obtain `[Tag, Data, Child...]` and recurses on each child. There is one subtlety: P2996 reflections preserve alias structure, so when the user writes `using S = decltype(sender_v::just(42))` and reflects `^^S`, the resulting `std::meta::info` is an alias-entity reflection. `template_arguments_of` on an alias throws because aliases do not have template arguments — the type they denote does. `std::meta::dealias` peels the alias layer off and returns the underlying class template specialisation. Tag classification has a parallel subtlety: the tag may be a template specialisation (e.g. `just_t<set_value_t>`) or a plain class (e.g. `when_all_t`); `has_template_arguments` distinguishes the two so we can read the right identifier in each case.

```cpp
/// Classifies a sender type by reading the unqualified identifier of its tag.
///
/// Beman Execution26 senders have the form
/// @c basic_sender<Tag,Data,Child...>. The tag may be a class template
/// specialisation (e.g. @c just_t<set_value_t>) or a plain class
/// (e.g. @c when_all_t); both are reduced to their unqualified identifier.
/// We dealias the sender type because P2996 reflections preserve alias
/// structure and @c template_arguments_of requires a class template
/// specialisation, not an alias.
///
/// @param type A P2996 reflection of the sender type.
/// @return One of @c "just", @c "then", @c "when_all", or @c "unknown".
consteval auto classify_sender(std::meta::info type) -> std::string_view {
    auto args = std::meta::template_arguments_of(std::meta::dealias(type));
    if (args.empty())
        return "unknown";
    auto tag = std::meta::dealias(args[0]);
    auto tag_name = std::meta::has_template_arguments(tag)
                        ? std::meta::identifier_of(std::meta::template_of(tag))
                        : std::meta::identifier_of(tag);
    if (tag_name == "just_t")
        return "just";
    if (tag_name == "then_t")
        return "then";
    if (tag_name == "when_all_t")
        return "when_all";
    return "unknown";
}

/// Recursively builds a @ref scheme_tree by reflecting on a sender type.
///
/// @c basic_sender<Tag,Data,Child...> — args[0]=Tag, args[1]=Data,
/// args[2+]=Child. P2996 reflections preserve aliases, so the sender type
/// (typically obtained via @c decltype(factory_call())) must be dealiased
/// before @c template_arguments_of will recognise it as a specialisation.
///
/// @tparam MaxNodes Maximum tree capacity (default 64).
/// @param  sender_type P2996 reflection of the sender type to traverse.
/// @param  tree        Output tree; nodes are appended in pre-order.
/// @return The integer id of the root node allocated for @p sender_type.
template <int MaxNodes = 64>
consteval auto build_scheme_tree(std::meta::info sender_type,
                                 scheme_tree<MaxNodes> &tree) -> int {
    auto type = std::meta::dealias(sender_type);
    scheme_node_data node{};
    node.sender_algo = classify_sender(type);
    auto id = tree.allocate(node);

    auto args = std::meta::template_arguments_of(type);
    for (std::size_t i = 2; i < args.size(); ++i) {
        auto child_id = build_scheme_tree<MaxNodes>(args[i], tree);
        tree.get(id).child_ids.push_back(child_id);
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
