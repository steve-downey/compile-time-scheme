// src/smd/cl/foundation/monoid.hpp                                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Forwarding shim (step R8, decision 2 corrected): smd::cl::foundation's
// monoid vocabulary now names smd::kit::foundation's. The initial R8 cut
// left this in cl on the grounds that no sibling front end had a use for
// it yet; that was overturned as measuring a sibling project's backlog
// rather than this code's shape (docs/compiler_architecture.org § "The
// kit: a real second target with one real client"). The include path and
// the names below are unchanged for every existing caller in this tree;
// only the definition moved.
#ifndef SRC_SMD_CL_FOUNDATION_MONOID_HPP
#define SRC_SMD_CL_FOUNDATION_MONOID_HPP

#include <smd/kit/foundation/monoid.hpp>

namespace smd::cl::foundation {

using smd::kit::foundation::all_monoid;
using smd::kit::foundation::all_monoid_t;
using smd::kit::foundation::any_monoid;
using smd::kit::foundation::any_monoid_t;
using smd::kit::foundation::monoid_for;
using smd::kit::foundation::sum_monoid;
using smd::kit::foundation::sum_monoid_t;
using smd::kit::foundation::unit;
using smd::kit::foundation::unit_monoid;
using smd::kit::foundation::unit_monoid_t;

} // namespace smd::cl::foundation

#endif
