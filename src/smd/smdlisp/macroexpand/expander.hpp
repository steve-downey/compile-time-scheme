// src/smd/smdlisp/macroexpand/expander.hpp                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_MACROEXPAND_EXPANDER_HPP
#define SRC_SMD_SMDLISP_MACROEXPAND_EXPANDER_HPP

#include <smd/smdlisp/closure/env.hpp>
#include <smd/smdlisp/closure/eval_direct.hpp>
#include <smd/smdlisp/closure/pairs.hpp>
#include <smd/smdlisp/closure/value.hpp>
#include <smd/smdlisp/elaborator/elaborate.hpp>
#include <smd/smdlisp/elaborator/elaborated_core.hpp>
#include <smd/smdlisp/reader/atom.hpp>
#include <smd/smdlisp/reader/datum_type.hpp>
#include <smd/smdscheme/foundation/arena_box.hpp>
#include <smd/smdscheme/foundation/parse_error.hpp>
#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/foundation/static_vector.hpp>

#include <smd/fixpoint/overloaded.hpp>

#include <array>
#include <span>
#include <string_view>
#include <utility>
#include <variant>

namespace smd::smdlisp::macroexpand {

/// The datum tree the expander reads and writes; an alias to keep this
/// file's signatures readable (decision D9: macro expansion is a
/// datum-to-datum pass, operating entirely in @ref reader terms, before
/// anything becomes an @ref elaborator::core_type node).
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
using datum = reader::datum_type<MaxNodes, MaxList>;

/// The arena backing @ref datum trees.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
using datum_arena =
    smdscheme::foundation::tree_arena<datum<MaxNodes, MaxList>, MaxNodes>;

/// A datum list, unwrapped from the @ref datum variant -- the shape every
/// macro call form and every host macro's expansion function operates on.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
using datum_list =
    reader::datum_list<datum<MaxNodes, MaxList>, MaxNodes, MaxList>;

/// A host-defined (C++) macro: a name and a function from a call-form datum
/// list to a replacement datum, writing any new nodes into the same arena
/// the call form came from.
///
/// Per docs/cl-pivot-plan.md step L17's sketch, @ref name is matched against
/// the folded spelling (decision D2) of the head symbol of a call form --
/// the same spelling-comparison pattern
/// @c elaborator::detail::elaborate_list already uses for special operators
/// (`sym.name.view() == "IF"`, etc.), reused here rather than inventing a
/// second symbol-comparison mechanism. There is no dedicated `symbol` type
/// at the datum layer (that is a value-layer, post-evaluation concept from
/// L7); comparing folded spellings via @c std::string_view is what "a
/// symbol" means at this layer.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
struct host_macro {
    /// The folded (uppercase, per D2) spelling this macro is registered
    /// under, e.g. `"COND"`.
    std::string_view name;

    /// `(call-form, arena) -> replacement datum, or error`.
    using macro_fn = smdscheme::foundation::result<datum<MaxNodes, MaxList>> (
            *)(datum_list<MaxNodes, MaxList> const &call,
               datum_arena<MaxNodes, MaxList> &arena);

    macro_fn expand;
};

/// Default budget for @ref macroexpand's fixpoint loop.
///
/// Per the plan: "budget exhaustion is a diagnosed error, not a hang."
/// None of the six host macros landed in this step can expand into another
/// call headed by the same macro's own name (each rewrites to `if`/`progn`/
/// `let`/`cond`, none of which are themselves in the macro table), so in
/// practice every one of them reaches fixpoint in exactly one
/// @ref macroexpand_1 step; the budget exists for macros not yet imagined
/// (and is exercised directly by a deliberately self-looping test macro,
/// see expander.test.cpp).
inline constexpr int default_max_expansions = 64;

namespace detail {

/// Builds a @ref datum symbol node from an already-uppercase static
/// spelling, folding it via @ref reader::detail::fold so there is one
/// definition of "how a folded name is built" (mirrors
/// @c elaborator::detail::literal_symbol's rationale exactly).
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto make_symbol(std::string_view spelling)
    -> datum<MaxNodes, MaxList> {
    using D = datum<MaxNodes, MaxList>;
    using datum_f =
        typename reader::datum_f_factory<MaxNodes, MaxList>::template type<D>;
    return D{datum_f{reader::datum_symbol{reader::detail::fold(spelling)}}};
}

/// The canonical `NIL` constant as a datum (decision D3): sole false value
/// and empty list.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto nil_symbol() -> datum<MaxNodes, MaxList> {
    return make_symbol<MaxNodes, MaxList>("NIL");
}

/// The canonical `T` constant as a datum (decision D3).
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto true_symbol() -> datum<MaxNodes, MaxList> {
    return make_symbol<MaxNodes, MaxList>("T");
}

/// Builds a fixed-arity datum list from already-computed datum values,
/// allocating each into @p arena as a child node. Used for shapes whose
/// element count is known at the call site (an `if`, a one-name `let`
/// binding, ...).
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
/// @tparam Ds       Element types, each convertible to @c
/// datum<MaxNodes,MaxList>.
template <int MaxNodes, int MaxList, typename... Ds>
[[nodiscard]] constexpr auto make_list_v(datum_arena<MaxNodes, MaxList> &arena,
                                         Ds const &...items)
    -> datum<MaxNodes, MaxList> {
    using D = datum<MaxNodes, MaxList>;
    using datum_f =
        typename reader::datum_f_factory<MaxNodes, MaxList>::template type<D>;
    datum_list<MaxNodes, MaxList> lst{};
    (lst.elements.push_back(
         smdscheme::foundation::make_arena_box(arena, D(items))),
     ...);
    return D{datum_f{std::move(lst)}};
}

/// Builds a datum list from a variable-length @ref
/// smdscheme::foundation::static_vector of already-computed datum values
/// (e.g. a `progn` body, a `cond`'s clause list). Complements
/// @ref make_list_v for shapes whose element count is only known once the
/// caller has walked its own input.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
/// @tparam N        Capacity of the source @c static_vector.
template <int MaxNodes, int MaxList, int N>
[[nodiscard]] constexpr auto make_list_from(
    datum_arena<MaxNodes, MaxList> &arena,
    smdscheme::foundation::static_vector<datum<MaxNodes, MaxList>, N> const
        &items) -> datum<MaxNodes, MaxList> {
    using D = datum<MaxNodes, MaxList>;
    using datum_f =
        typename reader::datum_f_factory<MaxNodes, MaxList>::template type<D>;
    datum_list<MaxNodes, MaxList> lst{};
    for (int i = 0; i < items.size(); ++i)
        lst.elements.push_back(
            smdscheme::foundation::make_arena_box(arena, items[i]));
    return D{datum_f{std::move(lst)}};
}

/// Wraps @p target in a native @ref reader::datum_quote node -- the same
/// node kind the reader's `'x` shorthand produces -- rather than
/// synthesizing a `(quote x)` list. Used by @ref expand_case to make case
/// keys literal data without going through the elaborator's second,
/// list-shaped spelling of quote.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto make_quote(datum_arena<MaxNodes, MaxList> &arena,
                                        datum<MaxNodes, MaxList> const &target)
    -> datum<MaxNodes, MaxList> {
    using D = datum<MaxNodes, MaxList>;
    using datum_f =
        typename reader::datum_f_factory<MaxNodes, MaxList>::template type<D>;
    return D{datum_f{reader::datum_quote<D, MaxNodes>{
        smdscheme::foundation::make_arena_box(arena, target)}}};
}

// -- (WHEN test body...) -> (IF test (PROGN body...) NIL) -------------------

/// @c when is a macro in ANSI CL (never a special operator), expanding to
/// `if`/`progn`. At least one body expression is required, matching the
/// same "at least one body expression" restriction @c elaborator::elaborate
/// already places on `lambda`/`progn`/`let`/`let*` bodies (not a new scope
/// cut introduced by this step).
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
expand_when(datum_list<MaxNodes, MaxList> const &call,
            datum_arena<MaxNodes, MaxList> &arena)
    -> smdscheme::foundation::result<datum<MaxNodes, MaxList>> {
    if (call.elements.size() < 3)
        return smdscheme::foundation::parse_error{
            {}, "when: expected a test and at least one body expression"};

    auto test = arena.get(call.elements[1]);

    smdscheme::foundation::static_vector<datum<MaxNodes, MaxList>, MaxList>
        body;
    body.push_back(make_symbol<MaxNodes, MaxList>("PROGN"));
    for (int i = 2; i < call.elements.size(); ++i)
        body.push_back(arena.get(call.elements[i]));
    auto consequent = make_list_from<MaxNodes, MaxList>(arena, body);

    return make_list_v<MaxNodes, MaxList>(
        arena, make_symbol<MaxNodes, MaxList>("IF"), test, consequent,
        nil_symbol<MaxNodes, MaxList>());
}

