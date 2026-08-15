// src/smd/cl/foundation/static_vector.hpp                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Forwarding shim (step R8): smd::cl::foundation::static_vector now names
// smd::kit::foundation::static_vector, extracted to the shared constexpr
// substrate. cl's filled()/append_range()/pop_back() additions travelled
// with the extraction as a strict superset over the older smdscheme/forth
// shape; D14 is unaffected (Capacity is still static_vector's own template
// parameter, never borrowed from a stored value's type). The include path
// and the name below are unchanged for every existing caller in this tree;
// only the definition moved.
#ifndef SRC_SMD_CL_FOUNDATION_STATIC_VECTOR_HPP
#define SRC_SMD_CL_FOUNDATION_STATIC_VECTOR_HPP

#include <smd/kit/foundation/static_vector.hpp>

namespace smd::cl::foundation {

using smd::kit::foundation::static_vector;

} // namespace smd::cl::foundation

#endif
