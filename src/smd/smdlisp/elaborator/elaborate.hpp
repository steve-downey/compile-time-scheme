// src/smd/smdlisp/elaborator/elaborate.hpp                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_ELABORATOR_ELABORATE_HPP
#define SRC_SMD_SMDLISP_ELABORATOR_ELABORATE_HPP

#include <smd/smdlisp/elaborator/elaborated_core.hpp>
#include <smd/smdlisp/reader/datum_type.hpp>
#include <smd/smdscheme/foundation/result.hpp>

namespace smd::smdlisp::elaborator {
namespace detail {

/// The set of block-namespace names (decision D4/D5) lexically visible at
/// the point currently being elaborated: every enclosing `block` (and every
/// enclosing `defun`'s implicit `(block name ...)`, per ANSI CL -- see
/// @ref core_block's docs), innermost last.
///
/// Threaded (by const reference) through every mutually-recursive
/// elaboration function precisely so `return-from`'s target-name check
/// (@ref elaborate_list's `RETURN-FROM` case) can be a purely lexical,
/// elaboration-time decision (decision D5: "unknown block name is an
/// elaboration error") -- whether that block's *dynamic extent* is still
/// active when a `return-from` actually runs is a separate, runtime
/// question the evaluator alone answers (see `eval_direct.hpp`/
/// `cps_code.hpp`'s `exit_record::live` check). A `lambda` does not push a
/// new frame here: CL's `block` scope is an ordinary lexical scope, not
/// reset by an intervening `lambda` -- that is exactly what makes the
/// closure-smuggling diagnosed-error case possible in the first place.
///
/// @tparam MaxList Maximum lexically-nested block depth (reuses the same
///                  bound as argument/body-list length; block nesting this
///                  deep is not a realistic concern for this baseline
///                  elaborator).
template <int MaxList>
using block_scope_type =
    smdscheme::foundation::static_vector<std::string_view, MaxList>;

// bcb23ea4-e878-4fe7-99f5-8049175bda8b
/// The set of `tagbody` tags (step L23) lexically visible at the point
/// currently being elaborated: every tag of every enclosing `tagbody`.
///
/// The exact twin of @ref block_scope_type, one namespace over, and threaded
/// for the same reason: `go`'s target check (@ref elaborate_list's `GO` case)
/// is a purely lexical, elaboration-time decision, while whether the target
/// `tagbody`'s activation is still running is a runtime one. A `lambda` does
/// not push a new frame here either, which is what makes a `go` inside a
/// closure -- and therefore the dead-extent diagnosis -- possible at all.
///
/// It is a *separate* list from @ref block_scope_type rather than a shared
/// one because ANSI CL genuinely gives block names and go tags separate
/// namespaces: `(block foo (tagbody foo (return-from foo 1)))` has a block
/// and a tag both named `FOO`, and the `return-from` must find the block.
///
/// @tparam MaxList Maximum number of lexically visible tags.
template <int MaxList>
using tag_scope_type = smdscheme::foundation::static_vector<core_tag, MaxList>;

/// The lexical scopes the elaborator threads through its mutually recursive
/// functions: everything resolved by *name at elaboration time* rather than
/// by value at run time.
///
/// Bundled into one struct rather than passed as two parallel parameters so
/// that adding the tag namespace did not have to touch every call site of
/// every elaboration function -- and so that adding a third such namespace
/// later will not either.
///
/// @tparam MaxList Capacity of each namespace.
template <int MaxList>
struct lexical_scope {
    block_scope_type<MaxList> blocks{}; ///< Enclosing `block` names.
    tag_scope_type<MaxList> tags{};     ///< Enclosing `tagbody` tags.
};

/// Recognizes a `tagbody` body element that is a *tag* rather than a
/// statement, writing it to @p out.
///
/// ANSI CL: an element that is a symbol or an integer is a tag; anything else
/// is a form to evaluate. That is the entire classification, and it is
/// deliberately not sensitive to *which* symbol -- a bare `nil` or `t` in
/// statement position is a tag named `NIL`/`T`, not the constant, because a
/// bare symbol in a `tagbody` is never an expression.
///
/// @param  d   The body element to classify.
/// @param  out Receives the tag when @p d is one; untouched otherwise.
/// @return True iff @p d is a tag.
template <int MaxNodes, int MaxList>
constexpr auto tagbody_tag_of(reader::datum_type<MaxNodes, MaxList> const &d,
                              core_tag &out) -> bool {
    if (std::holds_alternative<reader::datum_integer>(d.inner)) {
        out = core_tag{std::get<reader::datum_integer>(d.inner).value};
        return true;
    }
    if (std::holds_alternative<reader::datum_symbol>(d.inner)) {
        out = core_tag{std::get<reader::datum_symbol>(d.inner).name.view()};
        return true;
    }
    return false;
}
// bcb23ea4-e878-4fe7-99f5-8049175bda8b end

/// Builds a @ref core_symbol from an already-uppercase static spelling.
///
/// Used only for the synthetic `QUOTE`/`FUNCTION` head symbols
/// @ref elaborate_quoted_datum constructs when quoting a nested quote or
/// sharpsign-quote datum (`''x` / `'#'x`); folding is a no-op for these
/// literals, but going through @ref reader::detail::fold keeps one
/// definition of "how a folded name is built" rather than hand-filling
/// @ref reader::folded_name's storage array here.
constexpr auto literal_symbol(std::string_view spelling) -> core_symbol {
    return core_symbol{reader::detail::fold(spelling)};
}

/// Elaborates a quoted datum into a core expression that *constructs* the
/// datum.
///
/// A quoted compound datum is lowered to construction rather than a second
/// literal-data representation: `'(1 2 3)` becomes nested @ref core_cons
/// cells bottoming in @ref core_nil. Atoms become @ref core_quote, except
/// `NIL`/`T` symbols, which elaborate directly to @ref core_nil /
/// @ref core_true — quoting them is a no-op per decision D3, so there is no
/// separate "quoted nil" representation. A nested quote datum (`''x`) or a
/// nested sharpsign-quote datum (`'#'x`) is data: it builds the two-element
/// list `(QUOTE x)` / `(FUNCTION x)`, mirroring the Scheme elaborator's
/// treatment of a nested quote.
///
/// This function does not compute real source positions for the errors it
/// returns: the datum tree (`smd::smdlisp::reader::datum_type`) does not
/// thread @c source_pos through its nodes, so there is nothing to recover
/// once a datum has already been read. Every error uses a default-valued
/// @c source_pos, matching the Scheme elaborator's `elaborate_quoted_datum`
/// exactly (real positions are a reader-level concern; see
/// `smd::smdlisp::reader::read_datum`'s errors for those).
///
/// No block-scope parameter: quoted data is never evaluated, so it can
/// never contain a live `return-from`.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
/// @param  d           The datum to quote.
/// @param  datum_arena Read-only datum arena.
/// @param  core_arena  Core arena receiving the constructed nodes.
template <int MaxNodes, int MaxList>
constexpr auto elaborate_quoted_datum(
    reader::datum_type<MaxNodes, MaxList> const &d,
    smdscheme::foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>,
                                      MaxNodes> const &datum_arena,
    smdscheme::foundation::tree_arena<core_type<MaxNodes, MaxList>, MaxNodes>
        &core_arena)
    -> smdscheme::foundation::result<core_type<MaxNodes, MaxList>> {
    using core = core_type<MaxNodes, MaxList>;
    using core_f =
        typename core_f_factory<MaxNodes, MaxList>::template type<core>;
    using DatumT = reader::datum_type<MaxNodes, MaxList>;
    using DatumList = reader::datum_list<DatumT, MaxNodes, MaxList>;
    using DatumQuote = reader::datum_quote<DatumT, MaxNodes>;
    using DatumFunction = reader::datum_function<DatumT, MaxNodes>;
    using Cons = core_cons<core, MaxNodes>;

    if (std::holds_alternative<reader::datum_integer>(d.inner)) {
        return core{
            core_f{core_quote{std::get<reader::datum_integer>(d.inner).value}}};
    }
    if (std::holds_alternative<reader::datum_symbol>(d.inner)) {
        // Copy the folded_name (not a view into it): d may be the root
        // datum returned by read_datum, which is not itself arena-backed
        // (see core_symbol's docs for why this matters).
        auto const &sym = std::get<reader::datum_symbol>(d.inner);
        if (sym.name.view() == "NIL")
            return core{core_f{core_nil{}}};
        if (sym.name.view() == "T")
            return core{core_f{core_true{}}};
        return core{core_f{core_quote{core_symbol{sym.name}}}};
    }
    if (std::holds_alternative<reader::datum_keyword>(d.inner)) {
        auto const &kw = std::get<reader::datum_keyword>(d.inner);
        return core{core_f{core_quote{core_keyword{kw.name}}}};
    }
    if (std::holds_alternative<DatumList>(d.inner)) {
        auto const &lst = std::get<DatumList>(d.inner);
        // Right-fold the elements into cons cells, bottoming in nil.
        core tail = core{core_f{core_nil{}}};
        for (int i = lst.elements.size() - 1; i >= 0; --i) {
            auto elem_r = elaborate_quoted_datum<MaxNodes, MaxList>(
                datum_arena.get(lst.elements[i]), datum_arena, core_arena);
            if (!elem_r.has_value())
                return elem_r;
            Cons cell{smdscheme::foundation::make_arena_box(
                          core_arena, std::move(elem_r.value())),
                      smdscheme::foundation::make_arena_box(core_arena,
                                                            std::move(tail))};
            tail = core{core_f{std::move(cell)}};
        }
        return tail;
    }
    if (std::holds_alternative<DatumQuote>(d.inner)) {
        // A quote inside a quote is data: '(quote <inner>).
        auto const &q = std::get<DatumQuote>(d.inner);
        auto inner_r = elaborate_quoted_datum<MaxNodes, MaxList>(
            datum_arena.get(q.quoted), datum_arena, core_arena);
        if (!inner_r.has_value())
            return inner_r;
        Cons after{smdscheme::foundation::make_arena_box(
                       core_arena, std::move(inner_r.value())),
                   smdscheme::foundation::make_arena_box(
                       core_arena, core{core_f{core_nil{}}})};
        Cons outer{
            smdscheme::foundation::make_arena_box(
                core_arena, core{core_f{core_quote{literal_symbol("QUOTE")}}}),
            smdscheme::foundation::make_arena_box(
                core_arena, core{core_f{std::move(after)}})};
        return core{core_f{std::move(outer)}};
    }
    if (std::holds_alternative<DatumFunction>(d.inner)) {
        // A sharpsign-quote inside a quote is data: '(function <inner>).
        auto const &fq = std::get<DatumFunction>(d.inner);
        auto inner_r = elaborate_quoted_datum<MaxNodes, MaxList>(
            datum_arena.get(fq.target), datum_arena, core_arena);
        if (!inner_r.has_value())
            return inner_r;
        Cons after{smdscheme::foundation::make_arena_box(
                       core_arena, std::move(inner_r.value())),
                   smdscheme::foundation::make_arena_box(
                       core_arena, core{core_f{core_nil{}}})};
        Cons outer{smdscheme::foundation::make_arena_box(
                       core_arena,
                       core{core_f{core_quote{literal_symbol("FUNCTION")}}}),
                   smdscheme::foundation::make_arena_box(
                       core_arena, core{core_f{std::move(after)}})};
        return core{core_f{std::move(outer)}};
    }
    return smdscheme::foundation::parse_error{{}, "quote: unsupported datum"};
}

/// Forward declarations (mutually recursive elaboration functions).
template <int MaxNodes, int MaxList>
constexpr auto elaborate_node(
    reader::datum_type<MaxNodes, MaxList> const &d,
    smdscheme::foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>,
                                      MaxNodes> const &datum_arena,
    smdscheme::foundation::tree_arena<core_type<MaxNodes, MaxList>, MaxNodes>
        &core_arena,
    lexical_scope<MaxList> const &scope)
    -> smdscheme::foundation::result<core_type<MaxNodes, MaxList>>;

template <int MaxNodes, int MaxList>
constexpr auto elaborate_lambda(
    reader::datum_list<reader::datum_type<MaxNodes, MaxList>, MaxNodes,
                       MaxList> const &lst,
    smdscheme::foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>,
                                      MaxNodes> const &datum_arena,
    smdscheme::foundation::tree_arena<core_type<MaxNodes, MaxList>, MaxNodes>
        &core_arena,
    lexical_scope<MaxList> const &scope)
    -> smdscheme::foundation::result<core_type<MaxNodes, MaxList>>;

template <int MaxNodes, int MaxList>
constexpr auto elaborate_function_position(
    reader::datum_type<MaxNodes, MaxList> const &d,
    smdscheme::foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>,
                                      MaxNodes> const &datum_arena,
    smdscheme::foundation::tree_arena<core_type<MaxNodes, MaxList>, MaxNodes>
        &core_arena,
    char const *error_message, lexical_scope<MaxList> const &scope)
    -> smdscheme::foundation::result<core_type<MaxNodes, MaxList>>;

/// Elaborates a formals list and a body-expression range into a
/// @ref core_lambda.
///
/// This is the shared implementation behind @ref elaborate_lambda (the
/// standalone `lambda` special form, and `#'(lambda ...)`/`(function
/// (lambda ...))`) and `defun`'s elaboration (step L12): both forms need
/// "a formals list, then one or more body expressions", differing only in
/// *where* those two pieces sit inside their enclosing datum list --
/// `(LAMBDA (params...) body...)` has formals at index 1 and body starting
/// at index 2, while `(DEFUN name (params...) body...)` has formals at
/// index 2 and body starting at index 3. Factoring this out lets `defun`
/// reuse the exact same formals/body elaboration `lambda` uses, rather than
/// duplicating it, by passing the formals handle and body-start index
/// explicitly instead of assuming fixed positions in @p lst.
///
/// The body is elaborated as an EXPRESSION SEQUENCE with implicit progn:
/// at least one body expression is required (a zero-form body, though
/// legal in full ANSI CL, is out of scope for this baseline elaborator).
///
/// `defun`'s implicit `(block name ...)` wrapping (step L14) is *not* done
/// here: this function only builds the raw formals/body pair shared by
/// `lambda` and `defun` alike. @ref elaborate_list's `DEFUN` case wraps the
/// returned body afterward -- see that case's comment and @ref
/// core_defun's docs for why the wrapping happens one level up instead of
/// being threaded through this shared helper. @p scope is still
/// threaded through unchanged (an ordinary `lambda` does not push a new
/// block-scope frame; see @ref block_scope_type's docs) so a `defun` caller
/// that already pushed its own name before calling this function has that
/// name visible to `return-from` inside the body.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
/// @param  formals_id  Handle to the formals-list datum.
/// @param  lst         The enclosing datum list (`lambda` or `defun` form);
///                     only elements at and after @p body_start are read.
/// @param  body_start  Index into @p lst.elements of the first body
///                     expression.
/// @param  scope Lexically visible block names and tagbody tags (see
///                      @ref lexical_scope).
template <int MaxNodes, int MaxList>
constexpr auto elaborate_lambda_body(
    smdscheme::foundation::arena_box<reader::datum_type<MaxNodes, MaxList>,
                                     MaxNodes>
        formals_id,
    reader::datum_list<reader::datum_type<MaxNodes, MaxList>, MaxNodes,
                       MaxList> const &lst,
    int body_start,
    smdscheme::foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>,
                                      MaxNodes> const &datum_arena,
    smdscheme::foundation::tree_arena<core_type<MaxNodes, MaxList>, MaxNodes>
        &core_arena,
    lexical_scope<MaxList> const &scope)
    -> smdscheme::foundation::result<core_type<MaxNodes, MaxList>> {
    using core = core_type<MaxNodes, MaxList>;
    using core_f =
        typename core_f_factory<MaxNodes, MaxList>::template type<core>;
    using DatumList = reader::datum_list<reader::datum_type<MaxNodes, MaxList>,
                                         MaxNodes, MaxList>;

    if (body_start >= lst.elements.size())
        return smdscheme::foundation::parse_error{
            {}, "lambda: expected formals and at least one body expression"};

    auto const &formals_node = datum_arena.get(formals_id);
    if (!std::holds_alternative<DatumList>(formals_node.inner))
        return smdscheme::foundation::parse_error{{},
                                                  "lambda: formals must be a "
                                                  "list"};

    core_lambda<core, MaxNodes, MaxList> lam{};
    auto const &formals = std::get<DatumList>(formals_node.inner);
    for (int i = 0; i < formals.elements.size(); ++i) {
        auto const &p = datum_arena.get(formals.elements[i]);
        if (!std::holds_alternative<reader::datum_symbol>(p.inner))
            return smdscheme::foundation::parse_error{
                {}, "lambda: formal must be a symbol"};
        auto p_name = std::get<reader::datum_symbol>(p.inner).name.view();
        for (auto const &existing : lam.params) {
            if (existing == p_name)
                return smdscheme::foundation::parse_error{
                    {}, "lambda: duplicate parameter"};
        }
        lam.params.push_back(p_name);
    }

    for (int i = body_start; i < lst.elements.size(); ++i) {
        auto body_r = elaborate_node<MaxNodes, MaxList>(
            datum_arena.get(lst.elements[i]), datum_arena, core_arena, scope);
        if (!body_r.has_value())
            return body_r;
        lam.body.push_back(smdscheme::foundation::make_arena_box(
            core_arena, std::move(body_r.value())));
    }

    return core{core_f{std::move(lam)}};
}

/// Elaborates the formals and body of a @c lambda form.
///
/// @p lst is the whole `(LAMBDA (params...) body...)` datum list, including
/// the leading `LAMBDA` symbol; this is shared by the standalone @c lambda
/// special form and by @ref elaborate_function_position's `(function
/// (lambda ...))` / `#'(lambda ...)` case, so both spellings of an embedded
/// lambda expression go through one implementation. Delegates the actual
/// formals/body elaboration to @ref elaborate_lambda_body (shared with
/// `defun`'s elaboration, step L12).
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
/// @param  scope Lexically visible block names and tagbody tags, threaded
///                      through unchanged (a `lambda` does not push a new
///                      frame; see @ref lexical_scope).
template <int MaxNodes, int MaxList>
constexpr auto elaborate_lambda(
    reader::datum_list<reader::datum_type<MaxNodes, MaxList>, MaxNodes,
                       MaxList> const &lst,
    smdscheme::foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>,
                                      MaxNodes> const &datum_arena,
    smdscheme::foundation::tree_arena<core_type<MaxNodes, MaxList>, MaxNodes>
        &core_arena,
    lexical_scope<MaxList> const &scope)
    -> smdscheme::foundation::result<core_type<MaxNodes, MaxList>> {
    if (lst.elements.size() < 3)
        return smdscheme::foundation::parse_error{
            {}, "lambda: expected formals and at least one body expression"};
    return elaborate_lambda_body<MaxNodes, MaxList>(
        lst.elements[1], lst, 2, datum_arena, core_arena, scope);
}

/// Elaborates a datum in FUNCTION position (decision D4): the operand of
/// @c function/@c #', or the head of an application.
///
/// ANSI CL permits exactly two spellings here: a symbol (a FUNCTION
/// -namespace reference) or a @c (lambda ...) expression (elaborated by
/// @ref elaborate_lambda and embedded directly). Anything else is a
/// compile-time error using @p error_message, so callers can give a
/// diagnostic appropriate to their own context (`function`/`#'` vs. an
/// application head).
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
/// @param  d             The datum in function position.
/// @param  datum_arena   Read-only datum arena.
/// @param  core_arena    Core arena receiving elaborated nodes.
/// @param  error_message Static diagnostic used when @p d is neither a
///                       symbol nor a `(lambda ...)` form.
/// @param  scope   Lexically visible block names and tagbody tags, threaded
///                        through unchanged into an embedded lambda's body
///                        (see @ref lexical_scope).
template <int MaxNodes, int MaxList>
constexpr auto elaborate_function_position(
    reader::datum_type<MaxNodes, MaxList> const &d,
    smdscheme::foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>,
                                      MaxNodes> const &datum_arena,
    smdscheme::foundation::tree_arena<core_type<MaxNodes, MaxList>, MaxNodes>
        &core_arena,
    char const *error_message, lexical_scope<MaxList> const &scope)
    -> smdscheme::foundation::result<core_type<MaxNodes, MaxList>> {
    using core = core_type<MaxNodes, MaxList>;
    using core_f =
        typename core_f_factory<MaxNodes, MaxList>::template type<core>;
    using DatumList = reader::datum_list<reader::datum_type<MaxNodes, MaxList>,
                                         MaxNodes, MaxList>;

    if (std::holds_alternative<reader::datum_symbol>(d.inner)) {
        auto name = std::get<reader::datum_symbol>(d.inner).name.view();
        return core{core_f{core_function<core, MaxNodes>{name}}};
    }

    if (std::holds_alternative<DatumList>(d.inner)) {
        auto const &lst = std::get<DatumList>(d.inner);
        if (!lst.elements.empty()) {
            auto const &head = datum_arena.get(lst.elements[0]);
            if (std::holds_alternative<reader::datum_symbol>(head.inner) &&
                std::get<reader::datum_symbol>(head.inner).name.view() ==
                    "LAMBDA") {
                auto lam_r = elaborate_lambda<MaxNodes, MaxList>(
                    lst, datum_arena, core_arena, scope);
                if (!lam_r.has_value())
                    return lam_r;
                return core{core_f{core_function<core, MaxNodes>{
                    smdscheme::foundation::make_arena_box(
                        core_arena, std::move(lam_r.value()))}}};
            }
        }
    }

    return smdscheme::foundation::parse_error{{}, error_message};
}

/// Elaborates a datum list into a core form.
///
/// Recognizes the special operators `quote`, `if`, `progn`, `let`, `let*`,
/// `lambda`, `function`, `block`, `return-from`, `catch`, `throw`,
/// `unwind-protect`, `multiple-value-bind`, `tagbody`, and `go` by inspecting
/// the leading symbol (folded spellings, per decision D2). Any other head is an
/// ordinary application, whose head is elaborated through @ref
/// elaborate_function_position (a bare symbol resolves in the FUNCTION
/// namespace; a `(lambda ...)` head is also legal per ANSI CL). An empty
/// list (`()`) as an application is a compile-time error, not the
/// empty-list value — quote it (`'()`) to get @ref core_nil.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
/// @param  lst         The list datum to elaborate.
/// @param  datum_arena Read-only datum arena.
/// @param  core_arena  Core arena receiving elaborated nodes.
/// @param  scope Lexically visible block names and tagbody tags (decision
///                      D5, step L14) -- see @ref lexical_scope.
template <int MaxNodes, int MaxList>
constexpr auto elaborate_list(
    reader::datum_list<reader::datum_type<MaxNodes, MaxList>, MaxNodes,
                       MaxList> const &lst,
    smdscheme::foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>,
                                      MaxNodes> const &datum_arena,
    smdscheme::foundation::tree_arena<core_type<MaxNodes, MaxList>, MaxNodes>
        &core_arena,
    lexical_scope<MaxList> const &scope)
    -> smdscheme::foundation::result<core_type<MaxNodes, MaxList>> {
    using core = core_type<MaxNodes, MaxList>;
    using core_f =
        typename core_f_factory<MaxNodes, MaxList>::template type<core>;
    using DatumList = reader::datum_list<reader::datum_type<MaxNodes, MaxList>,
                                         MaxNodes, MaxList>;

    if (lst.elements.empty())
        return smdscheme::foundation::parse_error{{}, "empty application"};

    auto const &first = datum_arena.get(lst.elements[0]);
    if (std::holds_alternative<reader::datum_symbol>(first.inner)) {
        auto name = std::get<reader::datum_symbol>(first.inner).name.view();

        if (name == "IF") {
            if (lst.elements.size() != 3 && lst.elements.size() != 4)
                return smdscheme::foundation::parse_error{
                    {}, "if: expected 2 or 3 arguments"};

            auto cond_r = elaborate_node<MaxNodes, MaxList>(
                datum_arena.get(lst.elements[1]), datum_arena, core_arena,
                scope);
            if (!cond_r.has_value())
                return cond_r;

            auto cons_r = elaborate_node<MaxNodes, MaxList>(
                datum_arena.get(lst.elements[2]), datum_arena, core_arena,
                scope);
            if (!cons_r.has_value())
                return cons_r;

            smdscheme::foundation::arena_box<core, MaxNodes> alt_box;
            if (lst.elements.size() == 4) {
                auto alt_r = elaborate_node<MaxNodes, MaxList>(
                    datum_arena.get(lst.elements[3]), datum_arena, core_arena,
                    scope);
                if (!alt_r.has_value())
                    return alt_r;
                alt_box = smdscheme::foundation::make_arena_box(
                    core_arena, std::move(alt_r.value()));
            } else {
                // Two-argument if: implicit nil alternative.
                alt_box = smdscheme::foundation::make_arena_box(
                    core_arena, core{core_f{core_nil{}}});
            }

            return core{core_f{core_if<core, MaxNodes>{
                smdscheme::foundation::make_arena_box(
                    core_arena, std::move(cond_r.value())),
                smdscheme::foundation::make_arena_box(
                    core_arena, std::move(cons_r.value())),
                alt_box}}};
        }

        if (name == "QUOTE") {
            if (lst.elements.size() != 2)
                return smdscheme::foundation::parse_error{
                    {}, "quote: expected 1 argument"};
            return elaborate_quoted_datum<MaxNodes, MaxList>(
                datum_arena.get(lst.elements[1]), datum_arena, core_arena);
        }

        if (name == "PROGN") {
            if (lst.elements.size() < 2)
                return smdscheme::foundation::parse_error{
                    {}, "progn: expected at least one expression"};

            core_progn<core, MaxNodes, MaxList> seq{};
            for (int i = 1; i < lst.elements.size(); ++i) {
                auto expr_r = elaborate_node<MaxNodes, MaxList>(
                    datum_arena.get(lst.elements[i]), datum_arena, core_arena,
                    scope);
                if (!expr_r.has_value())
                    return expr_r;
                seq.exprs.push_back(smdscheme::foundation::make_arena_box(
                    core_arena, std::move(expr_r.value())));
            }
            return core{core_f{std::move(seq)}};
        }

        if (name == "LAMBDA") {
            return elaborate_lambda<MaxNodes, MaxList>(lst, datum_arena,
                                                       core_arena, scope);
        }

        if (name == "FUNCTION") {
            if (lst.elements.size() != 2)
                return smdscheme::foundation::parse_error{
                    {}, "function: expected 1 argument"};
            return elaborate_function_position<MaxNodes, MaxList>(
                datum_arena.get(lst.elements[1]), datum_arena, core_arena,
                "function: expected a function name or a lambda expression",
                scope);
        }

        if (name == "BLOCK") {
            // (block name body...) -- ANSI CL: body is &rest, zero or more
            // forms; zero forms evaluates to nil (see core_block's docs,
            // which also cover the "block/tag names are their own
            // namespace, decision D4" and "a lambda does not reset this
            // scope" points behind the block-scope threading below).
            if (lst.elements.size() < 2)
                return smdscheme::foundation::parse_error{
                    {}, "block: expected a name"};

            auto const &name_node = datum_arena.get(lst.elements[1]);
            if (!std::holds_alternative<reader::datum_symbol>(name_node.inner))
                return smdscheme::foundation::parse_error{
                    {}, "block: name must be a symbol"};
            auto blk_name =
                std::get<reader::datum_symbol>(name_node.inner).name.view();

            lexical_scope<MaxList> inner_scope = scope;
            inner_scope.blocks.push_back(blk_name);

            core_block<core, MaxNodes, MaxList> blk{};
            blk.name = blk_name;
            for (int i = 2; i < lst.elements.size(); ++i) {
                auto body_r = elaborate_node<MaxNodes, MaxList>(
                    datum_arena.get(lst.elements[i]), datum_arena, core_arena,
                    inner_scope);
                if (!body_r.has_value())
                    return body_r;
                blk.body.push_back(smdscheme::foundation::make_arena_box(
                    core_arena, std::move(body_r.value())));
            }
            if (blk.body.empty())
                blk.body.push_back(smdscheme::foundation::make_arena_box(
                    core_arena, core{core_f{core_nil{}}}));
            return core{core_f{std::move(blk)}};
        }

        if (name == "RETURN-FROM") {
            // (return-from name [value]) -- name must resolve to a
            // lexically enclosing block (decision D5); see
            // core_return_from's docs for why *this* check is the whole of
            // "resolves its block lexically at elaboration time", and why
            // that is not the same question as whether the block's dynamic
            // extent is still active when this form actually runs.
            if (lst.elements.size() != 2 && lst.elements.size() != 3)
                return smdscheme::foundation::parse_error{
                    {},
                    "return-from: expected a block name and an optional "
                    "value"};

            auto const &name_node = datum_arena.get(lst.elements[1]);
            if (!std::holds_alternative<reader::datum_symbol>(name_node.inner))
                return smdscheme::foundation::parse_error{
                    {}, "return-from: block name must be a symbol"};
            auto target_name =
                std::get<reader::datum_symbol>(name_node.inner).name.view();

            bool found = false;
            for (auto const &b : scope.blocks)
                if (b == target_name)
                    found = true;
            if (!found)
                return smdscheme::foundation::parse_error{
                    {}, "return-from: undefined block"};

            smdscheme::foundation::arena_box<core, MaxNodes> val_box;
            if (lst.elements.size() == 3) {
                auto val_r = elaborate_node<MaxNodes, MaxList>(
                    datum_arena.get(lst.elements[2]), datum_arena, core_arena,
                    scope);
                if (!val_r.has_value())
                    return val_r;
                val_box = smdscheme::foundation::make_arena_box(
                    core_arena, std::move(val_r.value()));
            } else {
                // No value form: implicit nil, mirroring if's implicit
                // alternative.
                val_box = smdscheme::foundation::make_arena_box(
                    core_arena, core{core_f{core_nil{}}});
            }

            return core{
                core_f{core_return_from<core, MaxNodes>{target_name, val_box}}};
        }

        if (name == "CATCH") {
            // (catch tag-form body...) -- ANSI CL: body is &rest, zero or
            // more forms; zero forms evaluates to nil, exactly as `block`'s
            // does. Nothing about the tag is resolved here: a catch tag is
            // an evaluated value, matched with `eq` at run time, so unlike
            // `return-from`'s lexical block name there is no
            // elaboration-time check to make (see core_catch's docs).
            if (lst.elements.size() < 2)
                return smdscheme::foundation::parse_error{
                    {}, "catch: expected a tag form"};

            auto tag_r = elaborate_node<MaxNodes, MaxList>(
                datum_arena.get(lst.elements[1]), datum_arena, core_arena,
                scope);
            if (!tag_r.has_value())
                return tag_r;

            core_catch<core, MaxNodes, MaxList> cat{};
            cat.tag = smdscheme::foundation::make_arena_box(
                core_arena, std::move(tag_r.value()));
            for (int i = 2; i < lst.elements.size(); ++i) {
                auto body_r = elaborate_node<MaxNodes, MaxList>(
                    datum_arena.get(lst.elements[i]), datum_arena, core_arena,
                    scope);
                if (!body_r.has_value())
                    return body_r;
                cat.body.push_back(smdscheme::foundation::make_arena_box(
                    core_arena, std::move(body_r.value())));
            }
            if (cat.body.empty())
                cat.body.push_back(smdscheme::foundation::make_arena_box(
                    core_arena, core{core_f{core_nil{}}}));
            return core{core_f{std::move(cat)}};
        }

        if (name == "THROW") {
            // (throw tag-form result-form) -- both required per ANSI CL
            // (see core_throw's docs for why there is no implicit-nil
            // result the way return-from has an implicit-nil value).
            if (lst.elements.size() != 3)
                return smdscheme::foundation::parse_error{
                    {}, "throw: expected a tag form and a result form"};

            auto tag_r = elaborate_node<MaxNodes, MaxList>(
                datum_arena.get(lst.elements[1]), datum_arena, core_arena,
                scope);
            if (!tag_r.has_value())
                return tag_r;
            auto res_r = elaborate_node<MaxNodes, MaxList>(
                datum_arena.get(lst.elements[2]), datum_arena, core_arena,
                scope);
            if (!res_r.has_value())
                return res_r;

            return core{core_f{core_throw<core, MaxNodes>{
                smdscheme::foundation::make_arena_box(core_arena,
                                                      std::move(tag_r.value())),
                smdscheme::foundation::make_arena_box(
                    core_arena, std::move(res_r.value()))}}};
        }

        if (name == "UNWIND-PROTECT") {
            // (unwind-protect protected-form cleanup...) -- ANSI CL: the
            // cleanup list may be empty, so no implicit nil is synthesized
            // (core_unwind_protect's docs).
            if (lst.elements.size() < 2)
                return smdscheme::foundation::parse_error{
                    {}, "unwind-protect: expected a protected form"};

            auto prot_r = elaborate_node<MaxNodes, MaxList>(
                datum_arena.get(lst.elements[1]), datum_arena, core_arena,
                scope);
            if (!prot_r.has_value())
                return prot_r;

            core_unwind_protect<core, MaxNodes, MaxList> up{};
            up.protected_form = smdscheme::foundation::make_arena_box(
                core_arena, std::move(prot_r.value()));
            for (int i = 2; i < lst.elements.size(); ++i) {
                auto cl_r = elaborate_node<MaxNodes, MaxList>(
                    datum_arena.get(lst.elements[i]), datum_arena, core_arena,
                    scope);
                if (!cl_r.has_value())
                    return cl_r;
                up.cleanup.push_back(smdscheme::foundation::make_arena_box(
                    core_arena, std::move(cl_r.value())));
            }
            return core{core_f{std::move(up)}};
        }

        // 45420d1b-7082-46d4-a27a-9aef17024f8c
        if (name == "TAGBODY") {
            // (tagbody {tag | statement}*) -- split the interleaved body into
            // a statement sequence and a tag -> block-index table right here,
            // so that no evaluator has to re-derive the basic blocks (see
            // core_tagbody's docs).
            //
            // TWO PASSES, and the reason is the merge criterion's own first
            // example: `(tagbody a (go c) b (setq x 1) c)` jumps FORWARD, so
            // every tag in the body must already be in scope when the first
            // statement is elaborated. Collecting labels first and elaborating
            // statements second is the whole of that.
            core_tagbody<core, MaxNodes, MaxList> tb{};
            lexical_scope<MaxList> inner_scope = scope;

            int block_start = 0;
            for (int i = 1; i < lst.elements.size(); ++i) {
                core_tag tag{};
                if (!tagbody_tag_of<MaxNodes, MaxList>(
                        datum_arena.get(lst.elements[i]), tag)) {
                    ++block_start;
                    continue;
                }
                for (auto const &existing : tb.labels) {
                    if (existing.tag == tag)
                        return smdscheme::foundation::parse_error{
                            {}, "tagbody: duplicate tag"};
                }
                tb.labels.push_back(core_tag_label{tag, block_start});
                inner_scope.tags.push_back(tag);
            }

            for (int i = 1; i < lst.elements.size(); ++i) {
                core_tag tag{};
                if (tagbody_tag_of<MaxNodes, MaxList>(
                        datum_arena.get(lst.elements[i]), tag))
                    continue;
                auto stmt_r = elaborate_node<MaxNodes, MaxList>(
                    datum_arena.get(lst.elements[i]), datum_arena, core_arena,
                    inner_scope);
                if (!stmt_r.has_value())
                    return stmt_r;
                tb.statements.push_back(smdscheme::foundation::make_arena_box(
                    core_arena, std::move(stmt_r.value())));
            }
            return core{core_f{std::move(tb)}};
        }

        if (name == "GO") {
            // (go tag) -- the tag must name a lexically enclosing tagbody's
            // label (decision D5, exactly as return-from's block name must);
            // whether that tagbody's activation is still running is a
            // *runtime* question, see core_go's docs.
            if (lst.elements.size() != 2)
                return smdscheme::foundation::parse_error{{},
                                                          "go: expected a tag"};

            core_tag target{};
            if (!tagbody_tag_of<MaxNodes, MaxList>(
                    datum_arena.get(lst.elements[1]), target))
                return smdscheme::foundation::parse_error{
                    {}, "go: tag must be a symbol or an integer"};

            bool found = false;
            for (auto const &t : scope.tags)
                if (t == target)
                    found = true;
            if (!found)
                return smdscheme::foundation::parse_error{{},
                                                          "go: undefined tag"};

            return core{core_f{core_go{target}}};
        }
        // 45420d1b-7082-46d4-a27a-9aef17024f8c end

        if (name == "SETQ") {
            // (setq name1 expr1 name2 expr2 ...) -- ANSI CL allows any
            // number of name/value pairs (at least one), assigning each in
            // turn and yielding the value of the last (evaluator's job;
            // see core_setq's docs). Adapted from the landed Scheme
            // `set!`/`core_set` machinery (PR #23), generalized from one
            // pair to CL's variadic pairs.
            if (lst.elements.size() < 3 || (lst.elements.size() % 2) != 1)
                return smdscheme::foundation::parse_error{
                    {},
                    "setq: expected an even number of name/value "
                    "arguments"};

            core_setq<core, MaxNodes, MaxList> sq{};
            for (int i = 1; i < lst.elements.size(); i += 2) {
                auto const &name_node = datum_arena.get(lst.elements[i]);
                if (!std::holds_alternative<reader::datum_symbol>(
                        name_node.inner))
                    return smdscheme::foundation::parse_error{
                        {}, "setq: target must be a symbol"};
                auto var_name =
                    std::get<reader::datum_symbol>(name_node.inner).name.view();

                auto expr_r = elaborate_node<MaxNodes, MaxList>(
                    datum_arena.get(lst.elements[i + 1]), datum_arena,
                    core_arena, scope);
                if (!expr_r.has_value())
                    return expr_r;

                sq.names.push_back(var_name);
                sq.exprs.push_back(smdscheme::foundation::make_arena_box(
                    core_arena, std::move(expr_r.value())));
            }
            return core{core_f{std::move(sq)}};
        }

        if (name == "DEFUN") {
            // (defun name (params...) body...) -- defines `name` in the
            // FUNCTION namespace (decision D4) as a closure. Reuses the
            // exact same formals/body elaboration as an ordinary `lambda`
            // (elaborate_lambda_body), offset by one element to skip the
            // function name.
            //
            // ANSI CL: a defun body is implicitly wrapped in `(block name
            // ...)` (step L14, core_block's docs), so `fn_name` is pushed
            // onto the block scope *before* elaborating the body (making
            // return-from valid inside the body, including inside a nested
            // lambda -- block_scope_type's docs), and the elaborated body
            // sequence is then re-wrapped into a single core_block node
            // after the fact, rather than threading the wrapping decision
            // through elaborate_lambda_body itself (which stays shared,
            // unmodified, with ordinary `lambda`).
            if (lst.elements.size() < 4)
                return smdscheme::foundation::parse_error{
                    {},
                    "defun: expected a name, formals, and at least one "
                    "body expression"};

            auto const &name_node = datum_arena.get(lst.elements[1]);
            if (!std::holds_alternative<reader::datum_symbol>(name_node.inner))
                return smdscheme::foundation::parse_error{
                    {}, "defun: name must be a symbol"};
            auto fn_name =
                std::get<reader::datum_symbol>(name_node.inner).name.view();

            lexical_scope<MaxList> inner_scope = scope;
            inner_scope.blocks.push_back(fn_name);

            auto lam_r = elaborate_lambda_body<MaxNodes, MaxList>(
                lst.elements[2], lst, 3, datum_arena, core_arena, inner_scope);
            if (!lam_r.has_value())
                return lam_r;

            auto lam_core = std::move(lam_r.value());
            auto &lam =
                std::get<core_lambda<core, MaxNodes, MaxList>>(lam_core.inner);

            core_block<core, MaxNodes, MaxList> blk{};
            blk.name = fn_name;
            blk.body = std::move(lam.body);

            core_lambda<core, MaxNodes, MaxList> wrapped{};
            wrapped.params = std::move(lam.params);
            wrapped.body.push_back(smdscheme::foundation::make_arena_box(
                core_arena, core{core_f{std::move(blk)}}));

            return core{core_f{core_defun<core, MaxNodes>{
                fn_name, smdscheme::foundation::make_arena_box(
                             core_arena, core{core_f{std::move(wrapped)}})}}};
        }

        if (name == "DEFVAR" || name == "DEFPARAMETER") {
            // (defvar name [init-form])
            // (defparameter name init-form)
            // Both define `name` in the VARIABLE namespace (decision D4)
            // and mark it special; special-variable dynamic-binding
            // *behavior* arrives in step L16, this step only records the
            // mark (evaluator's job -- see core_defvar's docs).
            bool const is_parameter = name == "DEFPARAMETER";
            if (is_parameter && lst.elements.size() != 3)
                return smdscheme::foundation::parse_error{
                    {}, "defparameter: expected a name and an init form"};
            if (!is_parameter &&
                (lst.elements.size() < 2 || lst.elements.size() > 3))
                return smdscheme::foundation::parse_error{
                    {}, "defvar: expected a name and an optional init form"};

            auto const &name_node = datum_arena.get(lst.elements[1]);
            if (!std::holds_alternative<reader::datum_symbol>(name_node.inner))
                return smdscheme::foundation::parse_error{
                    {}, "defvar: name must be a symbol"};
            auto var_name =
                std::get<reader::datum_symbol>(name_node.inner).name.view();

            core_defvar<core, MaxNodes> dv{};
            dv.name = var_name;
            dv.is_parameter = is_parameter;
            if (lst.elements.size() == 3) {
                auto init_r = elaborate_node<MaxNodes, MaxList>(
                    datum_arena.get(lst.elements[2]), datum_arena, core_arena,
                    scope);
                if (!init_r.has_value())
                    return init_r;
                dv.init = smdscheme::foundation::make_arena_box(
                    core_arena, std::move(init_r.value()));
                dv.has_init = true;
            }
            return core{core_f{std::move(dv)}};
        }

        // Note what is NOT recognized here: `values`.  It is a *function* in
        // ANSI CL, and step L20 keeps it one -- an ordinary builtin in the
        // FUNCTION namespace -- so `(values 1 2)` falls through to the
        // application path below like any other call.  See
        // core_multiple_value_bind's docs for why the binding form cannot
        // get the same treatment.
        if (name == "MULTIPLE-VALUE-BIND") {
            // (multiple-value-bind (var...) values-form body...)
            //   -> core_multiple_value_bind{values-form,
            //                               (lambda (var...) body...)}
            // The body is carried by a real core_lambda so that binding
            // reuses `let`'s exact lowering -- and therefore step L16's
            // special-variable rule -- rather than restating it; see
            // core_multiple_value_bind's docs.
            if (lst.elements.size() < 3)
                return smdscheme::foundation::parse_error{
                    {},
                    "multiple-value-bind: expected a variable list and a "
                    "values form"};

            auto const &vars_node = datum_arena.get(lst.elements[1]);
            if (!std::holds_alternative<DatumList>(vars_node.inner))
                return smdscheme::foundation::parse_error{
                    {}, "multiple-value-bind: variables must be a list"};
            auto const &vars = std::get<DatumList>(vars_node.inner);

            core_lambda<core, MaxNodes, MaxList> lam{};
            for (int i = 0; i < vars.elements.size(); ++i) {
                auto const &v = datum_arena.get(vars.elements[i]);
                if (!std::holds_alternative<reader::datum_symbol>(v.inner))
                    return smdscheme::foundation::parse_error{
                        {}, "multiple-value-bind: variable must be a symbol"};
                auto v_name =
                    std::get<reader::datum_symbol>(v.inner).name.view();
                for (auto const &existing : lam.params) {
                    if (existing == v_name)
                        return smdscheme::foundation::parse_error{
                            {}, "multiple-value-bind: duplicate variable"};
                }
                lam.params.push_back(v_name);
            }

            auto values_r = elaborate_node<MaxNodes, MaxList>(
                datum_arena.get(lst.elements[2]), datum_arena, core_arena,
                scope);
            if (!values_r.has_value())
                return values_r;

            for (int i = 3; i < lst.elements.size(); ++i) {
                auto body_r = elaborate_node<MaxNodes, MaxList>(
                    datum_arena.get(lst.elements[i]), datum_arena, core_arena,
                    scope);
                if (!body_r.has_value())
                    return body_r;
                lam.body.push_back(smdscheme::foundation::make_arena_box(
                    core_arena, std::move(body_r.value())));
            }
            // A zero-form body is legal and evaluates to nil, exactly as
            // `block`'s and `catch`'s are.
            if (lam.body.empty())
                lam.body.push_back(smdscheme::foundation::make_arena_box(
                    core_arena, core{core_f{core_nil{}}}));

            auto values_box = smdscheme::foundation::make_arena_box(
                core_arena, std::move(values_r.value()));
            auto lambda_box = smdscheme::foundation::make_arena_box(
                core_arena, core{core_f{std::move(lam)}});
            return core{core_f{core_multiple_value_bind<core, MaxNodes>{
                values_box, lambda_box}}};
        }

        if (name == "LET") {
            // (let ((name expr)...) body...)
            //   -> ((lambda (name...) body...) expr...)
            if (lst.elements.size() < 3)
                return smdscheme::foundation::parse_error{
                    {},
                    "let: expected bindings and at least one body "
                    "expression"};

            auto const &bindings_node = datum_arena.get(lst.elements[1]);
            if (!std::holds_alternative<DatumList>(bindings_node.inner))
                return smdscheme::foundation::parse_error{
                    {}, "let: bindings must be a list"};
            auto const &bindings = std::get<DatumList>(bindings_node.inner);

            using DatumHandle = smdscheme::foundation::arena_box<
                reader::datum_type<MaxNodes, MaxList>, MaxNodes>;
            core_lambda<core, MaxNodes, MaxList> lam{};
            smdscheme::foundation::static_vector<DatumHandle, MaxList> arg_ids;

            for (int i = 0; i < bindings.elements.size(); ++i) {
                auto const &pair_node = datum_arena.get(bindings.elements[i]);
                if (!std::holds_alternative<DatumList>(pair_node.inner))
                    return smdscheme::foundation::parse_error{
                        {}, "let: each binding must be a list"};
                auto const &pair = std::get<DatumList>(pair_node.inner);
                if (pair.elements.size() != 2)
                    return smdscheme::foundation::parse_error{
                        {}, "let: each binding must have 2 elements"};

                auto const &name_node = datum_arena.get(pair.elements[0]);
                if (!std::holds_alternative<reader::datum_symbol>(
                        name_node.inner))
                    return smdscheme::foundation::parse_error{
                        {}, "let: binding name must be a symbol"};
                auto p_name =
                    std::get<reader::datum_symbol>(name_node.inner).name.view();

                for (auto const &existing : lam.params) {
                    if (existing == p_name)
                        return smdscheme::foundation::parse_error{
                            {}, "let: duplicate binding name"};
                }
                lam.params.push_back(p_name);
                arg_ids.push_back(pair.elements[1]);
            }

            for (int i = 2; i < lst.elements.size(); ++i) {
                auto body_r = elaborate_node<MaxNodes, MaxList>(
                    datum_arena.get(lst.elements[i]), datum_arena, core_arena,
                    scope);
                if (!body_r.has_value())
                    return body_r;
                lam.body.push_back(smdscheme::foundation::make_arena_box(
                    core_arena, std::move(body_r.value())));
            }

            core_application<core, MaxNodes, MaxList> app{};
            app.func = smdscheme::foundation::make_arena_box(
                core_arena,
                core{core_f{core_function<core, MaxNodes>{
                    smdscheme::foundation::make_arena_box(
                        core_arena, core{core_f{std::move(lam)}})}}});

            for (int i = 0; i < arg_ids.size(); ++i) {
                auto arg_r = elaborate_node<MaxNodes, MaxList>(
                    datum_arena.get(arg_ids[i]), datum_arena, core_arena,
                    scope);
                if (!arg_r.has_value())
                    return arg_r;
                app.args.push_back(smdscheme::foundation::make_arena_box(
                    core_arena, std::move(arg_r.value())));
            }

            return core{core_f{std::move(app)}};
        }

        if (name == "LET*") {
            // (let* () body...) -> (progn body...)
            // (let* ((n1 e1) (n2 e2) ...) body...)
            //   -> ((lambda (n1) ((lambda (n2) ... body...) e2)) e1)
            if (lst.elements.size() < 3)
                return smdscheme::foundation::parse_error{
                    {},
                    "let*: expected bindings and at least one body "
                    "expression"};

            auto const &bindings_node = datum_arena.get(lst.elements[1]);
            if (!std::holds_alternative<DatumList>(bindings_node.inner))
                return smdscheme::foundation::parse_error{
                    {}, "let*: bindings must be a list"};
            auto const &bindings = std::get<DatumList>(bindings_node.inner);

            if (bindings.elements.empty()) {
                core_progn<core, MaxNodes, MaxList> seq{};
                for (int i = 2; i < lst.elements.size(); ++i) {
                    auto body_r = elaborate_node<MaxNodes, MaxList>(
                        datum_arena.get(lst.elements[i]), datum_arena,
                        core_arena, scope);
                    if (!body_r.has_value())
                        return body_r;
                    seq.exprs.push_back(smdscheme::foundation::make_arena_box(
                        core_arena, std::move(body_r.value())));
                }
                return core{core_f{std::move(seq)}};
            }

            using DatumHandle = smdscheme::foundation::arena_box<
                reader::datum_type<MaxNodes, MaxList>, MaxNodes>;
            smdscheme::foundation::static_vector<std::string_view, MaxList>
                names;
            smdscheme::foundation::static_vector<DatumHandle, MaxList> vals;

            for (int i = 0; i < bindings.elements.size(); ++i) {
                auto const &pair_node = datum_arena.get(bindings.elements[i]);
                if (!std::holds_alternative<DatumList>(pair_node.inner))
                    return smdscheme::foundation::parse_error{
                        {}, "let*: each binding must be a list"};
                auto const &pair = std::get<DatumList>(pair_node.inner);
                if (pair.elements.size() != 2)
                    return smdscheme::foundation::parse_error{
                        {}, "let*: each binding must have 2 elements"};

                auto const &bname_node = datum_arena.get(pair.elements[0]);
                if (!std::holds_alternative<reader::datum_symbol>(
                        bname_node.inner))
                    return smdscheme::foundation::parse_error{
                        {}, "let*: binding name must be a symbol"};

                names.push_back(std::get<reader::datum_symbol>(bname_node.inner)
                                    .name.view());
                vals.push_back(pair.elements[1]);
            }

            // Innermost layer: a lambda over the last binding's name,
            // carrying the full (implicit-progn) body.
            core_lambda<core, MaxNodes, MaxList> innermost{};
            innermost.params.push_back(names[names.size() - 1]);
            for (int i = 2; i < lst.elements.size(); ++i) {
                auto body_r = elaborate_node<MaxNodes, MaxList>(
                    datum_arena.get(lst.elements[i]), datum_arena, core_arena,
                    scope);
                if (!body_r.has_value())
                    return body_r;
                innermost.body.push_back(smdscheme::foundation::make_arena_box(
                    core_arena, std::move(body_r.value())));
            }

            auto last_val_r = elaborate_node<MaxNodes, MaxList>(
                datum_arena.get(vals[vals.size() - 1]), datum_arena, core_arena,
                scope);
            if (!last_val_r.has_value())
                return last_val_r;

            core_application<core, MaxNodes, MaxList> innermost_app{};
            innermost_app.func = smdscheme::foundation::make_arena_box(
                core_arena,
                core{core_f{core_function<core, MaxNodes>{
                    smdscheme::foundation::make_arena_box(
                        core_arena, core{core_f{std::move(innermost)}})}}});
            innermost_app.args.push_back(smdscheme::foundation::make_arena_box(
                core_arena, std::move(last_val_r.value())));

            auto nested = smdscheme::foundation::result<core>{
                core{core_f{std::move(innermost_app)}}};

            for (int i = names.size() - 2; i >= 0; --i) {
                auto val_r = elaborate_node<MaxNodes, MaxList>(
                    datum_arena.get(vals[i]), datum_arena, core_arena, scope);
                if (!val_r.has_value())
                    return val_r;

                core_lambda<core, MaxNodes, MaxList> lam{};
                lam.params.push_back(names[i]);
                lam.body.push_back(smdscheme::foundation::make_arena_box(
                    core_arena, std::move(nested.value())));

                core_application<core, MaxNodes, MaxList> app{};
                app.func = smdscheme::foundation::make_arena_box(
                    core_arena,
                    core{core_f{core_function<core, MaxNodes>{
                        smdscheme::foundation::make_arena_box(
                            core_arena, core{core_f{std::move(lam)}})}}});
                app.args.push_back(smdscheme::foundation::make_arena_box(
                    core_arena, std::move(val_r.value())));

                nested = smdscheme::foundation::result<core>{
                    core{core_f{std::move(app)}}};
            }

            return nested;
        }
    }

    // Ordinary application: the head resolves in the FUNCTION namespace
    // (decision D4), or is itself a (lambda ...) expression.
    auto func_r = elaborate_function_position<MaxNodes, MaxList>(
        first, datum_arena, core_arena,
        "application: operator position must be a function name or a "
        "lambda expression",
        scope);
    if (!func_r.has_value())
        return func_r;

    core_application<core, MaxNodes, MaxList> app{};
    app.func = smdscheme::foundation::make_arena_box(core_arena,
                                                     std::move(func_r.value()));

    for (int i = 1; i < lst.elements.size(); ++i) {
        auto arg_r = elaborate_node<MaxNodes, MaxList>(
            datum_arena.get(lst.elements[i]), datum_arena, core_arena, scope);
        if (!arg_r.has_value())
            return arg_r;
        app.args.push_back(smdscheme::foundation::make_arena_box(
            core_arena, std::move(arg_r.value())));
    }

    return core{core_f{std::move(app)}};
}

/// Elaborates a single datum node into a core node.
///
/// Integer, keyword, quote, and sharpsign-quote datums map directly.
/// Symbol datums map to @ref core_nil / @ref core_true for the `NIL`/`T`
/// spellings (decision D3) and to a VARIABLE-namespace @ref core_symbol
/// reference otherwise. List datums are dispatched to @ref elaborate_list.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
/// @param  d           The datum to elaborate.
/// @param  datum_arena Read-only datum arena.
/// @param  core_arena  Core arena receiving elaborated nodes.
/// @param  scope Lexically visible block names and tagbody tags (see
///                      @ref lexical_scope).
template <int MaxNodes, int MaxList>
constexpr auto elaborate_node(
    reader::datum_type<MaxNodes, MaxList> const &d,
    smdscheme::foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>,
                                      MaxNodes> const &datum_arena,
    smdscheme::foundation::tree_arena<core_type<MaxNodes, MaxList>, MaxNodes>
        &core_arena,
    lexical_scope<MaxList> const &scope)
    -> smdscheme::foundation::result<core_type<MaxNodes, MaxList>> {
    using core = core_type<MaxNodes, MaxList>;
    using core_f =
        typename core_f_factory<MaxNodes, MaxList>::template type<core>;
    using DatumT = reader::datum_type<MaxNodes, MaxList>;

    if (std::holds_alternative<reader::datum_integer>(d.inner)) {
        return core{core_f{
            core_integer{std::get<reader::datum_integer>(d.inner).value}}};
    }
    if (std::holds_alternative<reader::datum_symbol>(d.inner)) {
        // Copy the folded_name (not a view into it): d may be the root
        // datum returned by read_datum, which is not itself arena-backed
        // (see core_symbol's docs for why this matters).
        auto const &sym = std::get<reader::datum_symbol>(d.inner);
        if (sym.name.view() == "NIL")
            return core{core_f{core_nil{}}};
        if (sym.name.view() == "T")
            return core{core_f{core_true{}}};
        return core{core_f{core_symbol{sym.name}}};
    }
    if (std::holds_alternative<reader::datum_keyword>(d.inner)) {
        return core{core_f{
            core_keyword{std::get<reader::datum_keyword>(d.inner).name}}};
    }
    if (std::holds_alternative<reader::datum_quote<DatumT, MaxNodes>>(
            d.inner)) {
        auto const &q =
            std::get<reader::datum_quote<DatumT, MaxNodes>>(d.inner);
        return elaborate_quoted_datum<MaxNodes, MaxList>(
            datum_arena.get(q.quoted), datum_arena, core_arena);
    }
    if (std::holds_alternative<reader::datum_function<DatumT, MaxNodes>>(
            d.inner)) {
        auto const &fq =
            std::get<reader::datum_function<DatumT, MaxNodes>>(d.inner);
        return elaborate_function_position<MaxNodes, MaxList>(
            datum_arena.get(fq.target), datum_arena, core_arena,
            "function: expected a function name or a lambda expression", scope);
    }
    if (std::holds_alternative<reader::datum_list<DatumT, MaxNodes, MaxList>>(
            d.inner)) {
        return elaborate_list<MaxNodes, MaxList>(
            std::get<reader::datum_list<DatumT, MaxNodes, MaxList>>(d.inner),
            datum_arena, core_arena, scope);
    }

    return smdscheme::foundation::parse_error{
        {}, "elaborator: unsupported node type"};
}

} // namespace detail

/// Elaborates a parsed Common Lisp datum into the core AST.
///
/// This is the public entry point for the elaboration phase: it converts
/// the raw datum from the reader into a typed core expression, classifying
/// the special operators `quote`, `if`, `progn`, `let`, `let*`, `lambda`,
/// `function`, `block`, `return-from`, `catch`, `throw`,
/// `unwind-protect`, `multiple-value-bind`, `tagbody`, and `go`, and emitting
/// errors for malformed input. Elaboration begins with empty lexical scopes
/// (@ref detail::lexical_scope) -- see that type's docs for why
/// `return-from` validation is a purely lexical, elaboration-time decision
/// (decision D5).
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list/argument length.
/// @param  pd          Root datum to elaborate.
/// @param  datum_arena Read-only datum arena produced by the reader.
/// @param  core_arena  Core arena that receives elaborated nodes.
/// @return A @ref smdscheme::foundation::result holding the root core node,
///         or a @ref smdscheme::foundation::parse_error describing the
///         first failure.
template <int MaxNodes, int MaxList>
constexpr auto elaborate(
    reader::datum_type<MaxNodes, MaxList> const &pd,
    smdscheme::foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>,
                                      MaxNodes> const &datum_arena,
    smdscheme::foundation::tree_arena<core_type<MaxNodes, MaxList>, MaxNodes>
        &core_arena)
    -> smdscheme::foundation::result<core_type<MaxNodes, MaxList>> {
    detail::lexical_scope<MaxList> const empty_scope{};
    auto r = detail::elaborate_node<MaxNodes, MaxList>(pd, datum_arena,
                                                       core_arena, empty_scope);
    if (!r.has_value())
        return r.error();
    return r.value();
}

} // namespace smd::smdlisp::elaborator

#endif