// -- (UNLESS test body...) -> (IF test NIL (PROGN body...)) -----------------

/// @c unless is @c when with the branches swapped; see @ref expand_when.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
expand_unless(datum_list<MaxNodes, MaxList> const &call,
              datum_arena<MaxNodes, MaxList> &arena)
    -> smdscheme::foundation::result<datum<MaxNodes, MaxList>> {
    if (call.elements.size() < 3)
        return smdscheme::foundation::parse_error{
            {}, "unless: expected a test and at least one body expression"};

    auto test = arena.get(call.elements[1]);

    smdscheme::foundation::static_vector<datum<MaxNodes, MaxList>, MaxList>
        body;
    body.push_back(make_symbol<MaxNodes, MaxList>("PROGN"));
    for (int i = 2; i < call.elements.size(); ++i)
        body.push_back(arena.get(call.elements[i]));
    auto alternative = make_list_from<MaxNodes, MaxList>(arena, body);

    return make_list_v<MaxNodes, MaxList>(
        arena, make_symbol<MaxNodes, MaxList>("IF"), test,
        nil_symbol<MaxNodes, MaxList>(), alternative);
}

// -- (AND a1 a2 ... an) -> nested (IF a1 (IF a2 ... an NIL) NIL) -------------

/// @c and short-circuits on the first `nil`, returning `t` for zero
/// arguments and the argument unchanged for exactly one, per ANSI CL.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
expand_and(datum_list<MaxNodes, MaxList> const &call,
           datum_arena<MaxNodes, MaxList> &arena)
    -> smdscheme::foundation::result<datum<MaxNodes, MaxList>> {
    int nargs = call.elements.size() - 1;
    if (nargs == 0)
        return true_symbol<MaxNodes, MaxList>();
    if (nargs == 1)
        return arena.get(call.elements[1]);

    datum<MaxNodes, MaxList> acc = arena.get(call.elements[nargs]);
    for (int i = nargs - 1; i >= 1; --i) {
        auto test = arena.get(call.elements[i]);
        acc = make_list_v<MaxNodes, MaxList>(
            arena, make_symbol<MaxNodes, MaxList>("IF"), test, acc,
            nil_symbol<MaxNodes, MaxList>());
    }
    return acc;
}

// -- (OR a1 a2 ... an) -> nested (LET ((%OR-TEMP a1)) (IF ... )) ------------

/// @c or returns the first non-`nil` argument, evaluating each argument at
/// most once. Per ANSI CL, `(or)` is `nil` and `(or form)` is `form`
/// unchanged; the multi-argument case must not re-evaluate a truthy
/// argument to both test and return it.
///
/// **Known divergence (see
/// docs/divergences/DIV-0006-macro-temp-name-capture.md):** single evaluation
/// is achieved with a fixed reserved binding name,
/// `%OR-TEMP`, rather than a gensym -- this project has no gensym/backquote
/// machinery yet (that arrives at L18/L19); a user program that binds a
/// variable literally named `%OR-TEMP` inside an `or` form can be shadowed
/// by this expansion.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
expand_or(datum_list<MaxNodes, MaxList> const &call,
          datum_arena<MaxNodes, MaxList> &arena)
    -> smdscheme::foundation::result<datum<MaxNodes, MaxList>> {
    int nargs = call.elements.size() - 1;
    if (nargs == 0)
        return nil_symbol<MaxNodes, MaxList>();
    if (nargs == 1)
        return arena.get(call.elements[1]);

    datum<MaxNodes, MaxList> acc = arena.get(call.elements[nargs]);
    for (int i = nargs - 1; i >= 1; --i) {
        auto test = arena.get(call.elements[i]);
        auto temp = make_symbol<MaxNodes, MaxList>("%OR-TEMP");
        auto binding = make_list_v<MaxNodes, MaxList>(arena, temp, test);
        auto bindings = make_list_v<MaxNodes, MaxList>(arena, binding);
        auto body = make_list_v<MaxNodes, MaxList>(
            arena, make_symbol<MaxNodes, MaxList>("IF"), temp, temp, acc);
        acc = make_list_v<MaxNodes, MaxList>(
            arena, make_symbol<MaxNodes, MaxList>("LET"), bindings, body);
    }
    return acc;
}

// -- (COND (test1 body1...) ...) -> nested IF/LET ----------------------------

/// @c cond evaluates each clause's test in order and, for the first true
/// test, evaluates and returns its body (an implicit `progn`); a
/// test-only clause (no body) returns the test's own value if true. `nil`
/// if no clause matches. Matches the same reserved-binding-name divergence
/// as @ref expand_or (`%COND-TMP`), for the same reason, only used for the
/// test-only-clause case where the test's value is both tested and
/// returned.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
expand_cond(datum_list<MaxNodes, MaxList> const &call,
            datum_arena<MaxNodes, MaxList> &arena)
    -> smdscheme::foundation::result<datum<MaxNodes, MaxList>> {
    int nclauses = call.elements.size() - 1;
    if (nclauses == 0)
        return nil_symbol<MaxNodes, MaxList>();

    datum<MaxNodes, MaxList> rest = nil_symbol<MaxNodes, MaxList>();
    for (int i = nclauses; i >= 1; --i) {
        auto const &clause_node = arena.get(call.elements[i]);
        if (!std::holds_alternative<datum_list<MaxNodes, MaxList>>(
                clause_node.inner))
            return smdscheme::foundation::parse_error{
                {}, "cond: each clause must be a list"};
        auto const &clause =
            std::get<datum_list<MaxNodes, MaxList>>(clause_node.inner);
        if (clause.elements.empty())
            return smdscheme::foundation::parse_error{
                {}, "cond: clause must have a test"};

        auto test = arena.get(clause.elements[0]);

        if (clause.elements.size() == 1) {
            auto temp = make_symbol<MaxNodes, MaxList>("%COND-TMP");
            auto binding = make_list_v<MaxNodes, MaxList>(arena, temp, test);
            auto bindings = make_list_v<MaxNodes, MaxList>(arena, binding);
            auto body = make_list_v<MaxNodes, MaxList>(
                arena, make_symbol<MaxNodes, MaxList>("IF"), temp, temp, rest);
            rest = make_list_v<MaxNodes, MaxList>(
                arena, make_symbol<MaxNodes, MaxList>("LET"), bindings, body);
        } else {
            smdscheme::foundation::static_vector<datum<MaxNodes, MaxList>,
                                                 MaxList>
                progn_items;
            progn_items.push_back(make_symbol<MaxNodes, MaxList>("PROGN"));
            for (int j = 1; j < clause.elements.size(); ++j)
                progn_items.push_back(arena.get(clause.elements[j]));
            auto consequent =
                make_list_from<MaxNodes, MaxList>(arena, progn_items);
            rest = make_list_v<MaxNodes, MaxList>(
                arena, make_symbol<MaxNodes, MaxList>("IF"), test, consequent,
                rest);
        }
    }
    return rest;
}

// -- (CASE keyform (key body...) ...) -> (LET ((%CASE-TMP keyform)) (COND...))

