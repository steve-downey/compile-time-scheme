# Architecture Plan: Visualizing Scheme Execution Senders with C++26 Reflection

## Status

**Implemented:** Phases 1–4 are complete.

- Phase 1: `scheme_node_data.hpp`, `scheme_tree.hpp` — runtime topology types
- Phase 2: `build_scheme_tree.hpp` — C++26 reflection bridge
- Phase 3: `string_writer.hpp`, `format_scheme_node.hpp` — Applicative DOT traverser
- Phase 4: `dump_scheme_plan.hpp` — top-level API

**Example:** `src/examples/sender_graph_demo.cpp`

**Usage:**

```bash
./build/src/examples/sender_graph_demo | dot -Tpng -o plan.png
```

## Overview
This document outlines the architectural plan to integrate a DOT-format graph printer into the `compile-time-scheme` compiler (`steve-downey/compile-time-scheme`). The compiler translates Scheme expressions into deeply nested `std::execution` (P2300) senders.

By utilizing **GCC 16 with C++26 Reflection (P2996)**, we can bypass complex template metaprogramming and imperatively parse the heavily type-erased Sender AST. We then project this structure into a homogeneous runtime `Tree` and utilize the `Applicative` and `Traversable` typeclasses (from `steve-downey/trees/src/typeclass`) to elegantly generate a Graphviz DOT visualization of the execution plan.

---

## Phase 1: Define the Runtime Topology Target
The compile-time sender types must be projected into a uniform runtime data structure. We extend the basic node data to capture both the underlying sender algorithm and the originating Scheme context.

```cpp
#include <string>
#include <vector>

// Captures both the C++ Sender topology and the Scheme semantics
struct SchemeNodeData {
    int node_id;
    std::string sender_algo;    // e.g., "just", "when_all", "then"
    std::string scheme_context; // e.g., "Atom: 5", "Arg Eval", "Apply: +"
    std::vector<int> child_ids; // IDs of dependent senders for DOT edges
};

// Assuming the Tree structure from steve-downey/trees
template <typename T>
struct Tree;

using SchemeTree = Tree<SchemeNodeData>;
```

---

## Phase 2: The C++26 Reflection Bridge
This is the core translation step. A recursive `constexpr` function introspects the generated Scheme Sender using `<meta>` and populates the `SchemeTree`.

```cpp
#include <meta>

template <typename TreeType>
constexpr TreeType build_scheme_tree(std::meta::info sender_type, int& id_counter) {
    SchemeNodeData current_node;
    current_node.node_id = id_counter++;
    current_node.sender_algo = std::string(std::meta::identifier_of(sender_type));

    std::vector<TreeType> children;

    // Pattern match on the Sender algorithms used by the Scheme compiler
    if (current_node.sender_algo == "just") {
        current_node.scheme_context = "Atom"; // Further introspection can extract exact values
    }
    else if (current_node.sender_algo == "when_all") {
        current_node.scheme_context = "Parallel Argument Evaluation";
        // Iterate over the arguments of when_all and link them as children
        for (std::meta::info arg_info : std::meta::template_arguments_of(sender_type)) {
            if (std::meta::type_of(arg_info)) {
                auto child_tree = build_scheme_tree<TreeType>(arg_info, id_counter);
                current_node.child_ids.push_back(child_tree.data.node_id);
                children.push_back(child_tree);
            }
        }
    }
    else if (current_node.sender_algo == "then") {
        current_node.scheme_context = "Function Application";
        // Extract predecessor sender and invocable
        auto args = std::meta::template_arguments_of(sender_type);
        if (args.size() >= 2) {
            auto predecessor = build_scheme_tree<TreeType>(args[0], id_counter);
            current_node.child_ids.push_back(predecessor.data.node_id);
            children.push_back(predecessor);
        }
    }

    return TreeType{current_node, children};
}
```

---

## Phase 3: The Applicative Traverser
To convert the `SchemeTree` into a DOT string, we construct a `StringWriter` Applicative. The `traversable` typeclass will map over the tree, formatting each node, while the Applicative implicitly handles concatenating the accumulated string state.

### 3.1 The StringWriter Applicative
```cpp
#include <string>
#include <utility>

template <typename T>
struct StringWriter {
    std::string log;
    T value;

    // Applicative 'pure'
    static StringWriter<T> pure(T val) {
        return StringWriter<T>{"", std::move(val)};
    }
};

// Applicative 'liftA2' / 'ap'
template <typename F, typename A, typename B>
auto liftA2(F func, const StringWriter<A>& a, const StringWriter<B>& b)
    -> StringWriter<decltype(func(a.value, b.value))>
{
    return {
        a.log + b.log,
        func(a.value, b.value)
    };
}
```

### 3.2 The DOT Formatting Lambda
```cpp
#include <format>
#include <variant>

// The function mapped over the tree via traverse
auto format_scheme_node = [](const SchemeNodeData& node) -> StringWriter<std::monostate> {
    // Format the Graphviz node definition
    std::string label = std::format("{}\\n[{}]", node.sender_algo, node.scheme_context);
    std::string dot_chunk = std::format("  n{} [label=\"{}\"];\n", node.node_id, label);

    // Format the outgoing edges to child senders
    for (int child_id : node.child_ids) {
        dot_chunk += std::format("  n{} -> n{};\n", node.node_id, child_id);
    }

    return StringWriter<std::monostate>{dot_chunk, std::monostate{}};
};
```

---

## Phase 4: Compiler API and Integration
Finally, wire the printer into the compiler's diagnostic/testing pathways. This utility bridges the compile-time generation with the runtime traversal.

```cpp
#include <iostream>

template <typename Sender>
void dump_scheme_plan(const Sender& snd, std::ostream& out = std::cout) {
    int id = 0;

    // 1. Reify the Sender AST into a SchemeTree
    auto tree = build_scheme_tree<SchemeTree>(^Sender, id);

    // 2. Traverse the tree using Applicative accumulation
    StringWriter<std::monostate> result = traverse(format_scheme_node, tree);

    // 3. Output the final Graphviz structure
    out << "digraph SchemeExecutionPlan {\n";
    out << "  node [shape=Mrecord, style=filled, fillcolor=lightcyan];\n";
    out << result.log;
    out << "}\n";
}

// Example usage in smd/schemepoc tests:
void test_scheme_eval() {
    auto plan = compile_scheme("(+ 1 2)"); // Produces nested Sender

    // Print the graph before execution
    dump_scheme_plan(plan);

    // Execute the plan
    auto result = stdexec::sync_wait(plan);
}
```
