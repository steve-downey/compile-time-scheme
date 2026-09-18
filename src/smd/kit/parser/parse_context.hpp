// src/smd/kit/parser/parse_context.hpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_KIT_PARSER_PARSE_CONTEXT_HPP
#define SRC_SMD_KIT_PARSER_PARSE_CONTEXT_HPP

#include <smd/kit/parser/cursor.hpp>

#include <concepts>
#include <type_traits>

namespace smd::kit::parser {

/// The role @ref parser threads as its second argument: whatever a client
/// needs shared across one parse, distinct from the @ref cursor threaded as
/// its first. The kit cannot require anything language-specific of a
/// context, so this names the parameter's role rather than pretending to
/// constrain its shape. What it does catch is a @ref cursor passed where a
/// context belongs -- an argument-order mistake the type system would
/// otherwise miss silently, since both parameters are ordinary value-like
/// types.
template <class T>
concept parse_context = !std::same_as<std::remove_cvref_t<T>, cursor>;

/// A context for a parser that genuinely needs none.
///
/// Not a default template or function argument: D29 (docs/cl-parser-scoping.md)
/// rejects a captured context precisely because combinators reify parsers
/// into storable values, so defaulting the context here would invite the
/// same lifetime hazard back in by a different door. There is no backward
/// compatibility to preserve -- @c cl is this layer's only client.
struct no_context {};

} // namespace smd::kit::parser

#endif
