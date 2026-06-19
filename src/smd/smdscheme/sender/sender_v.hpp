// src/smd/smdscheme/sender/sender_v.hpp -*-C++-*- SPDX-License-Identifier:
// Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_SENDER_V_HPP
#define SRC_SMD_SMDSCHEME_SENDER_SENDER_V_HPP

#include <beman/execution26/execution.hpp>
#include <beman/task/task.hpp>

/// Thin vocabulary aliases for Beman Execution26 senders and the task coroutine
/// type.
///
/// Importing via @c using rather than @c using @c namespace keeps the
/// declarations explicit and avoids accidentally pulling in the full
/// Execution26 ADL surface into downstream code.
namespace smd::smdscheme::sender_v {
// 7a257e82-8d19-480a-974c-a088b7681250
using beman::execution26::just;
using beman::execution26::let_value;
using beman::execution26::sync_wait;
using beman::execution26::then;
using beman::execution26::when_all;
// 7a257e82-8d19-480a-974c-a088b7681250 end

/// Type alias for the Beman async task coroutine type.
/// @tparam T The value type produced by the coroutine.
template <typename T>
using task = beman::execution::task<T>;
} // namespace smd::smdscheme::sender_v

#endif
