// src/smd/schemepoc/reflection_reify.hpp            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef SRC_SMD_SCHEMEPOC_REFLECTION_REIFY_HPP
#define SRC_SMD_SCHEMEPOC_REFLECTION_REIFY_HPP

#include <meta>
#include <vector>
#include <string_view>

namespace smd::schemepoc {

// A compile-time description of a captured environment variable
struct capture_desc {
    std::meta::info type;
    std::string_view name;
};

// Target template for generating environment aggregate shapes.
// `Tag` allows creating uniquely named aggregate structures per invocation context.
template <typename Tag>
struct reified_environment;

// Inject fields into `reified_environment<Tag>` based on descriptor
// This generates an ordinary aggregate type that doesn't cross into evaluation runtime limits.
template <typename Tag>
consteval void compile_environment(std::vector<capture_desc> captures) {
    std::vector<std::meta::info> members;
    for (auto c : captures) {
        members.push_back(std::meta::data_member_spec(c.type, {.name = c.name}));
    }
    std::meta::define_aggregate(^^reified_environment<Tag>, members);
}

} // namespace smd::schemepoc

#endif
