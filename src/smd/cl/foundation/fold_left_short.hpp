// src/smd/cl/foundation/fold_left_short.hpp                         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Forwarding shim (step R8, decision 2 corrected): smd::cl::foundation's
// fold_left_short now names smd::kit::foundation's. The initial R8 cut
// left this in cl on the grounds that no sibling front end had a use for
// it yet; that was overturned as measuring a sibling project's backlog
// rather than this code's shape (docs/compiler_architecture.org § "The
// kit: a real second target with one real client"). The include path and
// the names below are unchanged for every existing caller in this tree;
// only the definition moved.
#ifndef SRC_SMD_CL_FOUNDATION_FOLD_LEFT_SHORT_HPP
#define SRC_SMD_CL_FOUNDATION_FOLD_LEFT_SHORT_HPP

#include <smd/kit/foundation/fold_left_short.hpp>

namespace smd::cl::foundation {

using smd::kit::foundation::fold_left_short;
using smd::kit::foundation::short_circuit_effect;

} // namespace smd::cl::foundation

#endif
