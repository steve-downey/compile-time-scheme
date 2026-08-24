// src/smd/kit/parser/parser_instances.hpp                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// New in step B2: parser<F> registered as a second Monad instance, in
// exactly the shape src/smd/kit/foundation/result_instances.hpp uses for
// result<T> -- an Impl struct supplying bind and pure, a monad_typeclass
// specialization, and every caller reaching bind through the same generic
// CPO result already uses. There is no parser-domain-specific spelling
// analogous to and_then: unlike result, parser has no pre-existing domain
// name to hide the CPO behind, so every call site qualifies bind or brings
// it into scope with a using declaration (see
// src/smd/cl/reader/detail/read_context.hpp).
#ifndef SRC_SMD_KIT_PARSER_PARSER_INSTANCES_HPP
#define SRC_SMD_KIT_PARSER_PARSER_INSTANCES_HPP

#include <smd/kit/foundation/monad.hpp>
#include <smd/kit/foundation/result.hpp>
#include <smd/kit/foundation/result_instances.hpp>
#include <smd/kit/parser/cursor.hpp>
#include <smd/kit/parser/parse_context.hpp>
#include <smd/kit/parser/parser.hpp>

#include <utility>

namespace smd::kit::parser {

// 606983ed-f5d7-454d-9570-3b510ace3aba
/// Monad @c Impl for @ref parser.
///
/// @c bind runs @p p; only on success does it run @p g on @p p's value and
/// resume from @p p's rest cursor, over the same threaded context. Written
/// in terms of @ref foundation::result's own registered Monad instance one
/// level down, rather than re-deriving the success/failure branch by hand:
/// @c p(cur, ctx) is already a @c foundation::result, so sequencing it
/// through @c foundation::bind is what skips @p g on failure.
struct parser_monad_impl {
    template <class T>
    constexpr auto pure(this auto &&, T value) {
        return smd::kit::parser::pure(std::move(value));
    }

    template <class F, class G>
    constexpr auto bind(this auto &&, parser<F> p, G g) {
        return parser{[p = std::move(p), g = std::move(g)](
                          cursor cur, parse_context auto &ctx) {
            return foundation::bind(
                p(cur, ctx), [&g, &ctx](auto const &state) {
                    return g(state.value)(state.rest, ctx);
                });
        }};
    }
};

/// Monad instance map for @ref parser.
struct parser_monad_map : foundation::monad<parser_monad_impl> {
    using parser_monad_impl::bind;
    using parser_monad_impl::pure;
};
// 606983ed-f5d7-454d-9570-3b510ace3aba end

} // namespace smd::kit::parser

// parser<F>'s Monad instance is registered here rather than back inside
// namespace smd::kit::parser: monad_typeclass lives in smd::kit::foundation
// (src/smd/kit/foundation/monad.hpp), and a variable-template specialization
// must be declared in a namespace that encloses the primary template's own
// namespace -- smd::kit::parser is a sibling of smd::kit::foundation, not an
// ancestor of it, so it cannot host this declaration even fully qualified.
// src/smd/cl/foundation/tagged_tree_instances.hpp already carries this same
// shape for the same reason, one level down in this same tree.
namespace smd::kit::foundation {

/// Registers the Monad instance for @ref smd::kit::parser::parser.
template <class F>
inline constexpr auto monad_typeclass<smd::kit::parser::parser<F>> =
    smd::kit::parser::parser_monad_map{};

} // namespace smd::kit::foundation

#endif
