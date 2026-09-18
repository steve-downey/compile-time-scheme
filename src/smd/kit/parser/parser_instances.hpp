// src/smd/kit/parser/parser_instances.hpp                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// New in step B2: parser<F> registered as a second Monad instance, in
// exactly the shape src/smd/kit/foundation/result_instances.hpp uses for
// result<T> -- an Impl struct supplying bind and pure, a foundation::monad
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
        return parser{[p = std::move(p),
                       g = std::move(g)](cursor cur, parse_context auto &ctx) {
            return foundation::bind(p(cur, ctx), [&g, &ctx](auto const &state) {
                return g(state.value)(state.rest, ctx);
            });
        }};
    }

    /// @c fmap, supplied natively rather than inherited.
    ///
    /// @ref foundation::derive_monad derives @c fmap as
    /// <tt>bind(ma, [&](a) { return pure(f(a)); })</tt>, and that lambda
    /// holds @p f by reference. A strict instance runs the lambda before
    /// @c fmap returns, so the reference is live for every use of it. This
    /// instance does not run it: @c bind above stores the continuation
    /// inside the parser it returns, that parser outlives the @c fmap call,
    /// and @p f does not. The derivation reads a dead callable, which is
    /// the case @c monad.hpp's own caveat names -- an instance that defers
    /// its continuation instead of running it "has to supply these
    /// operations itself rather than inherit them". This is that supply,
    /// and @ref smd::kit::parser::map is where the correct spelling already
    /// lived: both operands by value, nothing captured by reference.
    ///
    /// It is not a Functor registration. @c foundation::functor<parser<F>>
    /// is still unspecialized, so the @c fmap CPO still cannot reach a
    /// parser; only a caller holding the monad object can, and now it gets
    /// an answer instead of a dangling read.
    template <class F, class P>
    constexpr auto fmap(this auto &&, F f, P p) {
        return smd::kit::parser::map(std::move(p), std::move(f));
    }
};

/// Monad instance map for @ref parser.
///
/// What @ref foundation::derive_monad contributes, and what it does not.
/// Live on this branch: @c bind, reached through the CPO from
/// @c src/smd/cl/reader/detail/skip.hpp and @c .../sharpsign.hpp, and
/// @c pure. Inherited, called nowhere yet, and correct when they are:
/// @c join, @c then and @c kleisli, none of whose derivations captures by
/// reference anything the deferred parser outlives -- @c then's own comment
/// says it was written for this case. Inherited but absent from overload
/// resolution: @c apply, whose requires-clause needs
/// @c element_type_t<parser<F>>. Supplied above instead of inherited:
/// @c fmap, and with it @c as_functor, which probes back through to it.
///
/// @c element_type_t<parser<F>> does not exist, and that is structural
/// rather than an omission: @c parser<F>::operator() is a template over the
/// threaded context, so the parsed value type is not a property of the
/// parser type and no @c value_type could name it. @c parser<F> therefore
/// satisfies neither @c foundation::monad_impl nor
/// @c foundation::monad_object. That is the position, not a defect --
/// those concepts describe a context with an extractable element type, and
/// nothing here needs one: @c bind_fn and the other operation objects key
/// on @c monad<M>, never on either concept.
struct parser_monad_map : foundation::derive_monad<parser_monad_impl> {
    using parser_monad_impl::bind;
    using parser_monad_impl::pure;
};
// 606983ed-f5d7-454d-9570-3b510ace3aba end

} // namespace smd::kit::parser

// parser<F>'s Monad instance is registered here rather than back inside
// namespace smd::kit::parser: the monad lookup variable lives in
// smd::kit::foundation (src/smd/kit/foundation/monad.hpp), and a
// variable-template specialization must be declared in a namespace that
// encloses the primary template's own namespace -- smd::kit::parser is a
// sibling of smd::kit::foundation, not an ancestor of it, so it cannot host
// this declaration even fully qualified.
// src/smd/cl/foundation/tagged_tree_instances.hpp already carries this same
// shape for the same reason, one level down in this same tree.
namespace smd::kit::foundation {

/// Registers the Monad instance for @ref smd::kit::parser::parser.
template <class F>
inline constexpr auto monad<smd::kit::parser::parser<F>> =
    smd::kit::parser::parser_monad_map{};

} // namespace smd::kit::foundation

#endif