/// @c case evaluates @c keyform once, then dispatches on @c eql comparison
/// against each clause's (possibly list-valued) key set; a clause keyed by
/// the literal symbol `T` or `OTHERWISE` is the default and must be last.
/// Keys are literal data, never evaluated -- each is wrapped in a native
/// @ref reader::datum_quote node (@ref make_quote) before being compared.
/// Expands to a `let`-bound temporary plus a `cond` (itself a macro,
/// further expanded by the caller's recursive walk); shares the reserved
/// -binding-name divergence with @ref expand_or / @ref expand_cond
/// (`%CASE-TMP`).
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
expand_case(datum_list<MaxNodes, MaxList> const &call,
            datum_arena<MaxNodes, MaxList> &arena)
    -> smdscheme::foundation::result<datum<MaxNodes, MaxList>> {
    using DatumList = datum_list<MaxNodes, MaxList>;

    if (call.elements.size() < 2)
        return smdscheme::foundation::parse_error{{},
                                                  "case: expected a keyform"};

    auto keyform = arena.get(call.elements[1]);
    auto temp = make_symbol<MaxNodes, MaxList>("%CASE-TMP");

    int nclauses = call.elements.size() - 2;
    smdscheme::foundation::static_vector<datum<MaxNodes, MaxList>, MaxList>
        cond_form;
    cond_form.push_back(make_symbol<MaxNodes, MaxList>("COND"));

    for (int i = 0; i < nclauses; ++i) {
        auto const &clause_node = arena.get(call.elements[2 + i]);
        if (!std::holds_alternative<DatumList>(clause_node.inner))
            return smdscheme::foundation::parse_error{
                {}, "case: each clause must be a list"};
        auto const &clause = std::get<DatumList>(clause_node.inner);
        if (clause.elements.empty())
            return smdscheme::foundation::parse_error{
                {}, "case: clause must have a key"};
        if (clause.elements.size() < 2)
            return smdscheme::foundation::parse_error{
                {}, "case: clause must have at least one body expression"};

        auto const &key_node = arena.get(clause.elements[0]);

        bool is_default = false;
        if (std::holds_alternative<reader::datum_symbol>(key_node.inner)) {
            auto kname =
                std::get<reader::datum_symbol>(key_node.inner).name.view();
            if (kname == "T" || kname == "OTHERWISE")
                is_default = true;
        }

        if (is_default && i != nclauses - 1)
            return smdscheme::foundation::parse_error{
                {}, "case: T/OTHERWISE clause must be last"};

        datum<MaxNodes, MaxList> test;
        if (is_default) {
            test = true_symbol<MaxNodes, MaxList>();
        } else if (std::holds_alternative<DatumList>(key_node.inner)) {
            auto const &keys = std::get<DatumList>(key_node.inner);
            if (keys.elements.empty())
                return smdscheme::foundation::parse_error{
                    {}, "case: key list must not be empty"};
            smdscheme::foundation::static_vector<datum<MaxNodes, MaxList>,
                                                 MaxList>
                or_form;
            or_form.push_back(make_symbol<MaxNodes, MaxList>("OR"));
            for (int k = 0; k < keys.elements.size(); ++k) {
                auto key_copy = arena.get(keys.elements[k]);
                auto quoted = make_quote<MaxNodes, MaxList>(arena, key_copy);
                or_form.push_back(make_list_v<MaxNodes, MaxList>(
                    arena, make_symbol<MaxNodes, MaxList>("EQL"), temp,
                    quoted));
            }
            test = make_list_from<MaxNodes, MaxList>(arena, or_form);
        } else {
            auto quoted = make_quote<MaxNodes, MaxList>(arena, key_node);
            test = make_list_v<MaxNodes, MaxList>(
                arena, make_symbol<MaxNodes, MaxList>("EQL"), temp, quoted);
        }

        smdscheme::foundation::static_vector<datum<MaxNodes, MaxList>, MaxList>
            clause_form;
        clause_form.push_back(test);
        for (int j = 1; j < clause.elements.size(); ++j)
            clause_form.push_back(arena.get(clause.elements[j]));
        cond_form.push_back(
            make_list_from<MaxNodes, MaxList>(arena, clause_form));
    }

    auto cond_call = make_list_from<MaxNodes, MaxList>(arena, cond_form);

    auto binding = make_list_v<MaxNodes, MaxList>(arena, temp, keyform);
    auto bindings = make_list_v<MaxNodes, MaxList>(arena, binding);
    return make_list_v<MaxNodes, MaxList>(
        arena, make_symbol<MaxNodes, MaxList>("LET"), bindings, cond_call);
}

// -- BACKQUOTE (step L18) -----------------------------------------------

/// Forward declaration: @ref expand_backquote_template and
/// @ref expand_backquote_list_elems recurse into each other (a template
/// list element may itself hold a nested sub-template requiring the whole
/// lowering machinery again).
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
expand_backquote_template(datum<MaxNodes, MaxList> const &t,
                          datum_arena<MaxNodes, MaxList> &arena)
    -> smdscheme::foundation::result<datum<MaxNodes, MaxList>>;

/// Lowers the elements of a backquote template list, from @p idx onward,
/// into a `cons`/`append` construction chain.
///
/// Builds right-to-left: an ordinary element at @p idx lowers to
/// `(CONS <lowered element> <lowered rest>)`; an unquote-splicing element
/// (`,@x`, @ref reader::datum_unquote_splice) lowers to
/// `(APPEND x <lowered rest>)` instead, splicing `x`'s value (which must
/// itself be a list) onto the rest without an extra wrapping cons cell --
/// the whole reason `append` joins the builtin set in this step (plan
/// §9, L18). The base case (@p idx past the end of @p lst) is the literal
/// `NIL` symbol, the canonical empty list (decision D3).
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
/// @param  lst  The template list being lowered.
/// @param  idx  Index of the next element to process.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
expand_backquote_list_elems(datum_list<MaxNodes, MaxList> const &lst, int idx,
                            datum_arena<MaxNodes, MaxList> &arena)
    -> smdscheme::foundation::result<datum<MaxNodes, MaxList>> {
    using D = datum<MaxNodes, MaxList>;
    using DatumUnquoteSplice = reader::datum_unquote_splice<D, MaxNodes>;

    if (idx >= lst.elements.size())
        return nil_symbol<MaxNodes, MaxList>();

    auto const &elem = arena.get(lst.elements[idx]);

    auto rest_r =
        expand_backquote_list_elems<MaxNodes, MaxList>(lst, idx + 1, arena);
    if (!rest_r.has_value())
        return rest_r;
    auto const &rest = rest_r.value();

    if (std::holds_alternative<DatumUnquoteSplice>(elem.inner)) {
        auto const &us = std::get<DatumUnquoteSplice>(elem.inner);
        auto spliced = arena.get(us.target); // Code: evaluates to a list.
        return make_list_v<MaxNodes, MaxList>(
            arena, make_symbol<MaxNodes, MaxList>("APPEND"), spliced, rest);
    }

    auto elem_r = expand_backquote_template<MaxNodes, MaxList>(elem, arena);
    if (!elem_r.has_value())
        return elem_r;
    return make_list_v<MaxNodes, MaxList>(
        arena, make_symbol<MaxNodes, MaxList>("CONS"), elem_r.value(), rest);
}

/// Lowers one backquote template position @p t into code that constructs
/// the template's value.
///
/// - `,x` (@ref reader::datum_unquote) lowers to `x` itself: an unquote
///   escape names ordinary code, evaluated in place (this function only
///   strips the escape; @ref expand_datum's caller still recursively
///   expands `x` for any macro calls it contains).
/// - `,@x` (@ref reader::datum_unquote_splice) is an error here: splicing
///   is only meaningful as a list element (@ref
///   expand_backquote_list_elems handles that case directly, without
///   calling this function on the splice node itself), never as a whole
///   template or a non-list sub-template position.
/// - A list template lowers via @ref expand_backquote_list_elems.
/// - Anything else (an atom, a nested `` ` `` template, a `'x`/`#'x`
///   sub-datum) is literal data, wrapped in a native @ref
///   reader::datum_quote node via @ref make_quote exactly as written.
///   **Known simplification, see the DIV this step files:** a nested
///   backquote template is not itself lowered here -- ANSI CL's
///   nested-depth unquote tracking is out of scope for this step, so a
///   `` ` `` inside another `` ` `` is treated as opaque literal data
///   (its own `,`/`,@` escapes, if any, are quoted along with it, not
///   evaluated).
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
expand_backquote_template(datum<MaxNodes, MaxList> const &t,
                          datum_arena<MaxNodes, MaxList> &arena)
    -> smdscheme::foundation::result<datum<MaxNodes, MaxList>> {
    using D = datum<MaxNodes, MaxList>;
    using DatumList = datum_list<MaxNodes, MaxList>;
    using DatumUnquote = reader::datum_unquote<D, MaxNodes>;
    using DatumUnquoteSplice = reader::datum_unquote_splice<D, MaxNodes>;

    if (std::holds_alternative<DatumUnquote>(t.inner))
        return arena.get(std::get<DatumUnquote>(t.inner).target);

    if (std::holds_alternative<DatumUnquoteSplice>(t.inner))
        return smdscheme::foundation::parse_error{
            {}, "backquote: ,@ is only valid as a list element"};

    if (std::holds_alternative<DatumList>(t.inner))
        return expand_backquote_list_elems<MaxNodes, MaxList>(
            std::get<DatumList>(t.inner), 0, arena);

    return make_quote<MaxNodes, MaxList>(arena, t);
}

} // namespace detail

/// Maximum number of entries @ref default_macro_table holds (the six ANSI
/// CL macros this step implements).
inline constexpr int max_host_macros = 6;

/// The registry of host macros this step implements: `cond`, `when`,
/// `unless`, `and`, `or`, `case` -- exactly the six the plan calls out as
/// "never special operators in real Common Lisp, always macros defined in
/// terms of `if`/`progn`/etc." Every entry is an ordinary @ref host_macro
/// value; none of the six needed ad-hoc special-casing in the expander
/// itself (see docs/cl-pivot-plan.md step L17's explicit test of the
/// expander's shape).
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto default_macro_table()
    -> std::array<host_macro<MaxNodes, MaxList>, max_host_macros> {
    return {{
        {"COND", &detail::expand_cond<MaxNodes, MaxList>},
        {"WHEN", &detail::expand_when<MaxNodes, MaxList>},
        {"UNLESS", &detail::expand_unless<MaxNodes, MaxList>},
        {"AND", &detail::expand_and<MaxNodes, MaxList>},
        {"OR", &detail::expand_or<MaxNodes, MaxList>},
        {"CASE", &detail::expand_case<MaxNodes, MaxList>},
    }};
}

