// src/smd/cl/printer/prin1.hpp                                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_PRINTER_PRIN1_HPP
#define SRC_SMD_CL_PRINTER_PRIN1_HPP

#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/foundation/source_pos.hpp>
#include <smd/cl/foundation/static_vector.hpp>
#include <smd/cl/foundation/tagged_tree_schemes.hpp>
#include <smd/cl/reader/datum.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <variant>

/// A `prin1`-shaped printer for `smd::cl` datums (decision D16's differential
/// oracle, closing the gap A3 opened): a canonical structural rendering
/// sufficient for comparing against an outside Common Lisp implementation,
/// not a full ANSI printer.
///
/// Deliberately **not** rendered: `|...|` multiple-escape quoting of symbol
/// names (the reader upcases unescaped names, DIV-0001, so the common case
/// round-trips; a name needing escapes does not, and this printer does not
/// pretend otherwise), `*print-circle*`/`*print-level*`/`*print-length*`,
/// packages, readtable case other than upcase, and pretty-printer layout.
/// The quote family (`backquote`, `unquote`, `unquote_splice`) renders with
/// `` ` ``/`,`/`,@` prefixes so the printer is total, but those three are
/// excluded from every oracle comparison: SBCL 2.2.9 prints them as
/// implementation-specific reader-macro objects, which ANSI permits.
namespace smd::cl::printer {

/// Maximum characters one rendered datum may occupy.
///
/// A provisional decision, not a load-bearing one: `foundation::cata_short`
/// materialises one carrier per node, so a fold over an `MaxNodes`-node tree
/// holds `MaxNodes * max_print_chars` bytes — 32 KiB at this step's test
/// sizes. Revisit this only if a consumer actually wants a wide tree printed
/// in full; nothing here needs that yet.
inline constexpr int max_print_chars = 512;

/// One rendered datum's text, in the printer's own fixed-capacity storage.
using print_text = foundation::static_vector<char, max_print_chars>;

namespace detail {

/// The printer's one failure: the rendered text of some node would not fit
/// in `max_print_chars`. `foundation::cata_short`'s short-circuiting carries
/// this outward from wherever it first occurs.
[[nodiscard]] constexpr auto overflow() -> foundation::parse_error {
    return foundation::parse_error{foundation::source_pos{},
                                   "print buffer overflow"};
}

[[nodiscard]] constexpr auto fits(int needed) -> bool {
    return needed <= max_print_chars;
}

[[nodiscard]] constexpr auto view_of(print_text const &text) -> std::string_view {
    return std::string_view(text.begin(), static_cast<std::size_t>(text.size()));
}

/// Renders @p text verbatim, diagnosing overflow rather than asserting.
[[nodiscard]] constexpr auto render_verbatim(std::string_view text)
    -> foundation::result<print_text> {
    if (!fits(static_cast<int>(text.size()))) {
        return overflow();
    }
    print_text out;
    out.append_range(text);
    return out;
}

/// Renders a fixnum's decimal digits, `-` prefix when negative.
[[nodiscard]] constexpr auto render_fixnum(int value)
    -> foundation::result<print_text> {
    std::array<char, 16> digits{};
    auto const written =
        std::to_chars(digits.data(), digits.data() + digits.size(), value);
    return render_verbatim(std::string_view(
        digits.data(), static_cast<std::size_t>(written.ptr - digits.data())));
}

/// Renders `#\` followed by the character itself — not a character name
/// table. SBCL 2.2.9 happens to print space the same way (`#\` then a
/// literal space); `#\Newline` and `#\Tab` print by name there, which this
/// printer does not attempt, so the compared corpus keeps `#\Space` and
/// friends out.
[[nodiscard]] constexpr auto render_character(char value)
    -> foundation::result<print_text> {
    if (!fits(2)) {
        return overflow();
    }
    print_text out;
    out.append_range(std::string_view{"#\\"});
    out.push_back(value);
    return out;
}

/// Renders a string datum: `"..."`, with `\` before an embedded `"` or `\`.
[[nodiscard]] constexpr auto render_string(std::string_view contents)
    -> foundation::result<print_text> {
    int const body = std::ranges::fold_left(
        contents, 0, [](int acc, char c) {
            return acc + (c == '"' || c == '\\' ? 2 : 1);
        });
    if (!fits(2 + body)) {
        return overflow();
    }
    print_text out;
    out.push_back('"');
    out = std::ranges::fold_left(
        contents, std::move(out), [](print_text acc, char c) {
            if (c == '"' || c == '\\') {
                acc.push_back('\\');
            }
            acc.push_back(c);
            return acc;
        });
    out.push_back('"');
    return out;
}

/// Dispatches one leaf's variant alternative to its renderer. A single
/// `std::visit` over one node's own leaf, per `docs/cpp-rules.md`: it
/// dispatches, it does not drive any recursion.
template <class SymbolTable>
[[nodiscard]] constexpr auto render_leaf(reader::datum_atom const &atom,
                                         SymbolTable const &symbols)
    -> foundation::result<print_text> {
    return std::visit(
        [&symbols](auto const &value) -> foundation::result<print_text> {
            using value_type = std::remove_cvref_t<decltype(value)>;
            if constexpr (std::same_as<value_type, reader::datum_fixnum>) {
                return render_fixnum(value.value);
            } else if constexpr (std::same_as<value_type,
                                               reader::datum_symbol>) {
                return render_verbatim(symbols.name(value.id));
            } else if constexpr (std::same_as<value_type,
                                               reader::datum_keyword>) {
                return render_verbatim(symbols.name(value.id));
            } else if constexpr (std::same_as<value_type,
                                               reader::datum_character>) {
                return render_character(value.value);
            } else if constexpr (std::same_as<value_type,
                                               reader::datum_string>) {
                return render_string(value.view());
            } else {
                static_assert(std::same_as<value_type, reader::datum_tower>,
                             "datum_atom gained an alternative this visitor "
                             "does not know");
                // A tower renders its spelling, not its value (decision
                // D19: readable before executable) — see the architecture
                // doc's note on why the oracle and this printer disagree
                // outside canonical decimal spellings.
                return render_verbatim(value.spelling.view());
            }
        },
        atom);
}

/// Renders `open`, the children's already-rendered text separated by one
/// space, then `close` — the shape shared by non-empty lists and vectors.
template <int MaxList>
[[nodiscard]] constexpr auto
join_delimited(std::string_view open,
              foundation::static_vector<print_text, MaxList> const &children,
              std::string_view close) -> foundation::result<print_text> {
    int const separators = std::max(children.size() - 1, 0);
    int total = static_cast<int>(open.size()) + static_cast<int>(close.size()) +
               separators;
    total = std::ranges::fold_left(
        children, total,
        [](int acc, print_text const &child) { return acc + child.size(); });
    if (!fits(total)) {
        return overflow();
    }
    print_text out;
    out.append_range(open);
    out.append_range(children | std::views::join_with(' '));
    out.append_range(close);
    return out;
}

/// Renders `prefix`, one child's already-rendered text, then `suffix` — the
/// shape shared by the one-child branches (`quote`, `function`, and the
/// backquote family).
[[nodiscard]] constexpr auto wrap(std::string_view prefix,
                                  print_text const &child,
                                  std::string_view suffix)
    -> foundation::result<print_text> {
    int const total = static_cast<int>(prefix.size()) + child.size() +
                      static_cast<int>(suffix.size());
    if (!fits(total)) {
        return overflow();
    }
    print_text out;
    out.append_range(prefix);
    out.append_range(view_of(child));
    out.append_range(suffix);
    return out;
}

/// Dispatches one branch's tag with a `switch`, per `docs/cpp-rules.md`.
template <int MaxList>
[[nodiscard]] constexpr auto
render_branch(reader::datum_branch tag,
              foundation::static_vector<print_text, MaxList> const &children)
    -> foundation::result<print_text> {
    switch (tag) {
    case reader::datum_branch::list:
        // SBCL prints the empty list as NIL, never as "()".
        if (children.empty()) {
            return render_verbatim("NIL");
        }
        return join_delimited("(", children, ")");
    case reader::datum_branch::vector:
        return join_delimited("#(", children, ")");
    case reader::datum_branch::quote:
        if (children.empty()) {
            return foundation::parse_error{foundation::source_pos{},
                                           "quote branch has no child"};
        }
        // With *print-pretty* nil, SBCL prints 'x as (QUOTE X) — the
        // ANSI-defined reading of the syntax, not a printer-variable
        // default.
        return wrap("(QUOTE ", children[0], ")");
    case reader::datum_branch::function:
        if (children.empty()) {
            return foundation::parse_error{foundation::source_pos{},
                                           "function branch has no child"};
        }
        return wrap("(FUNCTION ", children[0], ")");
    case reader::datum_branch::backquote:
        if (children.empty()) {
            return foundation::parse_error{foundation::source_pos{},
                                           "backquote branch has no child"};
        }
        return wrap("`", children[0], "");
    case reader::datum_branch::unquote:
        if (children.empty()) {
            return foundation::parse_error{foundation::source_pos{},
                                           "unquote branch has no child"};
        }
        return wrap(",", children[0], "");
    case reader::datum_branch::unquote_splice:
        if (children.empty()) {
            return foundation::parse_error{
                foundation::source_pos{}, "unquote-splice branch has no child"};
        }
        return wrap(",@", children[0], "");
    }
    return foundation::parse_error{foundation::source_pos{},
                                   "unknown datum branch"};
}

} // namespace detail

// d67e6809-36c9-43c2-84cf-14f2d189aa77
/// Renders @p tree as `prin1` would, interning nothing and consulting
/// @p symbols only to recover interned names.
///
/// This is a `foundation::cata_short`, not a hand-written recursive
/// descent: the carrier is the rendered text of one node, and the failure
/// channel is the one way rendering fails — the text would overflow
/// @ref max_print_chars. See this header's own doc comment for what is
/// deliberately not rendered.
///
/// @tparam MaxNodes    @p tree's node capacity.
/// @tparam MaxList     @p tree's per-list element capacity.
/// @tparam SymbolTable A table offering `name(symbol_id) const`.
template <int MaxNodes, int MaxList, class SymbolTable>
[[nodiscard]] constexpr auto
prin1(reader::datum_tree<MaxNodes, MaxList> const &tree,
      SymbolTable const &symbols) -> foundation::result<print_text> {
    using layer =
        foundation::node_f<reader::datum_atom, reader::datum_branch,
                           print_text, MaxList>;
    auto const algebra = [&symbols](layer const &node)
        -> foundation::result<print_text> {
        if (auto const *leaf = std::get_if<reader::datum_atom>(&node)) {
            return detail::render_leaf(*leaf, symbols);
        }
        auto const &branch = std::get<
            foundation::branch_f<reader::datum_branch, print_text, MaxList>>(
            node);
        return detail::render_branch(branch.tag, branch.children);
    };
    return foundation::cata_short<print_text>(tree, algebra);
}
// d67e6809-36c9-43c2-84cf-14f2d189aa77 end

} // namespace smd::cl::printer

#endif
