// src/smd/schemepoc/datum_tree.hpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SCHEMEPOC_DATUM_TREE_HPP
#define SRC_SMD_SCHEMEPOC_DATUM_TREE_HPP

#include <smd/schemepoc/static_vector.hpp>

#include <string_view>
#include <variant>

namespace smd::schemepoc {

using node_id = int;
inline constexpr node_id invalid_node = -1;

struct datum_integer {
    int value;
};

struct datum_symbol {
    std::string_view name;
};

struct datum_boolean {
    bool value;
};

template <int MaxList>
struct datum_list {
    static_vector<node_id, MaxList> elements{};
};

struct datum_quote {
    node_id quoted{invalid_node};
};

template <int MaxList>
using datum_node = std::variant<datum_integer, datum_symbol, datum_boolean,
                                datum_list<MaxList>, datum_quote>;

template <int MaxNodes, int MaxList>
class datum_tree {
  public:
    constexpr auto add(datum_node<MaxList> value) -> node_id {
        node_id id = nodes_.size();
        nodes_.push_back(std::move(value));
        return id;
    }

    [[nodiscard]] constexpr auto get(node_id id) const
        -> datum_node<MaxList> const & {
        return nodes_[id];
    }

    [[nodiscard]] constexpr auto size() const -> int { return nodes_.size(); }

  private:
    static_vector<datum_node<MaxList>, MaxNodes> nodes_{};
};

} // namespace smd::schemepoc

#endif