/// The result of one @ref macroexpand_1 step: the (possibly-replaced) node,
/// and whether a macro actually fired.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
struct expansion_step {
    datum<MaxNodes, MaxList> node;
    bool expanded{false};
};

/// One step of macro expansion applied at the head position of @p d.
///
/// If @p d is a list whose head is a symbol registered in @p table, calls
/// that macro's expansion function and returns the replacement with
/// @c expanded set; otherwise returns @p d unchanged with @c expanded
/// clear. Mirrors ANSI CL's `macroexpand-1`.
///
/// The lookup is an ordinary linear scan of @p table rather than a
/// find-then-compare-to-null helper: GCC16 rejects comparing a
/// function-pointer value obtained this way against @c nullptr inside a
/// manifestly-constant-evaluated context (`static_assert`/`constexpr`
/// evaluation of this whole call chain), so the match and the call happen
/// in the same loop iteration instead of being split across a lookup step
/// and a nullability check.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
macroexpand_1(datum<MaxNodes, MaxList> const &d,
              datum_arena<MaxNodes, MaxList> &arena,
              std::span<host_macro<MaxNodes, MaxList> const> table)
    -> smdscheme::foundation::result<expansion_step<MaxNodes, MaxList>> {
    using DatumList = datum_list<MaxNodes, MaxList>;

    if (!std::holds_alternative<DatumList>(d.inner))
        return expansion_step<MaxNodes, MaxList>{d, false};

    auto const &lst = std::get<DatumList>(d.inner);
    if (lst.elements.empty())
        return expansion_step<MaxNodes, MaxList>{d, false};

    auto const &head = arena.get(lst.elements[0]);
    if (!std::holds_alternative<reader::datum_symbol>(head.inner))
        return expansion_step<MaxNodes, MaxList>{d, false};

    auto name = std::get<reader::datum_symbol>(head.inner).name.view();
    for (auto const &m : table) {
        if (m.name != name)
            continue;
        auto expanded_r = m.expand(lst, arena);
        if (!expanded_r.has_value())
            return expanded_r.error();
        return expansion_step<MaxNodes, MaxList>{expanded_r.value(), true};
    }
    return expansion_step<MaxNodes, MaxList>{d, false};
}

/// @overload Uses @ref default_macro_table.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
macroexpand_1(datum<MaxNodes, MaxList> const &d,
              datum_arena<MaxNodes, MaxList> &arena)
    -> smdscheme::foundation::result<expansion_step<MaxNodes, MaxList>> {
    auto table = default_macro_table<MaxNodes, MaxList>();
    return macroexpand_1<MaxNodes, MaxList>(d, arena, table);
}

/// Applies @ref macroexpand_1 repeatedly at the head position of @p d until
/// it reports no further expansion (fixpoint), or until @p max_expansions
/// steps have been taken.
///
/// Budget exhaustion is a diagnosed @ref smdscheme::foundation::parse_error,
/// not a hang or an infinite loop, per the plan's explicit requirement.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
/// @param  max_expansions Iteration budget; see @ref default_max_expansions.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
macroexpand(datum<MaxNodes, MaxList> const &d,
            datum_arena<MaxNodes, MaxList> &arena,
            std::span<host_macro<MaxNodes, MaxList> const> table,
            int max_expansions = default_max_expansions)
    -> smdscheme::foundation::result<datum<MaxNodes, MaxList>> {
    datum<MaxNodes, MaxList> current = d;
    for (int i = 0; i < max_expansions; ++i) {
        auto step_r = macroexpand_1<MaxNodes, MaxList>(current, arena, table);
        if (!step_r.has_value())
            return step_r.error();
        if (!step_r.value().expanded)
            return current;
        current = step_r.value().node;
    }
    return smdscheme::foundation::parse_error{
        {}, "macroexpand: expansion budget exceeded"};
}

/// @overload Uses @ref default_macro_table.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto macroexpand(datum<MaxNodes, MaxList> const &d,
                                         datum_arena<MaxNodes, MaxList> &arena)
    -> smdscheme::foundation::result<datum<MaxNodes, MaxList>> {
    auto table = default_macro_table<MaxNodes, MaxList>();
    return macroexpand<MaxNodes, MaxList>(d, arena, table);
}

// -- DEFMACRO (step L19) -----------------------------------------------
//
// A `defmacro`-defined macro is not a C++ function like @ref host_macro:
// its expander is Lisp code, compiled and run by the SAME evaluator
// machinery (@ref elaborator::elaborate / @ref closure::eval_direct) an
// ordinary program runs through -- "the compiler running the language it
// compiles, at compile time" (plan step L19). Two new pieces of machinery
// make that possible:
//
//  - Reification (`detail::datum_to_value` / `detail::value_to_datum`):
//    a macro call's raw argument data is a @ref datum (unevaluated code),
//    but the compile-time evaluator only understands @ref closure::value.
//    `datum_to_value` converts a call form's argument data into a value
//    the macro's compiled closure can be applied to; `value_to_datum`
//    converts the closure's return value (new code, as data) back into a
//    datum so @ref expand_datum can recursively expand it exactly like
//    any other macro's replacement code.
//  - @ref macro_context: the mutable, per-expansion-pass state
//    (registered macros, plus a private pair heap / env arena / eval
//    environment) `defmacro` needs, threaded through @ref expand_datum's
//    recursion the same way @p arena already is.

/// Maximum number of distinct `defmacro` names one @ref macro_context can
/// register.
inline constexpr int max_user_macros = 16;

/// One `defmacro`-registered macro: a folded name, the arity/rest shape
/// @ref invoke_user_macro needs to slice a call form's raw arguments, and
/// the materialized compile-time closure that implements the expansion.
///
/// @ref lambda_node and @ref macro_value are set together, once, when the
/// `defmacro` form is processed (@ref detail::process_defmacro):
/// @ref lambda_node holds the elaborated `(lambda (params...) body...)`
/// node in stable, arena-owned storage (a @ref macro_context's @c table is
/// a @ref smdscheme::foundation::static_vector, which never reallocates),
/// and @ref macro_value is a @ref closure::closure value whose @c node
/// pointer targets @ref lambda_node in that same stable slot -- the same
/// "closure captures a stable arena address, never a stack local" pattern
/// @ref closure::env_arena documents.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
struct user_macro {
    std::string_view name{}; ///< Folded spelling; arena-backed (the
                             ///< `defmacro` form's own name argument is
                             ///< always a list element, never an
                             ///< elaboration root -- see @ref
                             ///< elaborator::core_defun's identical
                             ///< reasoning).
    int required_count = 0;  ///< Number of fixed positional parameters.
    bool has_rest = false;   ///< True if the lambda list ended in
                             ///< `&rest`/`&body name`.
    elaborator::core_type<MaxNodes, MaxList>
        lambda_node{}; ///< The
                       ///< elaborated macro-expander lambda; stable storage for
                       ///< @ref macro_value's closure node pointer.
    closure::value<elaborator::core_type<MaxNodes, MaxList>> macro_value{};
    ///< The materialized closure @ref invoke_user_macro applies.
};

/// Mutable, per-expansion-pass state `defmacro` needs: the registry of
/// user-defined macros, plus the compile-time evaluator plumbing (a
/// private @ref closure::pair_heap, @ref closure::env_arena, and a
/// @ref closure::env sharing that heap and pre-populated with the default
/// builtins) used to elaborate and run a macro body -- entirely separate
/// from whatever pair heap / environment the REAL program later runs
/// under (@ref expand_and_elaborate's caller supplies those independently
/// for the elaborated, expanded result).
///
/// One @ref macro_context is constructed fresh per top-level
/// @ref expand_and_elaborate (or context-less @ref expand_datum) call:
/// `defmacro` registrations are visible for the rest of THAT call's
/// recursive expansion, never across separate top-level forms -- see
/// docs/divergences/DIV-0010 for why this is a divergence from ANSI CL's
/// session-global `defmacro`, and why it does not affect this step's
/// merge criteria (a `defmacro` and its use sites live in one top-level
/// form in every test this step adds).
///
/// @tparam MaxNodes    Arena capacity.
/// @tparam MaxList     Maximum list length.
/// @tparam MaxBindings Environment capacity for the compile-time
///                     evaluator; must be 16 (@ref closure::eval_direct's
///                     constraint).
/// @tparam MaxEnvs     Capacity of the compile-time @ref closure::env_arena.
// ee158407-41ed-4add-9a40-3ce719f057e6
template <int MaxNodes, int MaxList, int MaxBindings = 16,
          int MaxEnvs = closure::default_max_envs>
