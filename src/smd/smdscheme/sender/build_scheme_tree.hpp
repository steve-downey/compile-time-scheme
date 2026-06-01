// src/smd/smdscheme/sender/build_scheme_tree.hpp               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_BUILD_SCHEME_TREE_HPP
#define SRC_SMD_SMDSCHEME_SENDER_BUILD_SCHEME_TREE_HPP

#include <smd/smdscheme/sender/scheme_node_data.hpp>
#include <smd/smdscheme/sender/scheme_tree.hpp>

#include <algorithm>
#include <meta>
#include <string_view>

namespace smd::smdscheme::sender {

// ad3fee60-a0ea-4464-966c-bae3a964db79
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
// ad3fee60-a0ea-4464-966c-bae3a964db79 end

} // namespace smd::smdscheme::sender

#endif
