// src/smd/cl/foundation/foldable.hpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Forwarding shim (step R8, decision 2 corrected): smd::cl::foundation's
// Foldable vocabulary now names smd::kit::foundation's. The initial R8 cut
// left this in cl on the grounds that no sibling front end had a use for
// it yet; that was overturned as measuring a sibling project's backlog
// rather than this code's shape (docs/compiler_architecture.org § "The
// kit: a real second target with one real client"). The include path and
// the names below are unchanged for every existing caller in this tree;
// only the definition moved.
#ifndef SRC_SMD_CL_FOUNDATION_FOLDABLE_HPP
#define SRC_SMD_CL_FOUNDATION_FOLDABLE_HPP

#include <smd/kit/foundation/foldable.hpp>

// The original foldable.hpp included monoid.hpp; kept here so
// smd::cl::foundation still transitively names the monoid vocabulary
// exactly as it did before this file became a shim.
#include <smd/cl/foundation/monoid.hpp>

namespace smd::cl::foundation {

using smd::kit::foundation::fold_left;
using smd::kit::foundation::fold_left_fn;
using smd::kit::foundation::fold_map;
using smd::kit::foundation::fold_map_fn;
using smd::kit::foundation::fold_right;
using smd::kit::foundation::fold_right_fn;
using smd::kit::foundation::foldable;
using smd::kit::foundation::foldable_typeclass;
using smd::kit::foundation::length;
using smd::kit::foundation::length_fn;

} // namespace smd::cl::foundation

#endif