class macro_context {
  public:
    using Core = elaborator::core_type<MaxNodes, MaxList>;

    smdscheme::foundation::static_vector<user_macro<MaxNodes, MaxList>,
                                         max_user_macros>
        table{}; ///< Registered `defmacro` macros, in definition order.
    smdscheme::foundation::tree_arena<Core, MaxNodes>
        core_arena{}; ///< Compile-time-only core arena for macro bodies;
                      ///< never the real program's own core arena.
    closure::pair_heap<Core, closure::default_max_pairs>
        heap{}; ///< Compile-time-only pair heap (reification and macro
                ///< body `cons`/`append` evaluation both allocate here).
    closure::env_arena<Core, MaxBindings, MaxEnvs>
        envs{}; ///< Owns every environment a macro-body lambda captures.
    closure::env<Core, MaxBindings> env{
        closure::default_env<Core, MaxBindings>(heap)}; ///< The
    ///< compile-time evaluation environment (default builtins only).
};
// ee158407-41ed-4add-9a40-3ce719f057e6 end

/// Forward declaration: @ref expand_datum (the @ref macro_context-aware
/// overload) is defined later in this file, but
/// @ref detail::process_defmacro needs to recurse into it (a macro body
/// may itself use backquote, host macros, or a previously-registered
/// `defmacro`).
///
/// @tparam MaxNodes    Arena capacity.
/// @tparam MaxList     Maximum list length.
/// @tparam MaxBindings @ref macro_context's environment capacity.
/// @tparam MaxEnvs     @ref macro_context's env-arena capacity.
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs>
[[nodiscard]] constexpr auto
expand_datum(datum<MaxNodes, MaxList> const &d,
             datum_arena<MaxNodes, MaxList> &arena,
             macro_context<MaxNodes, MaxList, MaxBindings, MaxEnvs> &ctx)
    -> smdscheme::foundation::result<datum<MaxNodes, MaxList>>;

namespace detail {

/// Reifies a raw (unevaluated) datum -- an argument form at a `defmacro`
/// call site -- into a @ref closure::value the compile-time evaluator can
/// bind a macro parameter to.
///
/// Mirrors @ref elaborator::detail::elaborate_quoted_datum's NIL
/// special-casing (decision D3), so a macro parameter observes the exact
/// same `nil` identity ordinary quoted data does; `T` needs no special
/// case; it is already an ordinary symbol value with that spelling.
/// `'x` and `#'x` reify as their two-element list spellings (`(QUOTE x)` /
/// `(FUNCTION x)`) -- the ANSI CL identity between the reader shorthand
/// and the list form holds at the data level exactly as it does at the
/// reader level. A raw `` ` ``/`,`/`,@` datum reaching this function (a
/// macro call argument that is itself unexpanded backquote syntax) is a
/// diagnosed error rather than a structural reification -- see
/// docs/divergences/DIV-0010.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
/// @param  d     The argument datum to reify.
/// @param  arena Read-only datum arena backing @p d.
/// @param  heap  Compile-time pair heap; list data is allocated here.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
datum_to_value(datum<MaxNodes, MaxList> const &d,
               datum_arena<MaxNodes, MaxList> const &arena,
               closure::pair_heap<elaborator::core_type<MaxNodes, MaxList>,
                                  closure::default_max_pairs> &heap)
    -> smdscheme::foundation::result<
        closure::value<elaborator::core_type<MaxNodes, MaxList>>> {
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    using Val = closure::value<Core>;
    using DatumT = datum<MaxNodes, MaxList>;
    using DList = datum_list<MaxNodes, MaxList>;
    using DatumQuote = reader::datum_quote<DatumT, MaxNodes>;
    using DatumFunction = reader::datum_function<DatumT, MaxNodes>;
    using DatumBackquote = reader::datum_backquote<DatumT, MaxNodes>;
    using DatumUnquote = reader::datum_unquote<DatumT, MaxNodes>;
    using DatumUnquoteSplice = reader::datum_unquote_splice<DatumT, MaxNodes>;

    if (std::holds_alternative<reader::datum_integer>(d.inner))
        return Val{std::get<reader::datum_integer>(d.inner).value};

    if (std::holds_alternative<reader::datum_symbol>(d.inner)) {
        auto name = std::get<reader::datum_symbol>(d.inner).name.view();
        if (name == "NIL")
            return Val{closure::nil_t{}};
        return Val{closure::symbol{name}};
    }

    if (std::holds_alternative<reader::datum_keyword>(d.inner))
        return Val{closure::keyword{
            std::get<reader::datum_keyword>(d.inner).name.view()}};

    if (std::holds_alternative<DList>(d.inner)) {
        auto const &lst = std::get<DList>(d.inner);
        Val acc{closure::nil_t{}};
        for (int i = lst.elements.size() - 1; i >= 0; --i) {
            auto elem_r = datum_to_value<MaxNodes, MaxList>(
                arena.get(lst.elements[i]), arena, heap);
            if (!elem_r.has_value())
                return elem_r;
            acc = Val{closure::pair_ref{
                heap.alloc(closure::pair_cell<Core>{elem_r.value(), acc})}};
        }
        return acc;
    }

    if (std::holds_alternative<DatumQuote>(d.inner)) {
        auto const &q = std::get<DatumQuote>(d.inner);
        auto target_r =
            datum_to_value<MaxNodes, MaxList>(arena.get(q.quoted), arena, heap);
        if (!target_r.has_value())
            return target_r;
        Val tail{closure::pair_ref{heap.alloc(closure::pair_cell<Core>{
            target_r.value(), Val{closure::nil_t{}}})}};
        return Val{closure::pair_ref{heap.alloc(
            closure::pair_cell<Core>{Val{closure::symbol{"QUOTE"}}, tail})}};
    }

    if (std::holds_alternative<DatumFunction>(d.inner)) {
        auto const &fq = std::get<DatumFunction>(d.inner);
        auto target_r = datum_to_value<MaxNodes, MaxList>(arena.get(fq.target),
                                                          arena, heap);
        if (!target_r.has_value())
            return target_r;
        Val tail{closure::pair_ref{heap.alloc(closure::pair_cell<Core>{
            target_r.value(), Val{closure::nil_t{}}})}};
        return Val{closure::pair_ref{heap.alloc(
            closure::pair_cell<Core>{Val{closure::symbol{"FUNCTION"}}, tail})}};
    }

    if (std::holds_alternative<DatumBackquote>(d.inner) ||
        std::holds_alternative<DatumUnquote>(d.inner) ||
        std::holds_alternative<DatumUnquoteSplice>(d.inner))
        return smdscheme::foundation::parse_error{
            {},
            "defmacro: raw backquote syntax in macro-argument position "
            "is not supported"};

    return smdscheme::foundation::parse_error{
        {}, "defmacro: unsupported argument datum"};
}

/// Reifies a compile-time-evaluator @ref closure::value -- the result of
/// applying a `defmacro`'s expander closure -- back into a datum, so
/// @ref expand_datum can recursively expand it exactly like any other
/// macro's replacement code (host macro or backquote lowering). New nodes
/// are written into @p arena, the same reader-populated arena macro
/// expansion always writes into (decision D9's "datum-to-datum pass").
///
/// A pair chain that does not terminate in @ref closure::nil_t is a
/// diagnosed error: the datum layer has no dotted-list representation
/// (decision D6), matching the reader's own restriction.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
/// @param  v     The value to reify.
/// @param  arena Datum arena receiving any new nodes.
/// @param  heap  Compile-time pair heap backing @p v's pair cells, if any.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto value_to_datum(
    closure::value<elaborator::core_type<MaxNodes, MaxList>> const &v,
    datum_arena<MaxNodes, MaxList> &arena,
    closure::pair_heap<elaborator::core_type<MaxNodes, MaxList>,
                       closure::default_max_pairs> const &heap)
    -> smdscheme::foundation::result<datum<MaxNodes, MaxList>> {
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    using D = datum<MaxNodes, MaxList>;
    using datum_f =
        typename reader::datum_f_factory<MaxNodes, MaxList>::template type<D>;
    using DList = datum_list<MaxNodes, MaxList>;
    using Res = smdscheme::foundation::result<D>;

    return std::visit(
        smd::fixpoint::overloaded{
            [&](closure::nil_t const &) -> Res {
                return nil_symbol<MaxNodes, MaxList>();
            },
            [&](int i) -> Res { return D{datum_f{reader::datum_integer{i}}}; },
            [&](closure::symbol const &s) -> Res {
                return make_symbol<MaxNodes, MaxList>(s.name);
            },
            [&](closure::keyword const &k) -> Res {
                return D{datum_f{
                    reader::datum_keyword{reader::detail::fold(k.name)}}};
            },
            [&](closure::pair_ref const &p) -> Res {
                DList lst{};
                closure::value<Core> cur{p};
                while (std::holds_alternative<closure::pair_ref>(cur)) {
                    auto const &cell =
                        heap.get(std::get<closure::pair_ref>(cur).loc);
                    auto elem_r = value_to_datum<MaxNodes, MaxList>(
                        cell.car, arena, heap);
                    if (!elem_r.has_value())
                        return elem_r;
                    lst.elements.push_back(
                        smdscheme::foundation::make_arena_box(arena,
                                                              elem_r.value()));
                    cur = cell.cdr;
                }
                if (!std::holds_alternative<closure::nil_t>(cur))
                    return smdscheme::foundation::parse_error{
                        {},
                        "defmacro: macro expansion produced an improper "
                        "list (no dotted-list datum representation, "
                        "decision D6)"};
                return D{datum_f{std::move(lst)}};
            },
            [&](closure::builtin const &) -> Res {
                return smdscheme::foundation::parse_error{
                    {},
                    "defmacro: macro expansion produced a builtin value, "
                    "not data"};
            },
            [&](closure::closure<Core> const &) -> Res {
                return smdscheme::foundation::parse_error{
                    {},
                    "defmacro: macro expansion produced a closure value, "
                    "not data"};
            },
            [&](closure::foreign_function<Core> const &) -> Res {
                return smdscheme::foundation::parse_error{
                    {},
                    "defmacro: macro expansion produced a foreign-"
                    "function value, not data"};
            }},
        v);
}

