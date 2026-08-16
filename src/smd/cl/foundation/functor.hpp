// src/smd/cl/foundation/functor.hpp                                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Forwarding shim (step R8): smd::cl::foundation::functor now names
// smd::kit::foundation::functor, extracted to the shared constexpr
// substrate once cl became a second real client of the same generic
// typeclass framework smd::smdscheme and smd::forth apply to parser<T>.
// The include path and the names below are unchanged for every existing
// caller in this tree; only the definition moved.
#ifndef SRC_SMD_CL_FOUNDATION_FUNCTOR_HPP
#define SRC_SMD_CL_FOUNDATION_FUNCTOR_HPP

#include <smd/kit/foundation/functor.hpp>

namespace smd::cl::foundation {

using smd::kit::foundation::fmap;
using smd::kit::foundation::fmap_fn;
using smd::kit::foundation::functor;
using smd::kit::foundation::functor_typeclass;

} // namespace smd::cl::foundation

#endif
