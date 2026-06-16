// src/smd/smdscheme/sender/build_scheme_tree.hpp               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_BUILD_SCHEME_TREE_HPP
#define SRC_SMD_SMDSCHEME_SENDER_BUILD_SCHEME_TREE_HPP

#include <smd/smdscheme/sender/scheme_node_data.hpp>
#include <smd/smdscheme/sender/scheme_tree.hpp>

#include <meta>
#include <string_view>

namespace smd::smdscheme::sender {

// ad3fee60-a0ea-4464-966c-bae3a964db79
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
// ad3fee60-a0ea-4464-966c-bae3a964db79 end

} // namespace smd::smdscheme::sender

#endif
