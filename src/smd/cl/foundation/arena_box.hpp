// src/smd/cl/foundation/arena_box.hpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Forwarding shim (step R8): smd::cl::foundation::arena_box now names
// smd::kit::foundation::arena_box, extracted to the shared constexpr
// substrate as one of the reviewed-union files with essentially zero drift
// across smdscheme, forth and cl. The include path and the names below are
// unchanged for every existing caller in this tree; only the definition
// moved.
#ifndef SRC_SMD_CL_FOUNDATION_ARENA_BOX_HPP
#define SRC_SMD_CL_FOUNDATION_ARENA_BOX_HPP

#include <smd/kit/foundation/arena_box.hpp>

// The original arena_box.hpp included static_vector.hpp for tree_arena's
// data member; kept here (rather than relying on the kit header's own
// transitive include of the kit's static_vector) so smd::cl::foundation
// still transitively names static_vector exactly as it did before this
// file became a shim.
#include <smd/cl/foundation/static_vector.hpp>

namespace smd::cl::foundation {

using smd::kit::foundation::arena_box;
using smd::kit::foundation::make_arena_box;
using smd::kit::foundation::tree_arena;

} // namespace smd::cl::foundation

#endif