/// Parsed shape of a `defmacro` lambda list: `(p1 p2 ... [&rest|&body
/// rest-name])`.
///
/// **Known simplification (docs/divergences/DIV-0010):** this project's
/// macro lambda lists support only a run of required, fixed-position
/// parameters, optionally followed by exactly one `&rest`/`&body` name
/// capturing the remaining raw call arguments as a list -- not full ANSI
/// CL destructuring macro lambda lists (`&optional`/`&key`/`&aux`/nested
/// destructuring are unsupported). `&body` is accepted as a spelling
/// synonym for `&rest`: ANSI CL distinguishes them only for
/// pretty-printer indentation, which this pipeline has no use for.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
struct macro_formals {
    /// Formal-parameter datum handles, in `lambda` order: required
    /// parameters first, then the rest-name (if @ref has_rest) -- reused
    /// directly from the `defmacro` form's own formals list, never
    /// copied.
    smdscheme::foundation::static_vector<
        smdscheme::foundation::arena_box<datum<MaxNodes, MaxList>, MaxNodes>,
        MaxList>
        plain_params{};
    int required_count = 0; ///< Number of entries in @ref plain_params
                            ///< before the rest-name, if any.
    bool has_rest = false;  ///< True if the lambda list ended in
                            ///< `&rest`/`&body name`.
};

/// Parses @p formals per @ref macro_formals's grammar.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
parse_macro_formals(datum_list<MaxNodes, MaxList> const &formals,
                    datum_arena<MaxNodes, MaxList> const &arena)
    -> smdscheme::foundation::result<macro_formals<MaxNodes, MaxList>> {
    macro_formals<MaxNodes, MaxList> out{};
    for (int i = 0; i < formals.elements.size(); ++i) {
        auto const &p = arena.get(formals.elements[i]);
        if (!std::holds_alternative<reader::datum_symbol>(p.inner))
            return smdscheme::foundation::parse_error{
                {}, "defmacro: formal must be a symbol"};
        auto pname = std::get<reader::datum_symbol>(p.inner).name.view();
        if (pname == "&REST" || pname == "&BODY") {
            if (i + 2 != formals.elements.size())
                return smdscheme::foundation::parse_error{
                    {},
                    "defmacro: &rest/&body must be followed by exactly "
                    "one name, with nothing after it"};
            auto const &rest_node = arena.get(formals.elements[i + 1]);
            if (!std::holds_alternative<reader::datum_symbol>(rest_node.inner))
                return smdscheme::foundation::parse_error{
                    {}, "defmacro: &rest/&body name must be a symbol"};
            out.plain_params.push_back(formals.elements[i + 1]);
            out.has_rest = true;
            return out;
        }
        out.plain_params.push_back(formals.elements[i]);
        ++out.required_count;
    }
    return out;
}

/// True if @p lst is headed by the symbol `DEFMACRO`.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
is_defmacro_form(datum_list<MaxNodes, MaxList> const &lst,
                 datum_arena<MaxNodes, MaxList> const &arena) -> bool {
    if (lst.elements.empty())
        return false;
    auto const &head = arena.get(lst.elements[0]);
    return std::holds_alternative<reader::datum_symbol>(head.inner) &&
           std::get<reader::datum_symbol>(head.inner).name.view() == "DEFMACRO";
}

/// Processes a `(DEFMACRO name (formals...) body...)` form: parses and
/// validates the formals (@ref parse_macro_formals), builds a synthetic
/// `(LAMBDA (plain-params...) body...)` datum from the macro's own body
/// forms and formal names (reused verbatim, never copied), macro-expands
/// it (so a macro body may itself use backquote, host macros, or a
/// previously-registered `defmacro` -- @ref expand_datum), elaborates the
/// expanded lambda (@ref elaborator::elaborate) into @p ctx's private
/// compile-time core arena, and evaluates it (@ref closure::eval_direct)
/// in @p ctx's compile-time environment to materialize the closure
/// @ref invoke_user_macro will later apply. Registers the result in
/// @p ctx.table.
///
/// Per ANSI CL, `defmacro` returns the macro name; since this step has no
/// elaborator-level representation for "a compile-time-only definition"
/// (unlike `defun`/`defvar`, `defmacro` leaves nothing for the real
/// program's evaluator to run), the form is fully consumed here and
/// replaced with a quoted-symbol datum naming the macro -- see
/// docs/divergences/DIV-0010.
///
/// @tparam MaxNodes    Arena capacity.
/// @tparam MaxList     Maximum list length.
/// @tparam MaxBindings @ref macro_context's environment capacity.
/// @tparam MaxEnvs     @ref macro_context's env-arena capacity.
// 76bd8411-361f-4baa-9464-18aebd4128ce
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs>
[[nodiscard]] constexpr auto
process_defmacro(datum_list<MaxNodes, MaxList> const &call,
                 datum_arena<MaxNodes, MaxList> &arena,
                 macro_context<MaxNodes, MaxList, MaxBindings, MaxEnvs> &ctx)
    -> smdscheme::foundation::result<datum<MaxNodes, MaxList>> {
    using DatumT = datum<MaxNodes, MaxList>;
    using DList = datum_list<MaxNodes, MaxList>;
    using datum_f =
        typename reader::datum_f_factory<MaxNodes,
                                         MaxList>::template type<DatumT>;

    if (call.elements.size() < 3)
        return smdscheme::foundation::parse_error{
            {},
            "defmacro: expected a name, formals, and at least one body "
            "expression"};

    auto const &name_node = arena.get(call.elements[1]);
    if (!std::holds_alternative<reader::datum_symbol>(name_node.inner))
        return smdscheme::foundation::parse_error{
            {}, "defmacro: name must be a symbol"};
    auto macro_name =
        std::get<reader::datum_symbol>(name_node.inner).name.view();

    auto const &formals_node = arena.get(call.elements[2]);
    if (!std::holds_alternative<DList>(formals_node.inner))
        return smdscheme::foundation::parse_error{
            {}, "defmacro: formals must be a list"};

    auto formals_r = parse_macro_formals<MaxNodes, MaxList>(
        std::get<DList>(formals_node.inner), arena);
    if (!formals_r.has_value())
        return formals_r.error();
    auto const &formals = formals_r.value();

    DList lambda_formals{};
    for (int i = 0; i < formals.plain_params.size(); ++i)
        lambda_formals.elements.push_back(formals.plain_params[i]);

    DList lambda_form{};
    lambda_form.elements.push_back(smdscheme::foundation::make_arena_box(
        arena, make_symbol<MaxNodes, MaxList>("LAMBDA")));
    lambda_form.elements.push_back(smdscheme::foundation::make_arena_box(
        arena, DatumT{datum_f{std::move(lambda_formals)}}));
    for (int i = 3; i < call.elements.size(); ++i)
        lambda_form.elements.push_back(call.elements[i]);

    auto lambda_datum = DatumT{datum_f{std::move(lambda_form)}};

    auto expanded_lambda_r =
        expand_datum<MaxNodes, MaxList, MaxBindings, MaxEnvs>(lambda_datum,
                                                              arena, ctx);
    if (!expanded_lambda_r.has_value())
        return expanded_lambda_r;

    auto core_r = elaborator::elaborate<MaxNodes, MaxList>(
        expanded_lambda_r.value(), arena, ctx.core_arena);
    if (!core_r.has_value())
        return core_r.error();

    if (ctx.table.size() >= max_user_macros)
        return smdscheme::foundation::parse_error{
            {}, "defmacro: too many macros registered in this expansion"};

    user_macro<MaxNodes, MaxList> um{};
    um.name = macro_name;
    um.required_count = formals.required_count;
    um.has_rest = formals.has_rest;
    um.lambda_node = core_r.value();
    ctx.table.push_back(std::move(um));
    auto &entry = ctx.table[ctx.table.size() - 1];

    auto clo_r = closure::eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
        entry.lambda_node, ctx.core_arena, ctx.env, ctx.envs);
    if (!clo_r.has_value())
        return clo_r.error();
    entry.macro_value = clo_r.value();

    return make_quote<MaxNodes, MaxList>(
        arena, make_symbol<MaxNodes, MaxList>(macro_name));
}
// 76bd8411-361f-4baa-9464-18aebd4128ce end

} // namespace detail

