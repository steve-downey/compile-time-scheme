// src/smd/schemepoc/elaborator.hpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SCHEMEPOC_ELABORATOR_HPP
#define SRC_SMD_SCHEMEPOC_ELABORATOR_HPP

#include <smd/schemepoc/datum_tree.hpp>
#include <smd/schemepoc/result.hpp>
#include <smd/schemepoc/static_vector.hpp>

#include <string_view>
#include <variant>

namespace smd::schemepoc {

struct core_integer {
    int value;
};

struct core_boolean {
    bool value;
};

struct core_symbol {
    std::string_view name;
};

struct core_quote {
    std::variant<int, bool, std::string_view> atom;
};

struct core_if {
    node_id condition;
    node_id consequent;
    node_id alternative;
};

template <int MaxList>
struct core_lambda {
    static_vector<std::string_view, MaxList> params;
    node_id body;
};

template <int MaxList>
struct core_application {
    node_id func;
    static_vector<node_id, MaxList> args;
};

struct core_define {
    std::string_view name;
    node_id value;
};

template <int MaxList>
using core_node =
    std::variant<core_integer, core_boolean, core_symbol, core_quote, core_if,
                 core_lambda<MaxList>, core_application<MaxList>, core_define>;

template <int MaxNodes, int MaxList>
class core_tree {
  public:
    constexpr auto add(core_node<MaxList> value) -> node_id {
        node_id id = nodes_.size();
        nodes_.push_back(std::move(value));
        return id;
    }

    [[nodiscard]] constexpr auto get(node_id id) const
        -> core_node<MaxList> const & {
        return nodes_[id];
    }

    [[nodiscard]] constexpr auto size() const -> int { return nodes_.size(); }

