// src/smd/cl/foundation/static_vector_instances.hpp                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Forwarding shim (step R8, decision 2 corrected): smd::cl::foundation's
// static_vector typeclass instances now name smd::kit::foundation's.
// static_vector's Functor typeclass was already kit-owned; once Foldable
// and Traversable joined it (decision 2, corrected —
// docs/compiler_architecture.org § "The kit: a real second target with
// one real client"), this adapter's whole reason for staying in cl (that
// it depended on cl-only typeclasses) no longer applied, so it moved with
// them. The include path and the names below are unchanged for every
// existing caller in this tree; only the definition moved.
#ifndef SRC_SMD_CL_FOUNDATION_STATIC_VECTOR_INSTANCES_HPP
#define SRC_SMD_CL_FOUNDATION_STATIC_VECTOR_INSTANCES_HPP

#include <smd/kit/foundation/static_vector_instances.hpp>

// The original static_vector_instances.hpp included applicative.hpp,
// foldable.hpp, functor.hpp, static_vector.hpp, and traversable.hpp; kept
// here so smd::cl::foundation still transitively names those exactly as it
// did before this file became a shim.
#include <smd/cl/foundation/applicative.hpp>
#include <smd/cl/foundation/foldable.hpp>
#include <smd/cl/foundation/functor.hpp>
#include <smd/cl/foundation/static_vector.hpp>
#include <smd/cl/foundation/traversable.hpp>

namespace smd::cl::foundation {

using smd::kit::foundation::static_vector_foldable_impl;
using smd::kit::foundation::static_vector_foldable_map;
using smd::kit::foundation::static_vector_functor_impl;
using smd::kit::foundation::static_vector_functor_map;
using smd::kit::foundation::static_vector_traversable_impl;
using smd::kit::foundation::static_vector_traversable_map;

} // namespace smd::cl::foundation

#endif
