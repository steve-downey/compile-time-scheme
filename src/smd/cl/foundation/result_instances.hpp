// src/smd/cl/foundation/result_instances.hpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Forwarding shim (step R8, decision 2 corrected): smd::cl::foundation's
// result typeclass instances now name smd::kit::foundation's. result's
// Functor and Applicative typeclasses were already kit-owned; once
// foldable/traversable/monoid/identity joined them (decision 2, corrected
// — docs/compiler_architecture.org § "The kit: a real second target with
// one real client"), this adapter's whole reason for staying in cl (that
// it depended on cl-only typeclasses) no longer applied, so it moved with
// them. The include path and the names below are unchanged for every
// existing caller in this tree; only the definition moved.
#ifndef SRC_SMD_CL_FOUNDATION_RESULT_INSTANCES_HPP
#define SRC_SMD_CL_FOUNDATION_RESULT_INSTANCES_HPP

#include <smd/kit/foundation/result_instances.hpp>

// The original result_instances.hpp included applicative.hpp, functor.hpp,
// and result.hpp; kept here so smd::cl::foundation still transitively
// names those exactly as it did before this file became a shim (some
// existing callers, e.g. src/smd/cl/core/ast.test.cpp, rely on this file
// alone to bring result/parse_error/fmap into scope).
#include <smd/cl/foundation/applicative.hpp>
#include <smd/cl/foundation/functor.hpp>
#include <smd/cl/foundation/monad.hpp>
#include <smd/cl/foundation/result.hpp>

namespace smd::cl::foundation {

using smd::kit::foundation::and_then;
using smd::kit::foundation::result_applicative_impl;
using smd::kit::foundation::result_applicative_map;
using smd::kit::foundation::result_functor_impl;
using smd::kit::foundation::result_functor_map;
using smd::kit::foundation::result_monad_impl;
using smd::kit::foundation::result_monad_map;

} // namespace smd::cl::foundation

#endif
