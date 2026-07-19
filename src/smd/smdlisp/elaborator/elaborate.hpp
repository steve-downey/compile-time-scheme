// src/smd/smdlisp/elaborator/elaborate.hpp                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_ELABORATOR_ELABORATE_HPP
#define SRC_SMD_SMDLISP_ELABORATOR_ELABORATE_HPP

#include <smd/smdlisp/elaborator/elaborated_core.hpp>
#include <smd/smdlisp/reader/datum_type.hpp>
#include <smd/smdscheme/foundation/result.hpp>

namespace smd::smdlisp::elaborator {
namespace detail {

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
        &core_arena)
    -> smdscheme::foundation::result<core_type<MaxNodes, MaxList>>;

template <int MaxNodes, int MaxList>
constexpr auto elaborate_lambda(
    reader::datum_list<reader::datum_type<MaxNodes, MaxList>, MaxNodes,
                       MaxList> const &lst,
    smdscheme::foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>,
                                      MaxNodes> const &datum_arena,
    smdscheme::foundation::tree_arena<core_type<MaxNodes, MaxList>, MaxNodes>
        &core_arena)
    -> smdscheme::foundation::result<core_type<MaxNodes, MaxList>>;

template <int MaxNodes, int MaxList>
constexpr auto elaborate_function_position(
    reader::datum_type<MaxNodes, MaxList> const &d,
    smdscheme::foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>,
                                      MaxNodes> const &datum_arena,
    smdscheme::foundation::tree_arena<core_type<MaxNodes, MaxList>, MaxNodes>
        &core_arena,
    char const *error_message)
    -> smdscheme::foundation::result<core_type<MaxNodes, MaxList>>;

/// Elaborates the formals and body of a @c lambda form.
///
/// @p lst is the whole `(LAMBDA (params...) body...)` datum list, including
/// the leading `LAMBDA` symbol; this is shared by the standalone @c lambda
/// special form and by @ref elaborate_function_position's `(function
/// (lambda ...))` / `#'(lambda ...)` case, so both spellings of an embedded
/// lambda expression go through one implementation. The body is elaborated
/// as an EXPRESSION SEQUENCE with implicit progn: at least one body
/// expression is required (a zero-form body, though legal in full ANSI CL,
/// is out of scope for this baseline elaborator).
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
constexpr auto elaborate_lambda(
    reader::datum_list<reader::datum_type<MaxNodes, MaxList>, MaxNodes,
                       MaxList> const &lst,
    smdscheme::foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>,
                                      MaxNodes> const &datum_arena,
    smdscheme::foundation::tree_arena<core_type<MaxNodes, MaxList>, MaxNodes>
        &core_arena)
    -> smdscheme::foundation::result<core_type<MaxNodes, MaxList>> {
    using core = core_type<MaxNodes, MaxList>;
    using core_f =
        typename core_f_factory<MaxNodes, MaxList>::template type<core>;
    using DatumList = reader::datum_list<reader::datum_type<MaxNodes, MaxList>,
                                         MaxNodes, MaxList>;

    if (lst.elements.size() < 3)
        return smdscheme::foundation::parse_error{
            {}, "lambda: expected formals and at least one body expression"};

    auto const &formals_node = datum_arena.get(lst.elements[1]);
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

    for (int i = 2; i < lst.elements.size(); ++i) {
        auto body_r = elaborate_node<MaxNodes, MaxList>(
            datum_arena.get(lst.elements[i]), datum_arena, core_arena);
        if (!body_r.has_value())
            return body_r;
        lam.body.push_back(smdscheme::foundation::make_arena_box(
            core_arena, std::move(body_r.value())));
    }

    return core{core_f{std::move(lam)}};
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
template <int MaxNodes, int MaxList>
constexpr auto elaborate_function_position(
    reader::datum_type<MaxNodes, MaxList> const &d,
    smdscheme::foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>,
                                      MaxNodes> const &datum_arena,
    smdscheme::foundation::tree_arena<core_type<MaxNodes, MaxList>, MaxNodes>
        &core_arena,
    char const *error_message)
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
                    lst, datum_arena, core_arena);
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
/// `lambda`, and `function` by inspecting the leading symbol (folded
/// spellings, per decision D2). Any other head is an ordinary application,
/// whose head is elaborated through @ref elaborate_function_position (a
/// bare symbol resolves in the FUNCTION namespace; a `(lambda ...)` head is
/// also legal per ANSI CL). An empty list (`()`) as an application is a
/// compile-time error, not the empty-list value — quote it (`'()`) to get
/// @ref core_nil.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
/// @param  lst         The list datum to elaborate.
/// @param  datum_arena Read-only datum arena.
/// @param  core_arena  Core arena receiving elaborated nodes.
template <int MaxNodes, int MaxList>
constexpr auto elaborate_list(
    reader::datum_list<reader::datum_type<MaxNodes, MaxList>, MaxNodes,
                       MaxList> const &lst,
    smdscheme::foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>,
                                      MaxNodes> const &datum_arena,
    smdscheme::foundation::tree_arena<core_type<MaxNodes, MaxList>, MaxNodes>
        &core_arena)
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
                datum_arena.get(lst.elements[1]), datum_arena, core_arena);
            if (!cond_r.has_value())
                return cond_r;

            auto cons_r = elaborate_node<MaxNodes, MaxList>(
                datum_arena.get(lst.elements[2]), datum_arena, core_arena);
            if (!cons_r.has_value())
                return cons_r;

            smdscheme::foundation::arena_box<core, MaxNodes> alt_box;
            if (lst.elements.size() == 4) {
                auto alt_r = elaborate_node<MaxNodes, MaxList>(
                    datum_arena.get(lst.elements[3]), datum_arena, core_arena);
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
                    datum_arena.get(lst.elements[i]), datum_arena, core_arena);
                if (!expr_r.has_value())
                    return expr_r;
                seq.exprs.push_back(smdscheme::foundation::make_arena_box(
                    core_arena, std::move(expr_r.value())));
            }
            return core{core_f{std::move(seq)}};
        }

        if (name == "LAMBDA") {
            return elaborate_lambda<MaxNodes, MaxList>(lst, datum_arena,
                                                       core_arena);
        }

        if (name == "FUNCTION") {
            if (lst.elements.size() != 2)
                return smdscheme::foundation::parse_error{
                    {}, "function: expected 1 argument"};
            return elaborate_function_position<MaxNodes, MaxList>(
                datum_arena.get(lst.elements[1]), datum_arena, core_arena,
                "function: expected a function name or a lambda expression");
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
                    datum_arena.get(lst.elements[i]), datum_arena, core_arena);
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
                    datum_arena.get(arg_ids[i]), datum_arena, core_arena);
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
                        core_arena);
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
                    datum_arena.get(lst.elements[i]), datum_arena, core_arena);
                if (!body_r.has_value())
                    return body_r;
                innermost.body.push_back(smdscheme::foundation::make_arena_box(
                    core_arena, std::move(body_r.value())));
            }

            auto last_val_r = elaborate_node<MaxNodes, MaxList>(
                datum_arena.get(vals[vals.size() - 1]), datum_arena,
                core_arena);
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
                    datum_arena.get(vals[i]), datum_arena, core_arena);
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
        "lambda expression");
    if (!func_r.has_value())
        return func_r;

    core_application<core, MaxNodes, MaxList> app{};
    app.func = smdscheme::foundation::make_arena_box(core_arena,
                                                     std::move(func_r.value()));

    for (int i = 1; i < lst.elements.size(); ++i) {
        auto arg_r = elaborate_node<MaxNodes, MaxList>(
            datum_arena.get(lst.elements[i]), datum_arena, core_arena);
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
template <int MaxNodes, int MaxList>
constexpr auto elaborate_node(
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
            "function: expected a function name or a lambda expression");
    }
    if (std::holds_alternative<reader::datum_list<DatumT, MaxNodes, MaxList>>(
            d.inner)) {
        return elaborate_list<MaxNodes, MaxList>(
            std::get<reader::datum_list<DatumT, MaxNodes, MaxList>>(d.inner),
            datum_arena, core_arena);
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
/// and `function`, and emitting errors for malformed input.
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
    auto r =
        detail::elaborate_node<MaxNodes, MaxList>(pd, datum_arena, core_arena);
    if (!r.has_value())
        return r.error();
    return r.value();
}

} // namespace smd::smdlisp::elaborator

#endif
