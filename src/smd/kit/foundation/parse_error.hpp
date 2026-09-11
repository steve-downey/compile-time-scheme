// src/smd/kit/foundation/parse_error.hpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Extracted in step R8 from smd::cl::foundation::parse_error, itself the
// reviewed union of two independently-drifted copies:
// src/smd/smdscheme/foundation/parse_error.hpp (compile-time-scheme, at
// iteration/smdscheme-final) and src/smd/forth/foundation/parse_error.hpp
// (compile-time-forth). The equality below keeps the forth copy's explicit
// null handling; the character loop both copies carried is replaced by
// std::string_view comparison, and the scheme copy's same-pointer fast path
// is gone, since clang will not constant-evaluate an address comparison of
// two string literals. All of that is a strict quality improvement over
// either source copy, not a cl-specific need, so it travels with the
// extraction.
#ifndef SRC_SMD_KIT_FOUNDATION_PARSE_ERROR_HPP
#define SRC_SMD_KIT_FOUNDATION_PARSE_ERROR_HPP

#include <smd/kit/foundation/source_pos.hpp>

#include <string_view>

namespace smd::kit::foundation {

/// A parse failure with the position in the input where it occurred
/// and a static string describing the expected token or form.
///
/// The @p message pointer must be a string literal or have static lifetime;
/// the struct does not own or copy the pointed-to string.
struct parse_error {
    foundation::source_pos where{}; ///< Position of the failure in the input.
    char const *message{}; ///< Static description of what was expected.

    // HIDDEN FRIEND
    friend constexpr auto operator==(foundation::parse_error const &lhs,
                                     foundation::parse_error const &rhs)
        -> bool {
        if (!(lhs.where == rhs.where)) {
            return false;
        }
        // Null is handled by itself, and there is no same-pointer fast
        // path: comparing the addresses of two string literals is not a
        // constant expression when they might overlap, and clang refuses
        // it under static_assert even when gcc folds it.
        if (lhs.message == nullptr || rhs.message == nullptr) {
            return lhs.message == nullptr && rhs.message == nullptr;
        }
        return std::string_view{lhs.message} == std::string_view{rhs.message};
    }
};

} // namespace smd::kit::foundation

#endif
