// src/smd/cl/sender/sender_v.hpp                                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_SENDER_SENDER_V_HPP
#define SRC_SMD_CL_SENDER_SENDER_V_HPP

#include <beman/execution26/execution.hpp>

/// Thin vocabulary aliases for Beman Execution26, for the `cl` sender backend
/// (step R7).
///
/// Explicit `using` declarations rather than `using namespace`, so the full
/// Execution26 ADL surface does not leak into `smd::cl::sender` — the same
/// discipline `smd::smdlisp::sender_v` already established for the pivot.
/// This is a fresh copy rather than a reuse of either pivot vocabulary
/// header: both `smd::smdscheme` and `smd::smdlisp` are read-only trees
/// (dead-ended and behavioural-oracle respectively), so nothing under
/// `src/smd/cl/**` may depend on either.
namespace smd::cl::sender::sender_v {

// 2cae776b-38af-41b9-b878-2cca67ac1295
using beman::execution26::just;
using beman::execution26::just_error;
using beman::execution26::just_stopped;
using beman::execution26::sync_wait;
using beman::execution26::then;
using beman::execution26::when_all;

using beman::execution26::completion_signatures;
using beman::execution26::connect;
using beman::execution26::connect_result_t;
using beman::execution26::operation_state_tag;
using beman::execution26::receiver_tag;
using beman::execution26::sender_tag;
using beman::execution26::set_error;
using beman::execution26::set_error_t;
using beman::execution26::set_stopped;
using beman::execution26::set_stopped_t;
using beman::execution26::set_value;
using beman::execution26::set_value_t;
using beman::execution26::start;
// 2cae776b-38af-41b9-b878-2cca67ac1295 end

} // namespace smd::cl::sender::sender_v

#endif
