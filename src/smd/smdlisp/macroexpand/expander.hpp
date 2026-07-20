// src/smd/smdlisp/macroexpand/expander.hpp                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_MACROEXPAND_EXPANDER_HPP
#define SRC_SMD_SMDLISP_MACROEXPAND_EXPANDER_HPP

#include <smd/smdlisp/elaborator/elaborate.hpp>
#include <smd/smdlisp/elaborator/elaborated_core.hpp>
#include <smd/smdlisp/reader/atom.hpp>
#include <smd/smdlisp/reader/datum_type.hpp>
#include <smd/smdscheme/foundation/arena_box.hpp>
#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/foundation/static_vector.hpp>

#include <string_view>
#include <utility>
#include <variant>

namespace smd::smdlisp::macroexpand {

/// Decision D9 (docs/cl-pivot-plan.md): macro expansion is a separate
/// datum-to-datum pass that runs between the reader and the elaborator. This
/// namespace operates entirely on @ref reader::datum_type trees (L6);
/// nothing here constructs or inspects an @ref elaborator::core_type node,
/// except @ref expand_and_elaborate, which only calls @ref
/// elaborator::elaborate as the next pipeline stage once expansion has
/// finished.

/// The arena type expansion allocates new nodes into: the same
/// @ref smdscheme::foundation::tree_arena the reader already populated (see
/// @ref expand's docs for why reusing that arena, rather than allocating a
/// second one, is the chosen design).
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
using arena_type =
    smdscheme::foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>,
                                      MaxNodes>;

/// A handle into @ref arena_type.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
using handle_type =
    smdscheme::foundation::arena_box<reader::datum_type<MaxNodes, MaxList>,
                                     MaxNodes>;

/// The datum-list shape of a macro call form, e.g. the whole
/// `(cond (nil 1) (t 2))` list, including its leading operator symbol.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
using call_form = reader::datum_list<reader::datum_type<MaxNodes, MaxList>,
                                     MaxNodes, MaxList>;

/// A host macro's expansion function: a plain C++ function from a call-form
/// datum to a replacement datum, writing any newly constructed nodes into
/// @p arena (per the plan's sketch, docs/cl-pivot-plan.md step L17).
///
/// This is a real function pointer, not a type-erased callable: host macros
/// are ordinary, stateless, `constexpr`-callable C++ functions, so a
/// function pointer is sufficient and keeps @ref host_macro trivially
/// constructible and constexpr-friendly.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
using macro_fn =
    smdscheme::foundation::result<reader::datum_type<MaxNodes, MaxList>> (*)(
        call_form<MaxNodes, MaxList> const &call,
        arena_type<MaxNodes, MaxList> &arena);

/// One registry entry: a macro's folded name and its expansion function.
///
/// Matching a call form's head symbol against @ref name is a plain
/// @c std::string_view comparison against the head symbol's folded
/// spelling (@ref reader::folded_name::view), the same pattern
/// @c detail::elaborate_list already uses for special-form dispatch
/// (`elaborate.hpp`) -- reused here rather than inventing a second
/// symbol-matching mechanism, per the plan's explicit guidance.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
struct host_macro {
    std::string_view name{}; ///< Folded operator spelling, e.g. `"COND"`.
    macro_fn<MaxNodes, MaxList> expand{}; ///< The expansion function.
};

/// Maximum number of host macros a @ref macro_table can hold.
inline constexpr int max_host_macros = 16;

/// A fixed-capacity registry of host macros, keyed by symbol.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
using macro_table =
    smdscheme::foundation::static_vector<host_macro<MaxNodes, MaxList>,
                                         max_host_macros>;

/// Default budget for @ref macroexpand's fixpoint loop. Exhausting the
/// budget is a diagnosed @ref smdscheme::foundation::parse_error, not an
/// infinite loop -- per the plan's explicit instruction.
inline constexpr int default_max_expansions = 64;

/// The result of one @ref macroexpand_1 step: the (possibly unchanged)
/// datum, and whether an expansion actually happened.
///
/// Mirrors ANSI CL's `macroexpand-1`, which returns two values (the
/// expansion and a generalized boolean "expanded at all" flag) for exactly
/// this reason: a caller iterating to a fixpoint needs to know when to stop
/// without re-deriving that fact from whether the datum happened to compare
/// equal (datum trees here have no @c operator==).
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
struct macroexpand_step {
    reader::datum_type<MaxNodes, MaxList>
        value;            ///< The (possibly expanded) datum.
    bool expanded{false}; ///< True if a macro actually fired this step.
};

/// Applies at most one macro-expansion step at @p d's head position.
///
/// If @p d is a list whose head is a symbol matching a @ref host_macro in
/// @p table, calls that macro's expansion function once and returns its
/// result with @c expanded == true. Otherwise returns @p d unchanged with
/// @c expanded == false: @p d is not a list, is an empty list, its head is
/// not a symbol, or its head symbol does not name a registered macro.
///
/// This does not recurse into @p d's substructure; see @ref macroexpand for
/// the head-position fixpoint and @ref expand for the whole-tree walk that
/// wires this into the read -> expand -> elaborate pipeline.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
/// @param  d     The datum to inspect.
/// @param  arena Arena that back @p d and that receives any newly
///               constructed replacement nodes.
/// @param  table The macro registry to match against.
template <int MaxNodes, int MaxList>
constexpr auto macroexpand_1(reader::datum_type<MaxNodes, MaxList> const &d,
                             arena_type<MaxNodes, MaxList> &arena,
                             macro_table<MaxNodes, MaxList> const &table)
    -> smdscheme::foundation::result<macroexpand_step<MaxNodes, MaxList>> {
    using DatumT = reader::datum_type<MaxNodes, MaxList>;
    using DatumList = reader::datum_list<DatumT, MaxNodes, MaxList>;

    if (!std::holds_alternative<DatumList>(d.inner))
        return macroexpand_step<MaxNodes, MaxList>{d, false};

    auto const &lst = std::get<DatumList>(d.inner);
    if (lst.elements.empty())
        return macroexpand_step<MaxNodes, MaxList>{d, false};

    auto const &head = arena.get(lst.elements[0]);
    if (!std::holds_alternative<reader::datum_symbol>(head.inner))
        return macroexpand_step<MaxNodes, MaxList>{d, false};

    auto name = std::get<reader::datum_symbol>(head.inner).name.view();
    for (int i = 0; i < table.size(); ++i) {
        if (table[i].name == name) {
            auto r = table[i].expand(lst, arena);
            if (!r.has_value())
                return r.error();
            return macroexpand_step<MaxNodes, MaxList>{r.value(), true};
        }
    }
    return macroexpand_step<MaxNodes, MaxList>{d, false};
}

/// Iterates @ref macroexpand_1 at @p d's head position to a fixpoint.
///
/// Mirrors ANSI CL's `macroexpand`: repeatedly re-checks whether the
/// *current* result's head is itself a macro call (this is how, e.g., a
/// macro that expands into a call of another macro gets fully unwound at
/// the head position) and stops as soon as a step reports
/// @c expanded == false. Exhausting @p max_expansions without reaching a
/// fixpoint is a diagnosed error, never a hang -- a host macro that
/// (incorrectly) expands into another call of itself unconditionally would
/// otherwise loop forever.
///
/// This still only ever inspects the *current* expression's own head; it
/// does not walk into substructure. See @ref expand for the whole-tree
/// pass that also recurses.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
/// @param  d              The datum to expand.
/// @param  arena          Arena backing @p d and receiving new nodes.
/// @param  table          The macro registry to match against.
/// @param  max_expansions Budget on the number of expansion steps.
template <int MaxNodes, int MaxList>
constexpr auto macroexpand(reader::datum_type<MaxNodes, MaxList> const &d,
                           arena_type<MaxNodes, MaxList> &arena,
                           macro_table<MaxNodes, MaxList> const &table,
                           int max_expansions = default_max_expansions)
    -> smdscheme::foundation::result<reader::datum_type<MaxNodes, MaxList>> {
    reader::datum_type<MaxNodes, MaxList> current = d;
    for (int i = 0; i < max_expansions; ++i) {
        auto step_r = macroexpand_1<MaxNodes, MaxList>(current, arena, table);
        if (!step_r.has_value())
            return step_r.error();
        if (!step_r.value().expanded)
            return step_r.value().value;
        current = step_r.value().value;
    }
    return smdscheme::foundation::parse_error{
        {}, "macroexpand: expansion budget exceeded"};
}

namespace detail {

/// Builds a fresh symbol datum with an already-uppercase static spelling.
///
/// Mirrors @c elaborator::detail::literal_symbol: folding a literal like
/// `"IF"` is a no-op, but going through @ref reader::detail::fold keeps one
/// definition of "how a folded name is built" rather than hand-filling
/// @ref reader::folded_name's storage array in every macro below.
template <int MaxNodes, int MaxList>
constexpr auto make_symbol(std::string_view spelling)
    -> reader::datum_type<MaxNodes, MaxList> {
    using DatumT = reader::datum_type<MaxNodes, MaxList>;
    using datum_f =
        typename reader::datum_f_factory<MaxNodes,
                                         MaxList>::template type<DatumT>;
    return DatumT{
        datum_f{reader::datum_symbol{reader::detail::fold(spelling)}}};
}

/// Allocates @ref make_symbol's result into @p arena and returns a handle.
template <int MaxNodes, int MaxList>
constexpr auto make_symbol_box(arena_type<MaxNodes, MaxList> &arena,
                               std::string_view spelling)
    -> handle_type<MaxNodes, MaxList> {
    return smdscheme::foundation::make_arena_box(
        arena, make_symbol<MaxNodes, MaxList>(spelling));
}

} // namespace detail

// -- host macros: cond, when, unless, and, or, case ------------------------
//
// Each of these expands to `if`/`progn`/`let` (and, for `or`/`case`, to
// calls of the *other* host macros here), matching their real status in
// ANSI Common Lisp: none of the six is ever a special operator. Every
// implementation below peels off exactly one clause/argument per call and
// leaves the rest as a fresh call of the same (or a related) macro,
// relying on @ref expand's recursive whole-tree walk to keep unwinding the
// result -- none of the six needs to build its own multi-level expansion by
// hand, which is the API-shape sanity check the plan asks for.

/// `(cond (test1 form1...) (test2 form2...) ...)` ->
/// `(if test1 (progn form1...) (cond (test2 form2...) ...))`, bottoming out
/// in `nil` for an empty clause list.
///
/// A clause with a test but no forms (`(cond (a))`, legal in full ANSI CL)
/// is not supported: this baseline only handles clauses with at least one
/// body form. See docs/divergences for the scope-cut record.
template <int MaxNodes, int MaxList>
constexpr auto cond_expand(call_form<MaxNodes, MaxList> const &call,
                           arena_type<MaxNodes, MaxList> &arena)
    -> smdscheme::foundation::result<reader::datum_type<MaxNodes, MaxList>> {
    using DatumT = reader::datum_type<MaxNodes, MaxList>;
    using datum_f =
        typename reader::datum_f_factory<MaxNodes,
                                         MaxList>::template type<DatumT>;
    using DatumList = reader::datum_list<DatumT, MaxNodes, MaxList>;

    if (call.elements.size() == 1)
        return detail::make_symbol<MaxNodes, MaxList>("NIL");

    auto const &clause_node = arena.get(call.elements[1]);
    if (!std::holds_alternative<DatumList>(clause_node.inner))
        return smdscheme::foundation::parse_error{
            {}, "cond: each clause must be a list"};
    auto const &clause = std::get<DatumList>(clause_node.inner);
    if (clause.elements.empty())
        return smdscheme::foundation::parse_error{
            {}, "cond: clause must have a test"};
    if (clause.elements.size() < 2)
        return smdscheme::foundation::parse_error{
            {}, "cond: a test-only clause (no body forms) is not supported"};

    DatumList body{};
    body.elements.push_back(
        detail::make_symbol_box<MaxNodes, MaxList>(arena, "PROGN"));
    for (int i = 1; i < clause.elements.size(); ++i)
        body.elements.push_back(clause.elements[i]);
    auto body_box = smdscheme::foundation::make_arena_box(
        arena, DatumT{datum_f{std::move(body)}});

    DatumList rest{};
    rest.elements.push_back(
        detail::make_symbol_box<MaxNodes, MaxList>(arena, "COND"));
    for (int i = 2; i < call.elements.size(); ++i)
        rest.elements.push_back(call.elements[i]);
    auto rest_box = smdscheme::foundation::make_arena_box(
        arena, DatumT{datum_f{std::move(rest)}});

    DatumList if_form{};
    if_form.elements.push_back(
        detail::make_symbol_box<MaxNodes, MaxList>(arena, "IF"));
    if_form.elements.push_back(clause.elements[0]);
    if_form.elements.push_back(body_box);
    if_form.elements.push_back(rest_box);
    return DatumT{datum_f{std::move(if_form)}};
}

/// `(when test form...)` -> `(if test (progn form...))`; `nil` body if
/// @c form... is empty.
template <int MaxNodes, int MaxList>
constexpr auto when_expand(call_form<MaxNodes, MaxList> const &call,
                           arena_type<MaxNodes, MaxList> &arena)
    -> smdscheme::foundation::result<reader::datum_type<MaxNodes, MaxList>> {
    using DatumT = reader::datum_type<MaxNodes, MaxList>;
    using datum_f =
        typename reader::datum_f_factory<MaxNodes,
                                         MaxList>::template type<DatumT>;
    using DatumList = reader::datum_list<DatumT, MaxNodes, MaxList>;

    if (call.elements.size() < 2)
        return smdscheme::foundation::parse_error{{}, "when: expected a test"};

    handle_type<MaxNodes, MaxList> then_box;
    if (call.elements.size() == 2) {
        then_box = detail::make_symbol_box<MaxNodes, MaxList>(arena, "NIL");
    } else {
        DatumList body{};
        body.elements.push_back(
            detail::make_symbol_box<MaxNodes, MaxList>(arena, "PROGN"));
        for (int i = 2; i < call.elements.size(); ++i)
            body.elements.push_back(call.elements[i]);
        then_box = smdscheme::foundation::make_arena_box(
            arena, DatumT{datum_f{std::move(body)}});
    }

    DatumList if_form{};
    if_form.elements.push_back(
        detail::make_symbol_box<MaxNodes, MaxList>(arena, "IF"));
    if_form.elements.push_back(call.elements[1]);
    if_form.elements.push_back(then_box);
    return DatumT{datum_f{std::move(if_form)}};
}

/// `(unless test form...)` -> `(if test nil (progn form...))`; `nil`
/// alternative if @c form... is empty.
template <int MaxNodes, int MaxList>
constexpr auto unless_expand(call_form<MaxNodes, MaxList> const &call,
                             arena_type<MaxNodes, MaxList> &arena)
    -> smdscheme::foundation::result<reader::datum_type<MaxNodes, MaxList>> {
    using DatumT = reader::datum_type<MaxNodes, MaxList>;
    using datum_f =
        typename reader::datum_f_factory<MaxNodes,
                                         MaxList>::template type<DatumT>;
    using DatumList = reader::datum_list<DatumT, MaxNodes, MaxList>;

    if (call.elements.size() < 2)
        return smdscheme::foundation::parse_error{{},
                                                  "unless: expected a test"};

    handle_type<MaxNodes, MaxList> else_box;
    if (call.elements.size() == 2) {
        else_box = detail::make_symbol_box<MaxNodes, MaxList>(arena, "NIL");
    } else {
        DatumList body{};
        body.elements.push_back(
            detail::make_symbol_box<MaxNodes, MaxList>(arena, "PROGN"));
        for (int i = 2; i < call.elements.size(); ++i)
            body.elements.push_back(call.elements[i]);
        else_box = smdscheme::foundation::make_arena_box(
            arena, DatumT{datum_f{std::move(body)}});
    }

    DatumList if_form{};
    if_form.elements.push_back(
        detail::make_symbol_box<MaxNodes, MaxList>(arena, "IF"));
    if_form.elements.push_back(call.elements[1]);
    if_form.elements.push_back(
        detail::make_symbol_box<MaxNodes, MaxList>(arena, "NIL"));
    if_form.elements.push_back(else_box);
    return DatumT{datum_f{std::move(if_form)}};
}

/// `(and)` -> `t`; `(and e)` -> `e`; `(and e1 e2...)` ->
/// `(if e1 (and e2...) nil)`.
template <int MaxNodes, int MaxList>
constexpr auto and_expand(call_form<MaxNodes, MaxList> const &call,
                          arena_type<MaxNodes, MaxList> &arena)
    -> smdscheme::foundation::result<reader::datum_type<MaxNodes, MaxList>> {
    using DatumT = reader::datum_type<MaxNodes, MaxList>;
    using datum_f =
        typename reader::datum_f_factory<MaxNodes,
                                         MaxList>::template type<DatumT>;
    using DatumList = reader::datum_list<DatumT, MaxNodes, MaxList>;

    int nargs = call.elements.size() - 1;
    if (nargs == 0)
        return detail::make_symbol<MaxNodes, MaxList>("T");
    if (nargs == 1)
        return arena.get(call.elements[1]);

    DatumList rest{};
    rest.elements.push_back(
        detail::make_symbol_box<MaxNodes, MaxList>(arena, "AND"));
    for (int i = 2; i < call.elements.size(); ++i)
        rest.elements.push_back(call.elements[i]);
    auto rest_box = smdscheme::foundation::make_arena_box(
        arena, DatumT{datum_f{std::move(rest)}});

    DatumList if_form{};
    if_form.elements.push_back(
        detail::make_symbol_box<MaxNodes, MaxList>(arena, "IF"));
    if_form.elements.push_back(call.elements[1]);
    if_form.elements.push_back(rest_box);
    if_form.elements.push_back(
        detail::make_symbol_box<MaxNodes, MaxList>(arena, "NIL"));
    return DatumT{datum_f{std::move(if_form)}};
}

/// `(or)` -> `nil`; `(or e)` -> `e`; `(or e1 e2...)` ->
/// `(let ((%OR-TMP e1)) (if %OR-TMP %OR-TMP (or e2...)))`.
///
/// Binds the first argument's value once via `let` rather than expanding
/// to `(if e1 e1 (or e2...))` directly, so `e1` is evaluated exactly once
/// even though its value is needed in two positions (the test and the
/// true branch) -- matching ANSI `or`'s single-evaluation contract.
///
/// The bound name (`%OR-TMP`) is a fixed spelling, not a gensym: this
/// macro expander has no notion of uninterned/fresh symbols yet (that
/// machinery is not built until L18/L19's backquote and `defmacro` work),
/// so this is not fully hygienic -- a user form that happens to reference
/// a variable literally named `%OR-TMP` inside an `or` argument could
/// observe unexpected capture. See docs/divergences for the recorded scope
/// cut; nested `or` expansions do not collide with each other (`let`
/// introduces a fresh scope at each level), only a user-supplied name would.
template <int MaxNodes, int MaxList>
constexpr auto or_expand(call_form<MaxNodes, MaxList> const &call,
                         arena_type<MaxNodes, MaxList> &arena)
    -> smdscheme::foundation::result<reader::datum_type<MaxNodes, MaxList>> {
    using DatumT = reader::datum_type<MaxNodes, MaxList>;
    using datum_f =
        typename reader::datum_f_factory<MaxNodes,
                                         MaxList>::template type<DatumT>;
    using DatumList = reader::datum_list<DatumT, MaxNodes, MaxList>;

    int nargs = call.elements.size() - 1;
    if (nargs == 0)
        return detail::make_symbol<MaxNodes, MaxList>("NIL");
    if (nargs == 1)
        return arena.get(call.elements[1]);

    DatumList rest{};
    rest.elements.push_back(
        detail::make_symbol_box<MaxNodes, MaxList>(arena, "OR"));
    for (int i = 2; i < call.elements.size(); ++i)
        rest.elements.push_back(call.elements[i]);
    auto rest_box = smdscheme::foundation::make_arena_box(
        arena, DatumT{datum_f{std::move(rest)}});

    auto tmp_box = detail::make_symbol_box<MaxNodes, MaxList>(arena, "%OR-TMP");

    DatumList binding_pair{};
    binding_pair.elements.push_back(tmp_box);
    binding_pair.elements.push_back(call.elements[1]);
    auto binding_pair_box = smdscheme::foundation::make_arena_box(
        arena, DatumT{datum_f{std::move(binding_pair)}});

    DatumList bindings{};
    bindings.elements.push_back(binding_pair_box);
    auto bindings_box = smdscheme::foundation::make_arena_box(
        arena, DatumT{datum_f{std::move(bindings)}});

    DatumList if_form{};
    if_form.elements.push_back(
        detail::make_symbol_box<MaxNodes, MaxList>(arena, "IF"));
    if_form.elements.push_back(tmp_box);
    if_form.elements.push_back(tmp_box);
    if_form.elements.push_back(rest_box);
    auto if_box = smdscheme::foundation::make_arena_box(
        arena, DatumT{datum_f{std::move(if_form)}});

    DatumList let_form{};
    let_form.elements.push_back(
        detail::make_symbol_box<MaxNodes, MaxList>(arena, "LET"));
    let_form.elements.push_back(bindings_box);
    let_form.elements.push_back(if_box);
    return DatumT{datum_f{std::move(let_form)}};
}

/// `(case keyform ((k1 k2...) form...)... (t form...))` ->
/// `(let ((%CASE-KEY keyform)) (cond ((eql %CASE-KEY 'k1) form...)... (t
/// form...)))`.
///
/// `keyform` is bound once via `let` (matching `or`'s single-evaluation
/// approach and its same non-hygienic scope cut, see @ref or_expand). Each
/// clause's key spec is compared with `eql` against literal (quoted) keys,
/// per ANSI `case` semantics: keys are data, never evaluated. A bare `t` or
/// `otherwise` key spec (not inside a list) is the catch-all clause,
/// matching ANSI CL; `(t ...)`/`(otherwise ...)` with the symbol *inside* a
/// list is an ordinary one-key clause instead, also matching ANSI CL.
template <int MaxNodes, int MaxList>
constexpr auto case_expand(call_form<MaxNodes, MaxList> const &call,
                           arena_type<MaxNodes, MaxList> &arena)
    -> smdscheme::foundation::result<reader::datum_type<MaxNodes, MaxList>> {
    using DatumT = reader::datum_type<MaxNodes, MaxList>;
    using datum_f =
        typename reader::datum_f_factory<MaxNodes,
                                         MaxList>::template type<DatumT>;
    using DatumList = reader::datum_list<DatumT, MaxNodes, MaxList>;

    if (call.elements.size() < 2)
        return smdscheme::foundation::parse_error{{},
                                                  "case: expected a key form"};

    auto tmp_box =
        detail::make_symbol_box<MaxNodes, MaxList>(arena, "%CASE-KEY");

    DatumList cond_call{};
    cond_call.elements.push_back(
        detail::make_symbol_box<MaxNodes, MaxList>(arena, "COND"));

    for (int i = 2; i < call.elements.size(); ++i) {
        auto const &clause_node = arena.get(call.elements[i]);
        if (!std::holds_alternative<DatumList>(clause_node.inner))
            return smdscheme::foundation::parse_error{
                {}, "case: each clause must be a list"};
        auto const &clause = std::get<DatumList>(clause_node.inner);
        if (clause.elements.empty())
            return smdscheme::foundation::parse_error{
                {}, "case: clause must have a key spec"};

        auto const &keyspec = arena.get(clause.elements[0]);
        handle_type<MaxNodes, MaxList> test_box;
        bool is_otherwise = false;
        if (std::holds_alternative<reader::datum_symbol>(keyspec.inner)) {
            auto name =
                std::get<reader::datum_symbol>(keyspec.inner).name.view();
            if (name == "T" || name == "OTHERWISE")
                is_otherwise = true;
        }

        if (is_otherwise) {
            test_box = detail::make_symbol_box<MaxNodes, MaxList>(arena, "T");
        } else if (std::holds_alternative<DatumList>(keyspec.inner)) {
            auto const &keys = std::get<DatumList>(keyspec.inner);
            if (keys.elements.empty())
                return smdscheme::foundation::parse_error{
                    {}, "case: key list must have at least one key"};
            DatumList or_call{};
            or_call.elements.push_back(
                detail::make_symbol_box<MaxNodes, MaxList>(arena, "OR"));
            for (int k = 0; k < keys.elements.size(); ++k) {
                DatumList quoted{};
                quoted.elements.push_back(
                    detail::make_symbol_box<MaxNodes, MaxList>(arena, "QUOTE"));
                quoted.elements.push_back(keys.elements[k]);
                auto quoted_box = smdscheme::foundation::make_arena_box(
                    arena, DatumT{datum_f{std::move(quoted)}});

                DatumList eql_call{};
                eql_call.elements.push_back(
                    detail::make_symbol_box<MaxNodes, MaxList>(arena, "EQL"));
                eql_call.elements.push_back(tmp_box);
                eql_call.elements.push_back(quoted_box);
                or_call.elements.push_back(
                    smdscheme::foundation::make_arena_box(
                        arena, DatumT{datum_f{std::move(eql_call)}}));
            }
            test_box = smdscheme::foundation::make_arena_box(
                arena, DatumT{datum_f{std::move(or_call)}});
        } else {
            DatumList quoted{};
            quoted.elements.push_back(
                detail::make_symbol_box<MaxNodes, MaxList>(arena, "QUOTE"));
            quoted.elements.push_back(clause.elements[0]);
            auto quoted_box = smdscheme::foundation::make_arena_box(
                arena, DatumT{datum_f{std::move(quoted)}});

            DatumList eql_call{};
            eql_call.elements.push_back(
                detail::make_symbol_box<MaxNodes, MaxList>(arena, "EQL"));
            eql_call.elements.push_back(tmp_box);
            eql_call.elements.push_back(quoted_box);
            test_box = smdscheme::foundation::make_arena_box(
                arena, DatumT{datum_f{std::move(eql_call)}});
        }

        DatumList new_clause{};
        new_clause.elements.push_back(test_box);
        for (int f = 1; f < clause.elements.size(); ++f)
            new_clause.elements.push_back(clause.elements[f]);
        cond_call.elements.push_back(smdscheme::foundation::make_arena_box(
            arena, DatumT{datum_f{std::move(new_clause)}}));
    }

    auto cond_box = smdscheme::foundation::make_arena_box(
        arena, DatumT{datum_f{std::move(cond_call)}});

    DatumList binding_pair{};
    binding_pair.elements.push_back(tmp_box);
    binding_pair.elements.push_back(call.elements[1]);
    auto binding_pair_box = smdscheme::foundation::make_arena_box(
        arena, DatumT{datum_f{std::move(binding_pair)}});

    DatumList bindings{};
    bindings.elements.push_back(binding_pair_box);
    auto bindings_box = smdscheme::foundation::make_arena_box(
        arena, DatumT{datum_f{std::move(bindings)}});

    DatumList let_form{};
    let_form.elements.push_back(
        detail::make_symbol_box<MaxNodes, MaxList>(arena, "LET"));
    let_form.elements.push_back(bindings_box);
    let_form.elements.push_back(cond_box);
    return DatumT{datum_f{std::move(let_form)}};
}

/// Builds the registry of the six baseline host macros this step lands:
/// `cond`, `when`, `unless`, `and`, `or`, `case`.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
constexpr auto default_macro_table() -> macro_table<MaxNodes, MaxList> {
    macro_table<MaxNodes, MaxList> table{};
    table.push_back(
        host_macro<MaxNodes, MaxList>{"COND", &cond_expand<MaxNodes, MaxList>});
    table.push_back(
        host_macro<MaxNodes, MaxList>{"WHEN", &when_expand<MaxNodes, MaxList>});
    table.push_back(host_macro<MaxNodes, MaxList>{
        "UNLESS", &unless_expand<MaxNodes, MaxList>});
    table.push_back(
        host_macro<MaxNodes, MaxList>{"AND", &and_expand<MaxNodes, MaxList>});
    table.push_back(
        host_macro<MaxNodes, MaxList>{"OR", &or_expand<MaxNodes, MaxList>});
    table.push_back(
        host_macro<MaxNodes, MaxList>{"CASE", &case_expand<MaxNodes, MaxList>});
    return table;
}

/// Recursively macro-expands every code position within @p d, writing any
/// newly constructed nodes into @p arena -- the same arena @p d's own
/// structure already lives in. Reusing the reader's datum arena (rather
/// than allocating a second one for expansion output) is the simplest
/// design that satisfies "the expander speaks the reader's datum language"
/// (docs/cl-pivot-plan.md step L17): every handle @ref expand produces or
/// consumes is an ordinary @ref handle_type into the one arena the caller
/// already owns.
///
/// At each list node, @ref macroexpand is applied at the head position to a
/// fixpoint first; the (possibly replaced) list is then walked into its own
/// elements, EXCEPT when the list's head symbol is `QUOTE`, in which case
/// the argument is literal data and is never macro-walked -- every Lisp
/// treats `quote`'s operand as data, not code, and macro-expanding inside
/// it would be a correctness bug (e.g. `'(cond a b)` must stay the literal
/// three-element list `(COND A B)`, not become `2`). A `#'`/`(function
/// ...)` node (@ref reader::datum_function) is walked into its embedded
/// lambda body when its target is a `(lambda ...)` form (code), and left
/// untouched when its target is a bare function-name symbol (nothing to
/// walk).
///
/// This is a generic, special-form-agnostic walk: it does not know the
/// argument shapes of `if`/`let`/`lambda`/etc. the way the elaborator does
/// (by design, per D9 -- macro expansion is independent of the elaborator),
/// so it cannot distinguish "this sublist is a binding/parameter list, not
/// code" from "this sublist is an ordinary subexpression". In the narrow
/// case where a form reuses one of the six macro names as a variable or
/// parameter name (e.g. a lambda formal literally named `cond`), this walk
/// may misinterpret an unrelated list as an attempted macro call. See
/// docs/divergences for the recorded scope cut; this is not exercised by
/// any of this step's merge-criteria tests.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
/// @param  d              The datum to expand.
/// @param  arena          Arena backing @p d and receiving new nodes.
/// @param  table          The macro registry to match against.
/// @param  max_expansions Budget passed through to every @ref macroexpand
///                        call this walk makes.
template <int MaxNodes, int MaxList>
constexpr auto expand(reader::datum_type<MaxNodes, MaxList> const &d,
                      arena_type<MaxNodes, MaxList> &arena,
                      macro_table<MaxNodes, MaxList> const &table,
                      int max_expansions = default_max_expansions)
    -> smdscheme::foundation::result<reader::datum_type<MaxNodes, MaxList>> {
    using DatumT = reader::datum_type<MaxNodes, MaxList>;
    using datum_f =
        typename reader::datum_f_factory<MaxNodes,
                                         MaxList>::template type<DatumT>;
    using DatumList = reader::datum_list<DatumT, MaxNodes, MaxList>;
    using DatumFunction = reader::datum_function<DatumT, MaxNodes>;

    if (std::holds_alternative<DatumFunction>(d.inner)) {
        auto const &fq = std::get<DatumFunction>(d.inner);
        auto const &target = arena.get(fq.target);
        if (!std::holds_alternative<DatumList>(target.inner))
            return d;
        auto inner_r =
            expand<MaxNodes, MaxList>(target, arena, table, max_expansions);
        if (!inner_r.has_value())
            return inner_r.error();
        auto new_target = smdscheme::foundation::make_arena_box(
            arena, std::move(inner_r.value()));
        return DatumT{datum_f{DatumFunction{new_target}}};
    }

    if (!std::holds_alternative<DatumList>(d.inner))
        return d;

    auto expanded_r =
        macroexpand<MaxNodes, MaxList>(d, arena, table, max_expansions);
    if (!expanded_r.has_value())
        return expanded_r.error();
    DatumT expanded = expanded_r.value();

    if (!std::holds_alternative<DatumList>(expanded.inner))
        return expanded;

    auto const &lst = std::get<DatumList>(expanded.inner);
    if (!lst.elements.empty()) {
        auto const &head = arena.get(lst.elements[0]);
        if (std::holds_alternative<reader::datum_symbol>(head.inner) &&
            std::get<reader::datum_symbol>(head.inner).name.view() == "QUOTE") {
            return expanded;
        }
    }

    DatumList new_list{};
    for (int i = 0; i < lst.elements.size(); ++i) {
        auto const &elem = arena.get(lst.elements[i]);
        auto elem_r =
            expand<MaxNodes, MaxList>(elem, arena, table, max_expansions);
        if (!elem_r.has_value())
            return elem_r.error();
        new_list.elements.push_back(smdscheme::foundation::make_arena_box(
            arena, std::move(elem_r.value())));
    }
    return DatumT{datum_f{std::move(new_list)}};
}

/// The public pipeline entry point: read -> **expand** -> elaborate.
///
/// Runs @ref expand over @p pd (writing into @p datum_arena, the same arena
/// the reader already populated), then hands the fully macro-expanded datum
/// tree to @ref elaborator::elaborate. No evaluator or elaborator changes
/// were needed to land this step: every one of the six baseline macros
/// bottoms out in `if`/`progn`/`let`/application, all already-elaborable
/// special forms.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
/// @param  pd             Root datum to expand and elaborate.
/// @param  datum_arena    Arena backing @p pd; also receives every node
///                        @ref expand allocates.
/// @param  core_arena     Core arena receiving the elaborator's output.
/// @param  table          The macro registry to expand against.
/// @param  max_expansions Budget passed through to @ref expand.
template <int MaxNodes, int MaxList>
constexpr auto expand_and_elaborate(
    reader::datum_type<MaxNodes, MaxList> const &pd,
    arena_type<MaxNodes, MaxList> &datum_arena,
    smdscheme::foundation::tree_arena<elaborator::core_type<MaxNodes, MaxList>,
                                      MaxNodes> &core_arena,
    macro_table<MaxNodes, MaxList> const &table,
    int max_expansions = default_max_expansions)
    -> smdscheme::foundation::result<elaborator::core_type<MaxNodes, MaxList>> {
    auto expanded_r =
        expand<MaxNodes, MaxList>(pd, datum_arena, table, max_expansions);
    if (!expanded_r.has_value())
        return expanded_r.error();
    return elaborator::elaborate<MaxNodes, MaxList>(expanded_r.value(),
                                                    datum_arena, core_arena);
}

} // namespace smd::smdlisp::macroexpand

#endif
