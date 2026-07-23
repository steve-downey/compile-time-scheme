// src/smd/smdlisp/macroexpand/expander.hpp                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_MACROEXPAND_EXPANDER_HPP
#define SRC_SMD_SMDLISP_MACROEXPAND_EXPANDER_HPP

#include <smd/smdlisp/elaborator/elaborate.hpp>
#include <smd/smdlisp/elaborator/elaborated_core.hpp>
#include <smd/smdlisp/reader/atom.hpp>
#include <smd/smdlisp/reader/datum_type.hpp>
#include <smd/smdscheme/foundation/arena_box.hpp>
#include <smd/smdscheme/foundation/parse_error.hpp>
#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/foundation/static_vector.hpp>

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
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto expand_datum(datum<MaxNodes, MaxList> const &d,
                                          datum_arena<MaxNodes, MaxList> &arena)
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
        auto inner_r =
            expand_datum<MaxNodes, MaxList>(arena.get(fq.target), arena);
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
        return expand_datum<MaxNodes, MaxList>(lowered_r.value(), arena);
    }

    if (!std::holds_alternative<DatumList>(d.inner))
        return d; // integer, symbol, keyword: nothing to expand.

    if (detail::is_quote_form<MaxNodes, MaxList>(std::get<DatumList>(d.inner),
                                                 arena))
        return d;

    auto expanded_r = macroexpand<MaxNodes, MaxList>(d, arena);
    if (!expanded_r.has_value())
        return expanded_r;
    auto const &expanded = expanded_r.value();

    if (!std::holds_alternative<DatumList>(expanded.inner))
        return expand_datum<MaxNodes, MaxList>(expanded, arena);

    auto const &lst = std::get<DatumList>(expanded.inner);
    if (detail::is_quote_form<MaxNodes, MaxList>(lst, arena))
        return expanded;

    DatumList new_lst{};
    for (int i = 0; i < lst.elements.size(); ++i) {
        auto elem_r =
            expand_datum<MaxNodes, MaxList>(arena.get(lst.elements[i]), arena);
        if (!elem_r.has_value())
            return elem_r;
        new_lst.elements.push_back(
            smdscheme::foundation::make_arena_box(arena, elem_r.value()));
    }
    return D{datum_f{std::move(new_lst)}};
}

/// Expands then elaborates @p pd: the public pipeline entry point wiring
/// this step's pass in between the reader and the elaborator (read ->
/// **expand** -> elaborate), mirroring @ref elaborator::elaborate's own
/// `(datum, datum_arena, core_arena) -> result<core_type>` shape so
/// callers can drop this in as a direct replacement for a bare
/// `elaborate(...)` call.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list/argument length.
/// @param  pd          Root datum to expand and elaborate.
/// @param  datum_arena Datum arena produced by the reader; also receives
///                     every node the expansion pass allocates.
/// @param  core_arena  Core arena that receives elaborated nodes.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto expand_and_elaborate(
    reader::datum_type<MaxNodes, MaxList> const &pd,
    smdscheme::foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>,
                                      MaxNodes> &datum_arena,
    smdscheme::foundation::tree_arena<elaborator::core_type<MaxNodes, MaxList>,
                                      MaxNodes> &core_arena)
    -> smdscheme::foundation::result<elaborator::core_type<MaxNodes, MaxList>> {
    auto expanded_r = expand_datum<MaxNodes, MaxList>(pd, datum_arena);
    if (!expanded_r.has_value())
        return expanded_r.error();
    return elaborator::elaborate<MaxNodes, MaxList>(expanded_r.value(),
                                                    datum_arena, core_arena);
}

} // namespace smd::smdlisp::macroexpand

#endif
