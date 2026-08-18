// src/smd/cl/foundation/result.hpp                                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Forwarding shim (step R8): smd::cl::foundation::result now names
// smd::kit::foundation::result, extracted to the shared constexpr
// substrate. result_instances.hpp (the typeclass adapter for this type)
// travelled with it, along with foldable.hpp and traversable.hpp: both the
// datatype and every typeclass the adapter instantiates are kit-owned, so
// the adapter is kit material too.
//
// and_then travelled with it too and has since moved again, into
// result_instances.hpp, where it is a spelling of the Monad instance's bind
// rather than a second copy of it. A caller that says and_then wants that
// header now; this one no longer names it.
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

using smd::kit::foundation::result;

} // namespace smd::cl::foundation
// 80e2bedd-55e3-42e1-b835-55384ff35836 end

#endif
