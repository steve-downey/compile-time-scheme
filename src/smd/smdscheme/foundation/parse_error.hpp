// src/smd/smdscheme/foundation/parse_error.hpp                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_FOUNDATION_PARSE_ERROR_HPP
#define SRC_SMD_SMDSCHEME_FOUNDATION_PARSE_ERROR_HPP

#include <smd/smdscheme/foundation/source_pos.hpp>

namespace smd::smdscheme::foundation {

/// A parse failure with the position in the input where it occurred
/// and a static string describing the expected token or form.
///
/// The @p message pointer must be a string literal or have static lifetime;
/// the struct does not own or copy the pointed-to string.
struct parse_error {
    foundation::source_pos where{}; ///< Position of the failure in the input.
    char const *message{}; ///< Static description of what was expected.

    friend constexpr auto operator==(foundation::parse_error const &lhs,
                                     foundation::parse_error const &rhs)
        -> bool {
        if (!(lhs.where == rhs.where)) {
            return false;
        }
        if (lhs.message == rhs.message) {
            return true;
        }
        if (lhs.message == nullptr || rhs.message == nullptr) {
            return false;
        }
        auto const *a = lhs.message;
        auto const *b = rhs.message;
        while (*a != '\0' && *b != '\0') {
            if (*a != *b) {
                return false;
            }
            ++a;
            ++b;
        }
        return *a == *b;
    }
};

} // namespace smd::smdscheme::foundation

#endif