/// Applies a registered `defmacro` macro @p m to a call form @p call:
/// slices @p call's raw argument data into @p m's required positional
/// arguments plus (if @ref user_macro::has_rest) a trailing raw-argument
/// list, reifies each into a @ref closure::value (@ref
/// detail::datum_to_value), applies @p m's compiled closure (@ref
/// closure::apply_function_value), and reifies the resulting value back
/// into a datum (@ref detail::value_to_datum) for the caller to
/// recursively expand.
///
/// @tparam MaxNodes    Arena capacity.
/// @tparam MaxList     Maximum list length.
/// @tparam MaxBindings @ref macro_context's environment capacity.
/// @tparam MaxEnvs     @ref macro_context's env-arena capacity.
// 9d3bc54e-ef26-4b12-9471-0ad05344ff20
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs>
[[nodiscard]] constexpr auto
invoke_user_macro(user_macro<MaxNodes, MaxList> const &m,
                  datum_list<MaxNodes, MaxList> const &call,
                  datum_arena<MaxNodes, MaxList> &arena,
                  macro_context<MaxNodes, MaxList, MaxBindings, MaxEnvs> &ctx)
    -> smdscheme::foundation::result<datum<MaxNodes, MaxList>> {
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    using Val = closure::value<Core>;

    int nargs = call.elements.size() - 1;
    if (m.has_rest) {
        if (nargs < m.required_count)
            return smdscheme::foundation::parse_error{
                {}, "defmacro: too few arguments in macro call"};
    } else if (nargs != m.required_count) {
        return smdscheme::foundation::parse_error{
            {}, "defmacro: wrong number of arguments in macro call"};
    }

    smdscheme::foundation::static_vector<Val, MaxList> args{};
    for (int i = 0; i < m.required_count; ++i) {
        auto v_r = detail::datum_to_value<MaxNodes, MaxList>(
            arena.get(call.elements[1 + i]), arena, ctx.heap);
        if (!v_r.has_value())
            return v_r.error();
        args.push_back(v_r.value());
    }
    if (m.has_rest) {
        Val rest_val{closure::nil_t{}};
        for (int i = call.elements.size() - 1; i >= 1 + m.required_count; --i) {
            auto v_r = detail::datum_to_value<MaxNodes, MaxList>(
                arena.get(call.elements[i]), arena, ctx.heap);
            if (!v_r.has_value())
                return v_r.error();
            rest_val = Val{closure::pair_ref{ctx.heap.alloc(
                closure::pair_cell<Core>{v_r.value(), rest_val})}};
        }
        args.push_back(rest_val);
    }

    auto result_r =
        closure::apply_function_value<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            m.macro_value, std::span<Val const>(args.begin(), args.end()),
            ctx.core_arena, &ctx.heap, ctx.envs);
    if (!result_r.has_value())
        return result_r.error();

    return detail::value_to_datum<MaxNodes, MaxList>(result_r.value(), arena,
                                                     ctx.heap);
}
// 9d3bc54e-ef26-4b12-9471-0ad05344ff20 end

/// One step of macro expansion at the head position of @p d, trying host
/// macros first (@ref macroexpand_1) and, failing that, `defmacro`
/// -registered user macros in @p ctx.table (@ref invoke_user_macro).
///
/// @tparam MaxNodes    Arena capacity.
/// @tparam MaxList     Maximum list length.
/// @tparam MaxBindings @ref macro_context's environment capacity.
/// @tparam MaxEnvs     @ref macro_context's env-arena capacity.
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs>
[[nodiscard]] constexpr auto macroexpand_with_macros_1(
    datum<MaxNodes, MaxList> const &d, datum_arena<MaxNodes, MaxList> &arena,
    macro_context<MaxNodes, MaxList, MaxBindings, MaxEnvs> &ctx)
    -> smdscheme::foundation::result<expansion_step<MaxNodes, MaxList>> {
    auto host_r = macroexpand_1<MaxNodes, MaxList>(d, arena);
    if (!host_r.has_value())
        return host_r.error();
    if (host_r.value().expanded)
        return host_r.value();

    using DList = datum_list<MaxNodes, MaxList>;
    if (!std::holds_alternative<DList>(d.inner))
        return expansion_step<MaxNodes, MaxList>{d, false};
    auto const &lst = std::get<DList>(d.inner);
    if (lst.elements.empty())
        return expansion_step<MaxNodes, MaxList>{d, false};
    auto const &head = arena.get(lst.elements[0]);
    if (!std::holds_alternative<reader::datum_symbol>(head.inner))
        return expansion_step<MaxNodes, MaxList>{d, false};
    auto name = std::get<reader::datum_symbol>(head.inner).name.view();

    for (int i = 0; i < ctx.table.size(); ++i) {
        if (ctx.table[i].name != name)
            continue;
        auto rep_r = invoke_user_macro<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            ctx.table[i], lst, arena, ctx);
        if (!rep_r.has_value())
            return rep_r.error();
        return expansion_step<MaxNodes, MaxList>{rep_r.value(), true};
    }
    return expansion_step<MaxNodes, MaxList>{d, false};
}

/// Applies @ref macroexpand_with_macros_1 repeatedly at the head position
/// of @p d until fixpoint or @p max_expansions steps, exactly mirroring
/// @ref macroexpand's budget discipline ("budget exhaustion is a
/// diagnosed error, not a hang") for the combined host-macro/user-macro
/// expansion loop.
///
/// @tparam MaxNodes    Arena capacity.
/// @tparam MaxList     Maximum list length.
/// @tparam MaxBindings @ref macro_context's environment capacity.
/// @tparam MaxEnvs     @ref macro_context's env-arena capacity.
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs>
[[nodiscard]] constexpr auto macroexpand_with_macros(
    datum<MaxNodes, MaxList> const &d, datum_arena<MaxNodes, MaxList> &arena,
    macro_context<MaxNodes, MaxList, MaxBindings, MaxEnvs> &ctx,
    int max_expansions = default_max_expansions)
    -> smdscheme::foundation::result<datum<MaxNodes, MaxList>> {
    datum<MaxNodes, MaxList> current = d;
    for (int i = 0; i < max_expansions; ++i) {
        auto step_r =
            macroexpand_with_macros_1<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                current, arena, ctx);
        if (!step_r.has_value())
            return step_r.error();
        if (!step_r.value().expanded)
            return current;
        current = step_r.value().node;
    }
    return smdscheme::foundation::parse_error{
        {}, "macroexpand: expansion budget exceeded"};
}

namespace detail {

/// True if @p lst is a `(QUOTE datum)` form written out as an ordinary
/// list, the ANSI-equivalent long spelling of the reader's `'datum`
/// shorthand (@ref reader::datum_quote). @ref expand_datum treats both
/// spellings identically: quoted data is literal and must not be
/// macro-expanded or recursed into, matching ANSI CL's rule that `quote`
/// suppresses macroexpansion of its argument.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
is_quote_form(datum_list<MaxNodes, MaxList> const &lst,
              datum_arena<MaxNodes, MaxList> const &arena) -> bool {
    if (lst.elements.empty())
        return false;
    auto const &head = arena.get(lst.elements[0]);
    return std::holds_alternative<reader::datum_symbol>(head.inner) &&
           std::get<reader::datum_symbol>(head.inner).name.view() == "QUOTE";
}

} // namespace detail

