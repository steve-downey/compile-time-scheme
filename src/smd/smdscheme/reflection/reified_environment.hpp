// src/smd/smdscheme/reflection/reified_environment.hpp -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_REFLECTION_REIFIED_ENVIRONMENT_HPP
#define SRC_SMD_SMDSCHEME_REFLECTION_REIFIED_ENVIRONMENT_HPP

#include <meta>
#include <string_view>
#include <vector>

namespace smd::smdscheme::reflection {
// d0092f7c-b9de-440d-ac67-02e4e48c5aa4

/// Describes one captured variable in a compile-time environment: its type
/// (as a reflection) and its name.
struct capture_desc {
    std::meta::info type;  ///< P2996 reflection of the variable's type.
    std::string_view name; ///< The variable name to use as a struct field name.
};

/// Target template into which @ref compile_environment injects data members.
///
/// Specialize this template (implicitly, via @ref compile_environment) to
/// produce an ordinary aggregate type whose fields correspond to captured
/// variables.  A unique @p Tag per call site prevents different injections
/// from aliasing each other.
///
/// @tparam Tag A unique tag type identifying this particular environment shape.
template <typename Tag>
struct reified_environment;

/// Injects fields into @c reified_environment<Tag> based on the supplied
/// capture descriptors.
///
/// Generates an ordinary aggregate type by calling
/// @c std::meta::define_aggregate.  The result is a plain struct with one
/// data member per entry in @p captures.  This keeps environment shapes out
/// of the evaluation runtime and avoids limitations of consteval allocation.
///
/// @tparam Tag      Unique tag type for this environment shape.
/// @param  captures Ordered list of (type, name) pairs for the injected fields.
template <typename Tag>
consteval void compile_environment(std::vector<capture_desc> captures) {
    std::vector<std::meta::info> members;
    for (auto c : captures) {
        members.push_back(
            std::meta::data_member_spec(c.type, {.name = c.name}));
    }
    std::meta::define_aggregate(^^reified_environment<Tag>, members);
}
// d0092f7c-b9de-440d-ac67-02e4e48c5aa4 end

} // namespace smd::smdscheme::reflection

#endif
