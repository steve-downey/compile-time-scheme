// src/smd/cl/reader/detail/read_node_fwd.hpp                      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_READER_DETAIL_READ_NODE_FWD_HPP
#define SRC_SMD_CL_READER_DETAIL_READ_NODE_FWD_HPP

#include <smd/cl/foundation/result.hpp>
#include <smd/cl/reader/cursor.hpp>

namespace smd::cl::reader::detail {

// 8a2232b6-6436-4943-b35b-69e6df6faaa2
/// Forward declaration only: @ref read_wrapped, @ref read_delimited and
/// @ref read_sharpsign all call @c read_node, and @c read_node calls all of
/// them. Everything here is a template, so a declaration suffices at each
/// call site; the definition (@c detail/node.hpp) need only be visible at
/// the point of instantiation, which the @c read.hpp umbrella guarantees.
template <class Ctx>
[[nodiscard]] constexpr auto read_node(cursor cur, Ctx &ctx)
    -> foundation::result<parse_state<int>>;
// 8a2232b6-6436-4943-b35b-69e6df6faaa2 end

} // namespace smd::cl::reader::detail

#endif
