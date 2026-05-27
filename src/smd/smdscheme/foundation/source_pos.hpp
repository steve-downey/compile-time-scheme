// src/smd/smdscheme/foundation/source_pos.hpp                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_FOUNDATION_SOURCE_POS_HPP
#define SRC_SMD_SMDSCHEME_FOUNDATION_SOURCE_POS_HPP

namespace smd::smdscheme::foundation {

struct source_pos {
    int offset{};
    int line{1};
    int column{1};

    friend constexpr auto operator==(foundation::source_pos,
                                     foundation::source_pos) -> bool = default;
};

} // namespace smd::smdscheme::foundation

#endif
