// src/smd/smdscheme/sender/dump_scheme_plan.hpp                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_DUMP_SCHEME_PLAN_HPP
#define SRC_SMD_SMDSCHEME_SENDER_DUMP_SCHEME_PLAN_HPP

#include <smd/smdscheme/sender/build_scheme_tree.hpp>
#include <smd/smdscheme/sender/format_scheme_node.hpp>
#include <smd/smdscheme/sender/string_writer.hpp>

#include <iostream>
#include <string>

namespace smd::smdscheme::sender {

// Build the DOT string for a scheme_tree by formatting each node.
template <int MaxNodes>
auto format_tree(scheme_tree<MaxNodes> const& tree) -> std::string {
    std::string dot = "digraph SchemeExecutionPlan {\n";
    dot += "  node [shape=Mrecord, style=filled, fillcolor=lightcyan];\n";

    for (int i = 0; i < static_cast<int>(tree.nodes.size()); ++i) {
        dot += format_scheme_node(tree.get(i)).log;
    }

    dot += "}\n";
    return dot;
}

// Reflect on a Sender type, build the execution-plan tree at compile time,
// and emit Graphviz DOT to an output stream at runtime.
//
// Usage:
//   using S = decltype(sender_v::then(sender_v::just(1), [](int x){ return x+1; }));
//   sender::dump_scheme_plan<S>(std::cout);
template <typename Sender, int MaxNodes = 64>
void dump_scheme_plan(std::ostream& out = std::cout) {
    constexpr auto tree = [] {
        scheme_tree<MaxNodes> t{};
        auto root = build_scheme_tree<MaxNodes>(^^Sender, t);
        t.root_id = root;
        return t;
    }();

    out << format_tree(tree);
}

} // namespace smd::smdscheme::sender

#endif