/// Recursively expands every macro call anywhere in the datum tree rooted
/// at @p d, writing replacement nodes into @p arena (the same arena the
/// reader populated -- decision D9's "datum-to-datum pass", the simplest
/// integration: expansion output lives in the same arena its input came
/// from).
///
/// This is the whole expander pass, not just @ref macroexpand_1/
/// @ref macroexpand applied once: macro calls are expanded wherever they
/// appear -- inside `lambda` bodies, `if` branches, nested macro
/// expansions, anywhere -- except inside quoted data (@ref
/// reader::datum_quote, or the equivalent `(quote ...)` list form, see
/// @ref detail::is_quote_form), which ANSI CL treats as literal and exempt
/// from macroexpansion. `#'` (@ref reader::datum_function) targets ARE
/// recursed into, since a `#'(lambda ...)` target is code. `` ` `` (@ref
/// reader::datum_backquote) templates are lowered to `cons`/`list`/`append`
/// construction code (@ref detail::expand_backquote_template, step L18)
/// before being recursed into, so the code embedded via `,`/`,@` escapes is
/// still fully expanded.
///
/// At each list position, the head is expanded to fixpoint first (@ref
/// macroexpand); the algorithm never re-applies @ref macroexpand to the
/// same node twice, so composing macros (e.g. `case` expanding into a
/// `let`/`cond` combination, whose `cond` is itself expanded on the next
/// recursive step) terminates without needing a second, outer fixpoint
/// loop across the whole tree.
///
/// This overload is @ref macro_context-aware (step L19): `defmacro` forms
/// are recognized directly by head spelling (@ref detail::is_defmacro_form),
/// like backquote is recognized by datum kind -- registering a macro is a
/// side effect on @p ctx, not "rewrite this call into replacement code",
/// so it does not go through @ref macroexpand_with_macros at all. Every
/// other list position tries host macros then `defmacro`-registered user
/// macros, in that order, via @ref macroexpand_with_macros.
///
/// @tparam MaxNodes    Arena capacity.
/// @tparam MaxList     Maximum list length.
/// @tparam MaxBindings @ref macro_context's environment capacity.
/// @tparam MaxEnvs     @ref macro_context's env-arena capacity.
/// @param  d     Root datum to expand.
/// @param  arena Datum arena backing @p d; also receives every node this
///               pass allocates.
/// @param  ctx   Mutable `defmacro` registry and compile-time evaluator
///               state, threaded through the whole recursive walk.
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs>
[[nodiscard]] constexpr auto
expand_datum(datum<MaxNodes, MaxList> const &d,
             datum_arena<MaxNodes, MaxList> &arena,
             macro_context<MaxNodes, MaxList, MaxBindings, MaxEnvs> &ctx)
    -> smdscheme::foundation::result<datum<MaxNodes, MaxList>> {
    using D = datum<MaxNodes, MaxList>;
    using datum_f =
        typename reader::datum_f_factory<MaxNodes, MaxList>::template type<D>;
    using DatumList = datum_list<MaxNodes, MaxList>;
    using DatumQuote = reader::datum_quote<D, MaxNodes>;
    using DatumFunction = reader::datum_function<D, MaxNodes>;
    using DatumBackquote = reader::datum_backquote<D, MaxNodes>;

    if (std::holds_alternative<DatumQuote>(d.inner))
        return d;

    if (std::holds_alternative<DatumFunction>(d.inner)) {
        auto const &fq = std::get<DatumFunction>(d.inner);
        auto inner_r = expand_datum<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            arena.get(fq.target), arena, ctx);
        if (!inner_r.has_value())
            return inner_r;
        return D{datum_f{DatumFunction{
            smdscheme::foundation::make_arena_box(arena, inner_r.value())}}};
    }

    if (std::holds_alternative<DatumBackquote>(d.inner)) {
        // A dedicated template-datum case (plan §9, L18), not a host_macro
        // entry: `` ` `` is reader syntax, never a symbol-headed call form
        // a host_macro could match. Lower the template to `cons`/`list`/
        // `append` construction code (@ref detail::expand_backquote_template),
        // then recurse so any code embedded via `,`/`,@` escapes -- and any
        // macro calls it contains -- is still fully expanded, exactly like
        // every other macro expansion's replacement datum.
        auto const &bq = std::get<DatumBackquote>(d.inner);
        auto lowered_r = detail::expand_backquote_template<MaxNodes, MaxList>(
            arena.get(bq.templ), arena);
        if (!lowered_r.has_value())
            return lowered_r;
        return expand_datum<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            lowered_r.value(), arena, ctx);
    }

    if (!std::holds_alternative<DatumList>(d.inner))
        return d; // integer, symbol, keyword: nothing to expand.

    if (detail::is_quote_form<MaxNodes, MaxList>(std::get<DatumList>(d.inner),
                                                 arena))
        return d;

    if (detail::is_defmacro_form<MaxNodes, MaxList>(
            std::get<DatumList>(d.inner), arena))
        return detail::process_defmacro<MaxNodes, MaxList, MaxBindings,
                                        MaxEnvs>(std::get<DatumList>(d.inner),
                                                 arena, ctx);

    auto expanded_r =
        macroexpand_with_macros<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            d, arena, ctx);
    if (!expanded_r.has_value())
        return expanded_r;
    auto const &expanded = expanded_r.value();

    if (!std::holds_alternative<DatumList>(expanded.inner))
        return expand_datum<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            expanded, arena, ctx);

    auto const &lst = std::get<DatumList>(expanded.inner);
    if (detail::is_quote_form<MaxNodes, MaxList>(lst, arena))
        return expanded;

    DatumList new_lst{};
    for (int i = 0; i < lst.elements.size(); ++i) {
        auto elem_r = expand_datum<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            arena.get(lst.elements[i]), arena, ctx);
        if (!elem_r.has_value())
            return elem_r;
        new_lst.elements.push_back(
            smdscheme::foundation::make_arena_box(arena, elem_r.value()));
    }
    return D{datum_f{std::move(new_lst)}};
}

/// @overload Uses a fresh, local @ref macro_context: `defmacro`
/// registrations made while expanding @p d are visible only within this
/// one call (see docs/divergences/DIV-0010). Existing callers that never
/// use `defmacro` (host-macro and backquote expansion) are unaffected.
///
/// @tparam MaxNodes    Arena capacity.
/// @tparam MaxList     Maximum list length.
/// @tparam MaxBindings @ref macro_context's environment capacity.
/// @tparam MaxEnvs     @ref macro_context's env-arena capacity.
template <int MaxNodes, int MaxList, int MaxBindings = 16,
          int MaxEnvs = closure::default_max_envs>
[[nodiscard]] constexpr auto expand_datum(datum<MaxNodes, MaxList> const &d,
                                          datum_arena<MaxNodes, MaxList> &arena)
    -> smdscheme::foundation::result<datum<MaxNodes, MaxList>> {
    macro_context<MaxNodes, MaxList, MaxBindings, MaxEnvs> ctx{};
    return expand_datum<MaxNodes, MaxList, MaxBindings, MaxEnvs>(d, arena, ctx);
}

/// Expands then elaborates @p pd: the public pipeline entry point wiring
/// this step's pass in between the reader and the elaborator (read ->
/// **expand** -> elaborate), mirroring @ref elaborator::elaborate's own
/// `(datum, datum_arena, core_arena) -> result<core_type>` shape so
/// callers can drop this in as a direct replacement for a bare
/// `elaborate(...)` call.
///
/// Constructs one fresh @ref macro_context for this call (see that
/// class's docs and docs/divergences/DIV-0010): `defmacro` forms anywhere
/// in @p pd are visible to the rest of @p pd's expansion, but not to a
/// separate, later call to this function.
///
/// @tparam MaxNodes    Arena capacity.
/// @tparam MaxList     Maximum list/argument length.
/// @tparam MaxBindings @ref macro_context's environment capacity.
/// @tparam MaxEnvs     @ref macro_context's env-arena capacity.
/// @param  pd          Root datum to expand and elaborate.
/// @param  datum_arena Datum arena produced by the reader; also receives
///                     every node the expansion pass allocates.
/// @param  core_arena  Core arena that receives elaborated nodes (the
///                     REAL program's core arena -- distinct from
///                     @ref macro_context's own private, compile-time-only
///                     core arena used for macro bodies).
template <int MaxNodes, int MaxList, int MaxBindings = 16,
          int MaxEnvs = closure::default_max_envs>
[[nodiscard]] constexpr auto expand_and_elaborate(
    reader::datum_type<MaxNodes, MaxList> const &pd,
    smdscheme::foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>,
                                      MaxNodes> &datum_arena,
    smdscheme::foundation::tree_arena<elaborator::core_type<MaxNodes, MaxList>,
                                      MaxNodes> &core_arena)
    -> smdscheme::foundation::result<elaborator::core_type<MaxNodes, MaxList>> {
    macro_context<MaxNodes, MaxList, MaxBindings, MaxEnvs> ctx{};
    auto expanded_r = expand_datum<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
        pd, datum_arena, ctx);
    if (!expanded_r.has_value())
        return expanded_r.error();
    return elaborator::elaborate<MaxNodes, MaxList>(expanded_r.value(),
                                                    datum_arena, core_arena);
}

} // namespace smd::smdlisp::macroexpand

#endif
