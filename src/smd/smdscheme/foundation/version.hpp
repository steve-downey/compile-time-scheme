// src/smd/smdscheme/foundation/version.hpp                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_FOUNDATION_VERSION_HPP
#define SRC_SMD_SMDSCHEME_FOUNDATION_VERSION_HPP

namespace smd::smdscheme::foundation {

inline constexpr int version_major = 0; ///< Major version component.
inline constexpr int version_minor = 1; ///< Minor version component.
inline constexpr int version_patch = 0; ///< Patch version component.

/// Aggregates the three version components as static constexpr members.
struct version {
    static constexpr int major = version_major; ///< Major version.
    static constexpr int minor = version_minor; ///< Minor version.
    static constexpr int patch = version_patch; ///< Patch version.
};

} // namespace smd::smdscheme::foundation

#endif
