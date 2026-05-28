// src/smd/smdscheme/sender/scheme_tree.hpp                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_SCHEME_TREE_HPP
#define SRC_SMD_SMDSCHEME_SENDER_SCHEME_TREE_HPP

#include <smd/smdscheme/foundation/static_vector.hpp>
#include <smd/smdscheme/sender/scheme_node_data.hpp>

namespace smd::smdscheme::sender {

template <int MaxNodes = 64>
struct scheme_tree {
    foundation::static_vector<scheme_node_data, MaxNodes> nodes{};
    int root_id{-1};

    constexpr auto allocate(scheme_node_data node) -> int {
        int id = static_cast<int>(nodes.size());
        node.node_id = id;
        nodes.push_back(node);
        return id;
    }

    constexpr auto get(int id) const -> scheme_node_data const& {
        return nodes[id];
    }

    constexpr auto get(int id) -> scheme_node_data& { return nodes[id]; }
};

} // namespace smd::smdscheme::sender

#endif
