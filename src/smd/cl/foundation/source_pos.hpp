// src/smd/cl/foundation/source_pos.hpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Forwarding shim (step R8): smd::cl::foundation::source_pos now names
// smd::kit::foundation::source_pos, extracted to the shared constexpr
// substrate as one of the reviewed-union files that was already
// byte-identical across smdscheme, forth and cl but for guard, namespace
// and comment. The include path and the name below are unchanged for every
// existing caller in this tree; only the definition moved.
#ifndef SRC_SMD_CL_FOUNDATION_SOURCE_POS_HPP
#define SRC_SMD_CL_FOUNDATION_SOURCE_POS_HPP

#include <smd/kit/foundation/source_pos.hpp>

namespace smd::cl::foundation {

using smd::kit::foundation::source_pos;

} // namespace smd::cl::foundation

#endif
