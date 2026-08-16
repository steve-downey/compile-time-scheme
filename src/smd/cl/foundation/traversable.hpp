// src/smd/cl/foundation/traversable.hpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Forwarding shim (step R8, decision 2 corrected): smd::cl::foundation's
// Traversable vocabulary now names smd::kit::foundation's. The initial R8
// cut left this in cl on the grounds that no sibling front end had a use
// for it yet; that was overturned as measuring a sibling project's
// backlog rather than this code's shape (docs/compiler_architecture.org
// § "The kit: a real second target with one real client"). The include
// path and the names below are unchanged for every existing caller in
// this tree; only the definition moved.
#ifndef SRC_SMD_CL_FOUNDATION_TRAVERSABLE_HPP
#define SRC_SMD_CL_FOUNDATION_TRAVERSABLE_HPP

#include <smd/kit/foundation/traversable.hpp>

namespace smd::cl::foundation {

using smd::kit::foundation::sequence;
using smd::kit::foundation::sequence_fn;
using smd::kit::foundation::traversable;
using smd::kit::foundation::traversable_typeclass;
using smd::kit::foundation::traverse;
using smd::kit::foundation::traverse_fn;

} // namespace smd::cl::foundation

#endif
