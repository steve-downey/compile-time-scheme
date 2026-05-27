// src/smd/smdscheme/sender/build_scheme_tree.hpp               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_BUILD_SCHEME_TREE_HPP
#define SRC_SMD_SMDSCHEME_SENDER_BUILD_SCHEME_TREE_HPP

#include <smd/smdscheme/sender/scheme_node_data.hpp>
#include <smd/smdscheme/sender/scheme_tree.hpp>

#include <meta>
#include <string_view>

namespace smd::smdscheme::sender {

// Classify a sender type by inspecting its display string.
// Beman Execution senders are basic_sender<Tag, Data, Children...>.
// display_string_of gives the full type name; we search for the tag name.
consteval auto classify_sender(std::meta::info type) -> std::string_view {
    std::string_view name = std::meta::display_string_of(type);
    if (name.find("just_t") != std::string_view::npos) return "just";
    return "unknown";
}

// Build a scheme_tree by reflecting on a Sender type.
// For step 3, only just senders are handled.
template <int MaxNodes = 64>
consteval auto build_scheme_tree(std::meta::info sender_type,
                                 scheme_tree<MaxNodes>& tree) -> int {
    scheme_node_data node{};
    node.sender_algo = classify_sender(sender_type);
    return tree.allocate(node);
}

} // namespace smd::smdscheme::sender

#endif
