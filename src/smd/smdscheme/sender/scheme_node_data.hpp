// src/smd/smdscheme/sender/scheme_node_data.hpp               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_SCHEME_NODE_DATA_HPP
#define SRC_SMD_SMDSCHEME_SENDER_SCHEME_NODE_DATA_HPP

#include <smd/smdscheme/foundation/static_vector.hpp>

#include <string_view>

namespace smd::smdscheme::sender {

// c1d2e3f4-a5b6-4c7d-8e9f-0a1b2c3d4e5f
/// Data describing a single node in a reflected sender execution graph.
///
/// Populated at compile time by @ref build_scheme_tree via P2996 reflection.
/// Each node corresponds to one sender in a Beman Execution26 composition
/// (e.g. @c just, @c then, @c when_all).
struct scheme_node_data {
    int node_id{}; ///< Index of this node in its @ref scheme_tree.
    std::string_view
        sender_algo{}; ///< Sender kind: "just", "then", or "when_all".
    std::string_view
        scheme_context{}; ///< Optional annotation (e.g. "Atom: 5", "Apply: +").
    foundation::static_vector<int, 8>
        child_ids{}; ///< Indices of child sender nodes.

    friend constexpr auto operator==(scheme_node_data const &,
                                     scheme_node_data const &)
        -> bool = default;
};
// c1d2e3f4-a5b6-4c7d-8e9f-0a1b2c3d4e5f end

} // namespace smd::smdscheme::sender

#endif
