// src/smd/cl/foundation/source_span.hpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Reviewed union of the two prior copies of this component:
// src/smd/smdscheme/foundation/source_span.hpp (compile-time-scheme) and
// src/smd/forth/foundation/source_span.hpp (compile-time-forth).
#ifndef SRC_SMD_CL_FOUNDATION_SOURCE_SPAN_HPP
#define SRC_SMD_CL_FOUNDATION_SOURCE_SPAN_HPP

#include <smd/cl/foundation/source_pos.hpp>

namespace smd::cl::foundation {

/// A half-open range in source text, defined by its first and last positions.
struct source_span {
    foundation::source_pos first{}; ///< Inclusive start of the span.
    foundation::source_pos last{};  ///< Exclusive end of the span.

    friend constexpr auto operator==(source_span, source_span)
        -> bool = default;
};

} // namespace smd::cl::foundation

#endif
