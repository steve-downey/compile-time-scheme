// src/smd/smdlisp/sender/dump_lisp_plan.hpp                      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_SENDER_DUMP_LISP_PLAN_HPP
#define SRC_SMD_SMDLISP_SENDER_DUMP_LISP_PLAN_HPP

#include <smd/smdlisp/sender/build_lisp_tree.hpp>
#include <smd/smdlisp/sender/lisp_plan_tree.hpp>
#include <smd/smdscheme/sender/string_writer.hpp>

#include <format>
#include <iostream>
#include <string>
#include <variant>

namespace smd::smdlisp::sender {

/// Formats a single @ref lisp_node_data as Graphviz DOT node and edge
/// declarations.
///
/// Reuses `smd::smdscheme::sender::string_writer` rather than growing an
/// `smdlisp` copy: the frozen Scheme tree is read-only, not off-limits, and
/// a second writer monad would be duplication for its own sake.
///
/// @param node The sender graph node to render.
/// @return A writer whose log holds this node's DOT declarations.
inline auto format_lisp_node(lisp_node_data const &node)
    -> smd::smdscheme::sender::string_writer<std::monostate> {
    std::string dot =
        std::format("  n{} [label=\"{}\\n[{}]\"];\n", node.node_id,
                    node.sender_algo, node.lisp_context);

    for (int child_id : node.child_ids) {
        dot += std::format("  n{} -> n{};\n", node.node_id, child_id);
    }

    return smd::smdscheme::sender::tell(std::move(dot));
}

/// Renders a @ref lisp_plan_tree as a complete Graphviz DOT graph string.
///
/// @tparam MaxNodes Tree capacity.
/// @param  tree The pre-built plan tree to render.
/// @return The full DOT source.
template <int MaxNodes>
auto format_lisp_tree(lisp_plan_tree<MaxNodes> const &tree) -> std::string {
    std::string dot = "digraph LispExecutionPlan {\n";
    dot += "  node [shape=Mrecord, style=filled, fillcolor=lightyellow];\n";

    for (int i = 0; i < static_cast<int>(tree.nodes.size()); ++i) {
        dot += format_lisp_node(tree.get(i)).log;
    }

    dot += "}\n";
    return dot;
}

/// Reflects on @p Sender at compile time and returns its execution plan as
/// Graphviz DOT.
///
/// The tree is built in a @c constexpr lambda, so the reflection happens once
/// per @p Sender specialisation and only the string rendering runs at
/// runtime.
///
/// @tparam Sender   The sender type whose execution plan is wanted.
/// @tparam MaxNodes Maximum tree capacity (default 64).
template <typename Sender, int MaxNodes = 64>
auto lisp_plan_dot() -> std::string {
    constexpr auto tree = [] {
        lisp_plan_tree<MaxNodes> t{};
        auto root = build_lisp_tree<MaxNodes>(^^Sender, t);
        t.root_id = root;
        return t;
    }();

    return format_lisp_tree(tree);
}

/// Writes @p Sender's execution plan to @p out as Graphviz DOT.
///
/// Usage mirrors @ref smd::smdscheme::sender::dump_scheme_plan:
/// @code
///   using S = decltype(build_unwind_protect_plan());
///   sender::dump_lisp_plan<S>(std::cout);
/// @endcode
///
/// @tparam Sender   The sender type whose execution plan is to be shown.
/// @tparam MaxNodes Maximum tree capacity (default 64).
/// @param  out      Output stream; defaults to @c std::cout.
template <typename Sender, int MaxNodes = 64>
void dump_lisp_plan(std::ostream &out = std::cout) {
    out << lisp_plan_dot<Sender, MaxNodes>();
}

} // namespace smd::smdlisp::sender

#endif