  private:
    static_vector<core_node<MaxList>, MaxNodes> nodes_{};
};

namespace detail {

template <int MaxNodes, int MaxList>
constexpr auto elaborate_quote(node_id datum_id,
                               datum_tree<MaxNodes, MaxList> const &dt)
    -> result<std::variant<int, bool, std::string_view>> {
    auto const &node = dt.get(datum_id);
    if (std::holds_alternative<datum_integer>(node)) {
        return std::variant<int, bool, std::string_view>{
            std::get<datum_integer>(node).value};
    }
    if (std::holds_alternative<datum_boolean>(node)) {
        return std::variant<int, bool, std::string_view>{
            std::get<datum_boolean>(node).value};
    }
    if (std::holds_alternative<datum_symbol>(node)) {
        return std::variant<int, bool, std::string_view>{
            std::get<datum_symbol>(node).name};
    }
    return parse_error{{}, "quote: lists/nested quotes not yet supported"};
}

template <int MaxNodes, int MaxList>
constexpr auto elaborate_node(node_id datum_id,
                              datum_tree<MaxNodes, MaxList> const &dt,
                              core_tree<MaxNodes, MaxList> &ct)
    -> result<node_id>;

template <int MaxNodes, int MaxList>
constexpr auto elaborate_list(datum_list<MaxList> const &lst,
                              datum_tree<MaxNodes, MaxList> const &dt,
                              core_tree<MaxNodes, MaxList> &ct)
    -> result<node_id> {
    if (lst.elements.empty())
        return parse_error{{}, "empty application"};

    auto const &first = dt.get(lst.elements[0]);

    if (std::holds_alternative<datum_symbol>(first)) {
        std::string_view sym = std::get<datum_symbol>(first).name;

        if (sym == "if") {
            if (lst.elements.size() != 4)
                return parse_error{{}, "if arity"};
            auto cond_r = elaborate_node(lst.elements[1], dt, ct);
            if (!cond_r.has_value())
                return cond_r.error();
            auto cons_r = elaborate_node(lst.elements[2], dt, ct);
            if (!cons_r.has_value())
                return cons_r.error();
            auto alt_r = elaborate_node(lst.elements[3], dt, ct);
            if (!alt_r.has_value())
                return alt_r.error();
            node_id id =
                ct.add(core_if{cond_r.value(), cons_r.value(), alt_r.value()});
            return id;
        }

        if (sym == "lambda") {
            if (lst.elements.size() != 3)
                return parse_error{{}, "unknown special form shape"};
            auto const &params_datum = dt.get(lst.elements[1]);
            if (!std::holds_alternative<datum_list<MaxList>>(params_datum))
                return parse_error{{}, "lambda malformed parameter list"};
            auto const &param_list =
                std::get<datum_list<MaxList>>(params_datum);

            static_vector<std::string_view, MaxList> params{};
            for (int i = 0; i < param_list.elements.size(); ++i) {
                auto const &p = dt.get(param_list.elements[i]);
                if (!std::holds_alternative<datum_symbol>(p))
                    return parse_error{{}, "lambda malformed parameter list"};
                auto p_name = std::get<datum_symbol>(p).name;
                for (int j = 0; j < params.size(); ++j) {
                    if (params[j] == p_name)
                        return parse_error{{}, "duplicate parameter"};
                }
                params.push_back(p_name);
            }

            auto body_r = elaborate_node(lst.elements[2], dt, ct);
            if (!body_r.has_value())
                return body_r.error();

            node_id id = ct.add(core_lambda<MaxList>{params, body_r.value()});
            return id;
        }

        if (sym == "define") {
            if (lst.elements.size() != 3)
                return parse_error{{}, "unknown special form shape"};
            auto const &name_datum = dt.get(lst.elements[1]);
            if (!std::holds_alternative<datum_symbol>(name_datum))
                return parse_error{{}, "unknown special form shape"};
            std::string_view name = std::get<datum_symbol>(name_datum).name;

            auto value_r = elaborate_node(lst.elements[2], dt, ct);
            if (!value_r.has_value())
                return value_r.error();

            node_id id = ct.add(core_define{name, value_r.value()});
            return id;
        }

        if (sym == "quote") {
            if (lst.elements.size() != 2)
                return parse_error{{}, "unknown special form shape"};
            auto atom_r = elaborate_quote(lst.elements[1], dt);
            if (!atom_r.has_value())
                return atom_r.error();
            node_id id = ct.add(core_quote{atom_r.value()});
            return id;
        }
    }

    auto func_r = elaborate_node(lst.elements[0], dt, ct);
    if (!func_r.has_value())
        return func_r.error();

    static_vector<node_id, MaxList> args{};
    for (int i = 1; i < lst.elements.size(); ++i) {
        auto arg_r = elaborate_node(lst.elements[i], dt, ct);
        if (!arg_r.has_value())
            return arg_r.error();
        args.push_back(arg_r.value());
    }

    node_id id = ct.add(core_application<MaxList>{func_r.value(), args});
    return id;
}

template <int MaxNodes, int MaxList>
constexpr auto elaborate_node(node_id datum_id,
                              datum_tree<MaxNodes, MaxList> const &dt,
                              core_tree<MaxNodes, MaxList> &ct)
    -> result<node_id> {
    auto const &node = dt.get(datum_id);

    if (std::holds_alternative<datum_integer>(node)) {
        node_id id = ct.add(core_integer{std::get<datum_integer>(node).value});
        return id;
    }
    if (std::holds_alternative<datum_boolean>(node)) {
        node_id id = ct.add(core_boolean{std::get<datum_boolean>(node).value});
        return id;
    }
    if (std::holds_alternative<datum_symbol>(node)) {
        node_id id = ct.add(core_symbol{std::get<datum_symbol>(node).name});
        return id;
    }
    if (std::holds_alternative<datum_quote>(node)) {
        auto atom_r = elaborate_quote(std::get<datum_quote>(node).quoted, dt);
        if (!atom_r.has_value())
            return atom_r.error();
        node_id id = ct.add(core_quote{atom_r.value()});
        return id;
    }
    if (std::holds_alternative<datum_list<MaxList>>(node)) {
        return elaborate_list(std::get<datum_list<MaxList>>(node), dt, ct);
    }

    return parse_error{{}, "unknown datum node kind"};
}

} // namespace detail

template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto elaborate(datum_tree<MaxNodes, MaxList> const &dt)
    -> result<core_tree<MaxNodes, MaxList>> {
    if (dt.size() == 0)
        return parse_error{{}, "empty datum tree"};

    core_tree<MaxNodes, MaxList> ct{};
    node_id root = dt.size() - 1;
    auto r = detail::elaborate_node(root, dt, ct);
    if (!r.has_value())
        return r.error();
    return ct;
}

} // namespace smd::schemepoc

#endif
