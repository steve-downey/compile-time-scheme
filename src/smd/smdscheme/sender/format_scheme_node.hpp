// src/smd/smdscheme/sender/format_scheme_node.hpp              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_FORMAT_SCHEME_NODE_HPP
#define SRC_SMD_SMDSCHEME_SENDER_FORMAT_SCHEME_NODE_HPP

#include <smd/smdscheme/sender/scheme_node_data.hpp>
#include <smd/smdscheme/sender/string_writer.hpp>

#include <format>
#include <variant>

namespace smd::smdscheme::sender {

/// Formats a single @ref scheme_node_data as Graphviz DOT node and edge
/// declarations, returning them as a @ref string_writer log.
///
/// The returned writer carries @c std::monostate as its value; callers
/// extract the accumulated DOT text via the @c .log member.
///
/// @param node The sender graph node to render.
/// @return A @c string_writer<std::monostate> whose log contains the DOT
///         declarations for this node and its outgoing edges.
inline auto format_scheme_node(scheme_node_data const &node)
    -> string_writer<std::monostate> {
    std::string dot =
        std::format("  n{} [label=\"{}\\n[{}]\"];\n", node.node_id,
                    node.sender_algo, node.scheme_context);

    for (int child_id : node.child_ids) {
        dot += std::format("  n{} -> n{};\n", node.node_id, child_id);
    }

    return tell(std::move(dot));
}

} // namespace smd::smdscheme::sender

#endif
