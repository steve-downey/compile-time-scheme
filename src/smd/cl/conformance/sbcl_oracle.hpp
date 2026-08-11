// src/smd/cl/conformance/sbcl_oracle.hpp                         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_CONFORMANCE_SBCL_ORACLE_HPP
#define SRC_SMD_CL_CONFORMANCE_SBCL_ORACLE_HPP

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>

/// The differential half of decision D16: an informal cross-check against
/// SBCL, which R5's step brief left pending ("no CL implementation is
/// installed here, so R3's reader syntax, R4's elaboration and R5's
/// evaluation semantics were all derived from the specification and pinned
/// by tests").
///
/// Deliberately runtime-only and not `constexpr`: shelling out to a
/// subprocess has no meaning under constant evaluation, and the evaluator
/// side of any comparison already has its compile-time twin in
/// <smd/cl/conformance/corpus.hpp>. Every caller must treat a missing
/// `sbcl` binary as a skip, never a failure — the orchestrator's decision
/// is that this differential test is a member of the default suite
/// (`make test`), so an environment without SBCL installed must stay
/// green, not turn red for a reason it cannot fix.
namespace smd::cl::conformance {

namespace detail {

/// Runs @p command through the shell and returns its captured stdout, or
/// `nullopt` if the shell could not start the command or it exited
/// nonzero.
[[nodiscard]] inline auto run_shell_capture(std::string const &command)
    -> std::optional<std::string> {
    std::array<char, 256> buffer{};
    std::string output;
    FILE *const pipe = popen(command.c_str(), "r"); // NOLINT
    if (pipe == nullptr) {
        return std::nullopt;
    }
    std::size_t read = 0;
    while ((read = std::fread(buffer.data(), 1, buffer.size(), pipe)) > 0) {
        output.append(buffer.data(), read);
    }
    int const status = pclose(pipe);
    if (status != 0) {
        return std::nullopt;
    }
    return output;
}

/// Quotes @p text as a single POSIX shell word, so a Lisp form containing
/// parentheses, quotes or whitespace reaches SBCL as one `--eval` argument
/// rather than being reinterpreted by the shell.
[[nodiscard]] inline auto shell_quote(std::string_view text) -> std::string {
    std::string quoted = "'";
    for (char const c : text) {
        if (c == '\'') {
            quoted += "'\\''";
        } else {
            quoted += c;
        }
    }
    quoted += "'";
    return quoted;
}

/// Trims leading and trailing ASCII whitespace from @p text.
[[nodiscard]] inline auto trim(std::string text) -> std::string {
    auto const not_space = [](unsigned char c) { return std::isspace(c) == 0; };
    auto const first = std::ranges::find_if(text, not_space);
    text.erase(text.begin(), first);
    auto const last =
        std::ranges::find_if(text | std::views::reverse, not_space).base();
    text.erase(last, text.end());
    return text;
}

} // namespace detail

// 29a14fe5-7268-446e-9489-7ec8df52494c
/// Probes for an `sbcl` binary on `PATH` and returns its version string
/// (e.g. `SBCL 2.2.9.debian`), or `nullopt` if none is reachable.
///
/// A runtime probe rather than a build-time dependency, per the
/// orchestrator's decision: the differential test must degrade to a skip in
/// an environment without SBCL, not fail to configure. The returned string
/// is what a reported divergence should be attributed to, exactly as
/// `compile-time-forth`'s `gforth_diff` pins the gforth version it ran
/// against.
[[nodiscard]] inline auto find_sbcl_version() -> std::optional<std::string> {
    auto output = detail::run_shell_capture("sbcl --version 2>/dev/null");
    if (!output.has_value()) {
        return std::nullopt;
    }
    auto trimmed = detail::trim(std::move(*output));
    if (trimmed.empty()) {
        return std::nullopt;
    }
    return trimmed;
}
// 29a14fe5-7268-446e-9489-7ec8df52494c end

// 1e9c6a05-3f50-4a61-8552-e879e2053d2d
/// Evaluates @p form as a single SBCL top-level form and returns what
/// `prin1` prints for its value — the readable representation, which for a
/// fixnum or the symbols `T`/`NIL` matches this project's own printed form
/// exactly (both readers upcase). Returns `nullopt` if SBCL is unreachable;
/// a form SBCL itself signals an error on prints as the literal
/// `SBCL-ERROR`, a string no well-typed corpus form's `prin1` output can
/// collide with.
///
/// This is deliberately the informal half of the check named in the R5 step
/// brief: it compares one printed representation, not a full
/// condition-system-aware equivalence, which is enough to catch a semantic
/// disagreement on the scalar-valued forms the guarded differential test
/// runs it against, without building a general SBCL-condition-to-`outcome`
/// translation this step does not need.
[[nodiscard]] inline auto sbcl_prin1(std::string_view form)
    -> std::optional<std::string> {
    std::string const script = "(handler-case (prin1 " + std::string(form) +
                               ") (error () (princ \"SBCL-ERROR\")))";
    std::string const command =
        "sbcl --noinform --non-interactive --no-userinit --no-sysinit "
        "--eval " +
        detail::shell_quote(script) + " 2>/dev/null";
    auto output = detail::run_shell_capture(command);
    if (!output.has_value()) {
        return std::nullopt;
    }
    return detail::trim(std::move(*output));
}
// 1e9c6a05-3f50-4a61-8552-e879e2053d2d end

} // namespace smd::cl::conformance

#endif
