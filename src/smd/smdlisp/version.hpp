// src/smd/smdlisp/version.hpp                                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_VERSION_HPP
#define SRC_SMD_SMDLISP_VERSION_HPP

#include <smd/smdscheme/foundation/version.hpp>
#include <smd/smdscheme/parser/cursor.hpp>

namespace smd::smdlisp {

inline constexpr int version_major = 0; ///< Major version component.
inline constexpr int version_minor = 1; ///< Minor version component.
inline constexpr int version_patch = 0; ///< Patch version component.

/// Aggregates the three version components as static constexpr members.
struct version {
    static constexpr int major = version_major; ///< Major version.
    static constexpr int minor = version_minor; ///< Minor version.
    static constexpr int patch = version_patch; ///< Patch version.
};

/// Returns true when the @c smdscheme.foundation target is linked and its
/// symbols are usable from @c smdlisp.
///
/// This has no independent semantic purpose; it exists to prove the
/// `smdlisp -> smdscheme` dependency direction compiles and links, per
/// docs/cl-pivot-plan.md decision D1 (smdlisp consumes smdscheme's
/// language-agnostic foundation and parser targets rather than copying
/// them).
constexpr auto links_smdscheme_foundation() -> bool {
    return smd::smdscheme::foundation::version_major >= 0;
}

/// Returns true when the @c smdscheme.parser target is linked and its
/// symbols are usable from @c smdlisp.
///
/// Same rationale as @ref links_smdscheme_foundation, exercised against the
/// parser target instead.
constexpr auto links_smdscheme_parser() -> bool {
    smd::smdscheme::parser::cursor const probe{"probe"};
    return !probe.empty();
}

} // namespace smd::smdlisp

#endif
