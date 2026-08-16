// src/smd/cl/foundation/source_span.hpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Forwarding shim (step R8): smd::cl::foundation::source_span now names
// smd::kit::foundation::source_span, extracted to the shared constexpr
// substrate as one of the reviewed-union files that was already identical
// across smdscheme, forth and cl but for guard, namespace and comment. The
// include path and the name below are unchanged for every existing caller
// in this tree; only the definition moved.
#ifndef SRC_SMD_CL_FOUNDATION_SOURCE_SPAN_HPP
#define SRC_SMD_CL_FOUNDATION_SOURCE_SPAN_HPP

#include <smd/kit/foundation/source_span.hpp>

// The original source_span.hpp included source_pos.hpp; kept here so
// smd::cl::foundation still transitively names source_pos exactly as it
// did before this file became a shim.
#include <smd/cl/foundation/source_pos.hpp>

namespace smd::cl::foundation {

using smd::kit::foundation::source_span;

} // namespace smd::cl::foundation

#endif
