// src/smd/smdscheme/sender/scheme_node_data.hpp               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_SCHEME_NODE_DATA_HPP
#define SRC_SMD_SMDSCHEME_SENDER_SCHEME_NODE_DATA_HPP

#include <smd/smdscheme/foundation/static_vector.hpp>

#include <string_view>

namespace smd::smdscheme::sender {

struct scheme_node_data {
    int node_id{};
    std::string_view sender_algo{};     // "just", "then", "when_all"
    std::string_view scheme_context{};  // "Atom: 5", "Apply: +", etc.
    foundation::static_vector<int, 8> child_ids{};

    friend constexpr auto operator==(scheme_node_data const &,
                                     scheme_node_data const &)
        -> bool = default;
};

} // namespace smd::smdscheme::sender

#endif
