// src/smd/cl/foundation/parse_error.hpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Forwarding shim (step R8): smd::cl::foundation::parse_error now names
// smd::kit::foundation::parse_error, extracted to the shared constexpr
// substrate. The merged equality (scheme's same-pointer fast path, forth's
// explicit null handling, cl's std::string_view comparison) travelled with
// the extraction as a strict quality improvement over either older copy.
// The include path and the name below are unchanged for every existing
// caller in this tree; only the definition moved.
#ifndef SRC_SMD_CL_FOUNDATION_PARSE_ERROR_HPP
#define SRC_SMD_CL_FOUNDATION_PARSE_ERROR_HPP

#include <smd/kit/foundation/parse_error.hpp>

// The original parse_error.hpp included source_pos.hpp; kept here so
// smd::cl::foundation still transitively names source_pos exactly as it
// did before this file became a shim.
#include <smd/cl/foundation/source_pos.hpp>

namespace smd::cl::foundation {

using smd::kit::foundation::parse_error;

} // namespace smd::cl::foundation

#endif
