// src/smd/cl/foundation/identity.hpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Forwarding shim (step R8, decision 2 corrected): smd::cl::foundation's
// identity now names smd::kit::foundation's. The initial R8 cut left this
// in cl on the grounds that no sibling front end had a use for the
// identity applicative; that was overturned as measuring a sibling
// project's backlog rather than this code's shape
// (docs/compiler_architecture.org § "The kit: a real second target with
// one real client"). Moving the type itself (not just the typeclasses it
// instantiates) also let the kit's copy drop the namespace-reopening
// workaround this file used to carry. The include path and the names
// below are unchanged for every existing caller in this tree; only the
// definition moved.
#ifndef SRC_SMD_CL_FOUNDATION_IDENTITY_HPP
#define SRC_SMD_CL_FOUNDATION_IDENTITY_HPP

#include <smd/kit/foundation/identity.hpp>

namespace smd::cl::foundation {

using smd::kit::foundation::identity;
using smd::kit::foundation::identity_applicative_impl;
using smd::kit::foundation::identity_applicative_map;
using smd::kit::foundation::identity_functor_impl;
using smd::kit::foundation::identity_functor_map;

} // namespace smd::cl::foundation

#endif
