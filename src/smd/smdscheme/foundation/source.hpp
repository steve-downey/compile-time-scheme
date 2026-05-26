// src/smd/schemepoc/source.hpp                                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_FOUNDATION_SOURCE_HPP
#define SRC_SMD_SMDSCHEME_FOUNDATION_SOURCE_HPP

namespace smd::smdscheme::foundation {

struct source_pos {
    int offset{};
    int line{1};
    int column{1};

    friend constexpr auto operator==(source_pos, source_pos) -> bool = default;
};

struct source_span {
    source_pos first{};
    source_pos last{};

    friend constexpr auto operator==(source_span, source_span)
        -> bool = default;
};

struct parse_error {
    source_pos where{};
    char const *message{};

    friend constexpr auto operator==(parse_error const &lhs,
                                     parse_error const &rhs) -> bool {
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
