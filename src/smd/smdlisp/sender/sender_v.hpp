// src/smd/smdlisp/sender/sender_v.hpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_SENDER_SENDER_V_HPP
#define SRC_SMD_SMDLISP_SENDER_SENDER_V_HPP

#include <beman/execution26/execution.hpp>

/// Thin vocabulary aliases for Beman Execution26, for the `smdlisp` sender
/// backend (step L21).
///
/// Deliberately the same shape as `smd::smdscheme::sender_v` -- importing via
/// @c using rather than @c using @c namespace, so the declarations stay
/// explicit and the full Execution26 ADL surface does not leak into
/// downstream code.  It is a separate namespace rather than a reuse of the
/// Scheme one for one reason: the Scheme backend never needed the *error* and
/// *stopped* channels, so `smdscheme/sender/sender_v.hpp` exports only
/// @c just, @c let_value, @c sync_wait, @c then and @c when_all, and
/// `smd::smdscheme` is frozen for semantic changes (decision D1) so it cannot
/// grow the rest.  The `smdlisp` backend is *about* the other two channels
/// (decision D5), so it needs @c just_error, @c just_stopped, the completion
/// tags, and the raw @c connect / @c start pair that lets a driver observe a
/// completion instead of having @c sync_wait translate it into an exception.
namespace smd::smdlisp::sender_v {

// 1a1f0d02-d367-499c-929f-6152f4e9e6aa
// The Scheme backend's vocabulary, unchanged.
using beman::execution26::just;
using beman::execution26::let_value;
using beman::execution26::sync_wait;
using beman::execution26::then;
using beman::execution26::when_all;

// The two channels the Scheme backend never used, and the algorithms that
// observe them.  `smdlisp` maps every nonlocal exit onto these (D5).
using beman::execution26::just_error;
using beman::execution26::just_stopped;
using beman::execution26::let_error;
using beman::execution26::let_stopped;
using beman::execution26::upon_error;
using beman::execution26::upon_stopped;

// The concept/tag vocabulary a hand-written sender, receiver or operation
// state has to name to satisfy Execution26's concepts.
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
// 1a1f0d02-d367-499c-929f-6152f4e9e6aa end

} // namespace smd::smdlisp::sender_v

#endif
