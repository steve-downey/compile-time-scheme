// src/smd/cl/foundation/applicative.hpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Forwarding shim (step R8): smd::cl::foundation::applicative now names
// smd::kit::foundation::applicative, extracted to the shared constexpr
// substrate once cl became a second real client of the same generic
// typeclass framework smd::smdscheme and smd::forth apply to parser<T>.
// The include path and the names below are unchanged for every existing
// caller in this tree; only the definition moved.
#ifndef SRC_SMD_CL_FOUNDATION_APPLICATIVE_HPP
#define SRC_SMD_CL_FOUNDATION_APPLICATIVE_HPP

#include <smd/kit/foundation/applicative.hpp>

namespace smd::cl::foundation {

using smd::kit::foundation::applicative;
using smd::kit::foundation::applicative_typeclass;
using smd::kit::foundation::invoke;
using smd::kit::foundation::invoke_fn;

} // namespace smd::cl::foundation

#endif
