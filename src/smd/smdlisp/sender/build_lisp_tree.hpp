// src/smd/smdlisp/sender/build_lisp_tree.hpp                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_SENDER_BUILD_LISP_TREE_HPP
#define SRC_SMD_SMDLISP_SENDER_BUILD_LISP_TREE_HPP

#include <smd/smdlisp/sender/lisp_plan_tree.hpp>

#include <meta>
#include <string_view>

namespace smd::smdlisp::sender {

// 45796b9c-5930-453b-82b4-1b609ae2c8f8
/// Classifies a sender type by reflecting on the head of its type.
///
/// A port of @ref smd::smdscheme::sender::classify_sender with two
/// extensions, both forced by what a `smdlisp` graph actually contains.
///
/// **First: this backend's own senders are not `basic_sender`s.**
/// @ref unwind_protect_sender and @ref defer_outcome_sender are hand-written,
/// so the tag-based path cannot see them at all and the Scheme classifier
/// would label the two most interesting nodes in any `smdlisp` graph
/// @c "unknown".  They are matched by template identity first.
///
/// **Second: the completion channel lives in the tag's own template
/// argument, not in the tag's name.**  This is the part that is easy to get
/// wrong, and the Scheme classifier has it wrong -- harmlessly, because the
/// Scheme backend only ever built value-channel senders.  In this vendored
/// Beman revision the three families are spelled
///
/// @code
///   just_t<Completion>  then_t<Completion>  let_t<Completion>
/// @endcode
///
/// with `just_error_t` = `just_t<set_error_t>`, `upon_stopped_t` =
/// `then_t<set_stopped_t>`, `let_value_t` = `let_t<set_value_t>`, and so on.
/// So @c identifier_of(template_of(tag)) returns @c "just_t" for all three of
/// @c just, @c just_error and @c just_stopped; distinguishing them needs the
/// tag's argument.  A `smdlisp` graph is largely *about* the non-value
/// channels, so collapsing them would defeat the point of drawing it.
///
/// @c when_all_t is a plain class with no template arguments, and is matched
/// by its own identifier.
///
/// @param type A P2996 reflection of the sender type (dealiased by the
///             caller).
/// @return The node label, or @c "unknown".
consteval auto classify_lisp_sender(std::meta::info type) -> std::string_view {
    // This backend's own senders, matched by template rather than by tag.
    if (std::meta::has_template_arguments(type)) {
        auto const tmpl =
            std::meta::identifier_of(std::meta::template_of(type));
        if (tmpl == "unwind_protect_sender")
            return "unwind_protect";
        if (tmpl == "defer_outcome_sender")
            return "defer";
    }

    auto args = std::meta::template_arguments_of(type);
    if (args.empty())
        return "unknown";
    auto tag = std::meta::dealias(args[0]);

    // A tag with no template arguments names its algorithm directly.
    if (!std::meta::has_template_arguments(tag)) {
        if (std::meta::identifier_of(tag) == "when_all_t")
            return "when_all";
        return "unknown";
    }

    auto const family = std::meta::identifier_of(std::meta::template_of(tag));
    auto const tag_args = std::meta::template_arguments_of(tag);
    if (tag_args.empty())
        return "unknown";
    auto const completion =
        std::meta::identifier_of(std::meta::dealias(tag_args[0]));

    if (family == "just_t") {
        if (completion == "set_error_t")
            return "just_error";
        if (completion == "set_stopped_t")
            return "just_stopped";
        return "just";
    }
    if (family == "then_t") {
        if (completion == "set_error_t")
            return "upon_error";
        if (completion == "set_stopped_t")
            return "upon_stopped";
        return "then";
    }
    if (family == "let_t") {
        if (completion == "set_error_t")
            return "let_error";
        if (completion == "set_stopped_t")
            return "let_stopped";
        return "let_value";
    }
    return "unknown";
}

/// Recursively builds a @ref lisp_plan_tree by reflecting on a sender type.
///
/// Two child-selection rules, because there are two shapes of sender in play:
///
///  - @c basic_sender<Tag,Data,Child...> -- args[0] is the tag, args[1] the
///    data, args[2...] the children, exactly as the Scheme port has it;
///  - @ref unwind_protect_sender<Core,Child,Cleanup> -- args[1] is the single
///    child (args[0] is the core type, args[2] the cleanup callable), and
///    @ref defer_outcome_sender is a leaf.
///
/// P2996 reflections preserve aliases, so a sender type obtained via
/// @c decltype must be dealiased before @c template_arguments_of will
/// recognise it as a specialisation.
///
/// @tparam MaxNodes    Maximum tree capacity (default 64).
/// @param  sender_type P2996 reflection of the sender type to traverse.
/// @param  tree        Output tree; nodes are appended in pre-order.
/// @return The integer id of the root node allocated for @p sender_type.
template <int MaxNodes = 64>
consteval auto build_lisp_tree(std::meta::info sender_type,
                               lisp_plan_tree<MaxNodes> &tree) -> int {
    auto type = std::meta::dealias(sender_type);
    lisp_node_data node{};
    node.sender_algo = classify_lisp_sender(type);
    auto id = tree.allocate(node);

    auto args = std::meta::template_arguments_of(type);
    if (node.sender_algo == "unwind_protect") {
        auto child_id = build_lisp_tree<MaxNodes>(args[1], tree);
        tree.get(id).child_ids.push_back(child_id);
        return id;
    }
    if (node.sender_algo == "defer")
        return id; // A leaf: its thunk is opaque to reflection.

    for (std::size_t i = 2; i < args.size(); ++i) {
        auto child_id = build_lisp_tree<MaxNodes>(args[i], tree);
        tree.get(id).child_ids.push_back(child_id);
    }
    return id;
}
// 45796b9c-5930-453b-82b4-1b609ae2c8f8 end

} // namespace smd::smdlisp::sender

#endif
