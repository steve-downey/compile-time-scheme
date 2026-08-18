// src/smd/cl/foundation/monad.hpp                                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Forwarding shim: smd::cl::foundation's Monad typeclass names
// smd::kit::foundation's. Unlike the seventeen shims step R8 left behind,
// this one never had a cl-side definition to forward from — the typeclass
// was written in the kit directly, because result and its other typeclasses
// were already there. The shim exists so that cl code spells its includes
// the way the rest of cl does; it is the eighteenth candidate for
// docs/backlog/BL-0004-retire-the-cl-foundation-forwarding-shims.md.
#ifndef SRC_SMD_CL_FOUNDATION_MONAD_HPP
#define SRC_SMD_CL_FOUNDATION_MONAD_HPP

#include <smd/kit/foundation/monad.hpp>

namespace smd::cl::foundation {

using smd::kit::foundation::bind;
using smd::kit::foundation::bind_fn;
using smd::kit::foundation::join;
using smd::kit::foundation::join_fn;
using smd::kit::foundation::monad;
using smd::kit::foundation::monad_typeclass;

} // namespace smd::cl::foundation

#endif
