// src/smd/schemepoc/sender_adapter.hpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SCHEMEPOC_SENDER_ADAPTER_HPP
#define SRC_SMD_SCHEMEPOC_SENDER_ADAPTER_HPP

#include <beman/execution26/execution.hpp>

namespace smd::schemepoc::sender_v {
    using beman::execution26::just;
    using beman::execution26::sync_wait;
    using beman::execution26::then;
    using beman::execution26::when_all;
    using beman::execution26::let_value;
} // namespace smd::schemepoc::sender_v

#endif
