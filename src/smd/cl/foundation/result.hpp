// src/smd/cl/foundation/result.hpp                                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Forwarding shim (step R8): smd::cl::foundation::result now names
// smd::kit::foundation::result, extracted to the shared constexpr
// substrate. and_then travelled with it — it is generic over any result<T>,
// even though its doc comment still narrates the elaborator decision (D15)
// that motivated writing it. result_instances.hpp (the Functor/Applicative
// typeclass adapter for this type) stays in smd::cl::foundation: it also
// depends on foldable.hpp/traversable.hpp, which have exactly one
// implementation so far and have not earned kit membership by the same
// two-independent-copies test the nine files here already passed. The
// include path and the names below are unchanged for every existing caller
// in this tree; only the definition moved.
#ifndef SRC_SMD_CL_FOUNDATION_RESULT_HPP
#define SRC_SMD_CL_FOUNDATION_RESULT_HPP

// 80e2bedd-55e3-42e1-b835-55384ff35836
#include <smd/kit/foundation/result.hpp>

// The original result.hpp included parse_error.hpp; kept here (which in
// turn keeps source_pos.hpp) so smd::cl::foundation still transitively
// names parse_error and source_pos exactly as it did before this file
// became a shim.
#include <smd/cl/foundation/parse_error.hpp>

namespace smd::cl::foundation {

using smd::kit::foundation::and_then;
using smd::kit::foundation::result;

} // namespace smd::cl::foundation
// 80e2bedd-55e3-42e1-b835-55384ff35836 end

#endif
