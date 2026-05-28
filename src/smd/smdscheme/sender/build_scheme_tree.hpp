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

// Classify a sender type by finding the first tag name in its display string.
// The outermost tag always appears first because it is basic_sender's first
// template argument.  Position-based min-find avoids false matches from
// nested child tag names that also appear in the full type display string.
// Confirmed working approach in GCC16 consteval contexts (step 3).
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

// Build a scheme_tree by reflecting on a Sender type.
//
// Beman Execution senders have type basic_sender<Tag, Data, Child...> which
// inherits from product_type<Tag, Data, Child...>.  Child senders appear
// as template arguments starting at index 2 (Tag=0, Data=1).
//
// Traversal strategy (no splice — GCC16 cannot use [:r:] on function params):
//   1. Classify the sender via classify_sender (display_string_of approach).
//   2. Use bases_of(sender_type) to get the product_type base class.
//   3. Use type_of(base) + template_arguments_of to get [Tag, Data, Child...].
//   4. Recurse for each Child (args[2+]).
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

} // namespace smd::smdscheme::sender

#endif
