// src/smd/smdscheme/sender/sender_v.hpp                                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_SENDER_V_HPP
#define SRC_SMD_SMDSCHEME_SENDER_SENDER_V_HPP

#include <beman/execution26/execution.hpp>
#include <beman/task/task.hpp>

namespace smd::smdscheme::sender_v {
using beman::execution26::just;
using beman::execution26::let_value;
using beman::execution26::sync_wait;
using beman::execution26::then;
using beman::execution26::when_all;
template <typename T>
using task = beman::execution::task<T>;
} // namespace smd::smdscheme::sender_v

#endif
