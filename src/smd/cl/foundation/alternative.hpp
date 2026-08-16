// src/smd/cl/foundation/alternative.hpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Forwarding shim (step R8): smd::cl::foundation::alternative now names
// smd::kit::foundation::alternative, extracted to the shared constexpr
// substrate once cl became a second real client of the same generic
// typeclass framework smd::smdscheme and smd::forth apply to parser<T>.
// The include path and the names below are unchanged for every existing
// caller in this tree; only the definition moved.
#ifndef SRC_SMD_CL_FOUNDATION_ALTERNATIVE_HPP
#define SRC_SMD_CL_FOUNDATION_ALTERNATIVE_HPP

#include <smd/kit/foundation/alternative.hpp>

namespace smd::cl::foundation {

using smd::kit::foundation::alt;
using smd::kit::foundation::alt_fn;
using smd::kit::foundation::alternative;
using smd::kit::foundation::alternative_typeclass;
using smd::kit::foundation::empty;
using smd::kit::foundation::empty_fn;

} // namespace smd::cl::foundation

#endif
