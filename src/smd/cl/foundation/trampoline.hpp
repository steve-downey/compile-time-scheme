// src/smd/cl/foundation/trampoline.hpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Forwarding shim (step R8, decision 2 corrected): smd::cl::foundation's
// trampoline now names smd::kit::foundation's. The initial R8 cut left
// this in cl on the grounds that no sibling front end had a small-step
// machine to drive; that was overturned as measuring a sibling project's
// backlog rather than this code's shape (docs/compiler_architecture.org
// § "The kit: a real second target with one real client"). The include
// path and the names below are unchanged for every existing caller in
// this tree; only the definition moved.
#ifndef SRC_SMD_CL_FOUNDATION_TRAMPOLINE_HPP
#define SRC_SMD_CL_FOUNDATION_TRAMPOLINE_HPP

#include <smd/kit/foundation/trampoline.hpp>

// The original trampoline.hpp included parse_error.hpp and result.hpp;
// kept here so smd::cl::foundation still transitively names them exactly
// as it did before this file became a shim.
#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/foundation/result.hpp>

namespace smd::cl::foundation {

using smd::kit::foundation::machine_step;
using smd::kit::foundation::step_status;
using smd::kit::foundation::trampoline;

} // namespace smd::cl::foundation

#endif
