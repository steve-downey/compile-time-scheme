// src/smd/smdlisp/elaborator/elaborated_core.hpp                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_ELABORATOR_ELABORATED_CORE_HPP
#define SRC_SMD_SMDLISP_ELABORATOR_ELABORATED_CORE_HPP

#include <smd/smdlisp/reader/atom.hpp>
#include <smd/smdscheme/foundation/arena_box.hpp>
#include <smd/smdscheme/foundation/fix.hpp>
#include <smd/smdscheme/foundation/static_vector.hpp>

#include <string_view>
#include <variant>

namespace smd::smdlisp::elaborator {

/// A Common Lisp integer literal in the elaborated core.
struct core_integer {
    int value;
};

/// A VARIABLE-namespace reference (decision D4): an ordinary symbol used as
/// an expression, resolved among lexical/dynamic variable bindings, never
/// among function bindings.
///
/// Also reused, unchanged in shape, as the symbol-atom payload inside
/// @ref core_quote: a quoted symbol datum is "the symbol named @c name",
/// the same data whether it denotes a variable reference or quoted data.
///
/// @ref name is a @ref reader::folded_name (own storage), not a
/// @c std::string_view, even though the Scheme elaborator's equivalent node
/// is a bare @c string_view. This is a forced choice, not a style
/// preference: unlike the Scheme reader, @c smd::smdlisp::reader::read_datum
/// returns its *root* datum by value rather than through the arena (only a
/// datum's *children* — list elements, quote/sharpsign-quote targets — are
/// arena-allocated). A root datum that is itself a bare symbol/keyword
/// therefore owns its @c folded_name in a temporary that does not outlive
/// the read+elaborate call chain; a @c string_view into that temporary
/// would dangle the moment the caller's `read_datum(...)` result goes out
/// of scope; caught by AddressSanitizer's stack-use-after-return during
/// this step's own tests (`elab("foo")` / `elab(":foo")`, a bare top-level
/// atom with nothing to arena-allocate it). Copying the folded name instead
/// makes every @ref core_symbol / @ref core_keyword independent of the
/// datum tree's lifetime once elaboration returns, matching the same
/// contract @c datum_symbol/@c datum_keyword already keep (see
/// `reader/atom.hpp`'s @ref reader::folded_name docs) — and @c folded_name
/// remains trivially destructible, so this does not violate the
/// "core nodes are trivially destructible" architecture rule.
struct core_symbol {
    reader::folded_name name; ///< Symbol spelling, folded to uppercase per
                              ///< D2 (owned storage; see above).
};

/// A self-evaluating keyword (`:foo`, decision D7).
///
/// Also reused as the keyword-atom payload inside @ref core_quote, for the
/// same reason @ref core_symbol is reused there: a keyword is the same
/// data whether written bare or inside a quoted list.
///
/// @ref name is an owned @ref reader::folded_name for the same
/// dangling-view reason documented on @ref core_symbol.
struct core_keyword {
    reader::folded_name name; ///< Keyword spelling, folded per D2, colon
                              ///< stripped (owned storage; see
                              ///< @ref core_symbol).
};

/// The self-evaluating `nil` constant (decision D3): simultaneously the
/// sole false value and the empty list.
///
/// Distinct from @ref core_symbol so a bare `NIL` in source elaborates to a
/// constant rather than a VARIABLE-namespace lookup for a symbol named
/// `NIL`. Also the base case @ref detail::elaborate_quoted_datum constructs
/// for a quoted empty list (`'()`) and for a quoted `NIL` (`'nil`), since
/// both denote the identical `nil` value — there is no separate
/// representation for "quoted nil" versus "self-evaluating nil".
struct core_nil {};

/// The canonical `t` (true) constant (decision D3).
///
/// Distinct from @ref core_symbol for the same reason as @ref core_nil: `T`
/// is a self-evaluating constant, not a VARIABLE-namespace lookup. Later
/// steps (the evaluator, L11) are free to represent the runtime value
/// however the value model prefers (e.g. the symbol `T`); this core kind
/// only records that the source spelled the canonical true constant.
struct core_true {};

/// A quoted atom: a literal integer, symbol, or keyword that is data rather
/// than code at the call site.
///
/// Compound quoted data (lists) is not represented here; it is lowered to
/// @ref core_cons construction by @ref detail::elaborate_quoted_datum — the
/// same "construction, not a second literal-data representation" strategy
/// the Scheme elaborator's @c elaborate_quoted_datum uses. `NIL` and `T`
/// atoms are also not represented here; they elaborate directly to
/// @ref core_nil / @ref core_true regardless of quote context (see those
/// types' docs).
struct core_quote {
    std::variant<int, core_symbol, core_keyword> atom;
};

/// A hermetic pair-construction node, used only to build quoted list data
/// (e.g. the two `cons` cells inside `'(1 2)`).
///
/// This is the direct, scoped-down descendant of the Scheme elaborator's
/// @c core_prim / @c prim_op: quoted list data must construct the same
/// cons structure regardless of any later FUNCTION-namespace redefinition
/// of `cons` (ANSI CL: quoted data is literal, not a call to whatever
/// `cons` happens to be bound to). Unlike the Scheme original, this is not
/// a general primitive-dispatch node covering `car`/`cdr`/`eq`/etc.: step
/// L9 already installed those as ordinary FUNCTION-namespace @c builtin
/// values (`smd::smdlisp::closure::builtin_op`), so ordinary application
/// (`core_application` through `core_function`) is how a program calls
/// `cons`/`car`/`cdr` — only the constructor used internally to build
/// quoted list literals needs to be exempt from that lookup.
///
/// @tparam R        Recursive self-reference (the core fix-point type).
/// @tparam MaxNodes Arena capacity.
template <typename R, int MaxNodes>
struct core_cons {
    smdscheme::foundation::arena_box<R, MaxNodes>
        car; ///< The constructed cell's car.
    smdscheme::foundation::arena_box<R, MaxNodes>
        cdr; ///< The constructed cell's cdr.
};

/// A Common Lisp @c if form with condition, consequent, and alternative
/// branches.
///
/// A two-argument @c if (no alternative written) elaborates with an
/// implicit @ref core_nil alternative; @ref detail::elaborate_list
/// synthesizes that node, so this struct always holds three branches.
///
/// @tparam R        Recursive self-reference.
/// @tparam MaxNodes Arena capacity.
template <typename R, int MaxNodes>
struct core_if {
    smdscheme::foundation::arena_box<R, MaxNodes>
        condition; ///< The test expression.
    smdscheme::foundation::arena_box<R, MaxNodes>
        consequent; ///< Branch taken when the condition is true (D3:
                    ///< anything but `nil`).
    smdscheme::foundation::arena_box<R, MaxNodes>
        alternative; ///< Branch taken when the condition is `nil`.
};

/// A Common Lisp @c progn sequence: evaluates each expression in order and
/// yields the value of the last.
///
/// Also the node @c lambda/@c let/@c let* bodies with more than one
/// expression desugar through where a form needs to synthesize an implicit
/// progn (e.g. a zero-binding @c let*).
///
/// @tparam R        Recursive self-reference.
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum number of sequenced expressions.
template <typename R, int MaxNodes, int MaxList>
struct core_progn {
    smdscheme::foundation::static_vector<
        smdscheme::foundation::arena_box<R, MaxNodes>, MaxList>
        exprs; ///< The sequenced expressions (at least one).
};

/// A Common Lisp @c lambda form: a fixed parameter list and a body that is
/// an EXPRESSION SEQUENCE with implicit progn (at least one expression).
///
/// Unlike the Scheme elaborator's @c core_lambda (a single body
/// expression), @ref body is a sequence directly — there is no separate
/// wrapping @ref core_progn node for the common case, since the sequence
/// semantics are identical to progn's and every consumer (the evaluator)
/// needs to walk a body sequence for `lambda` regardless.
///
/// @tparam R        Recursive self-reference.
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum number of formal parameters or body
///                  expressions.
template <typename R, int MaxNodes, int MaxList>
struct core_lambda {
    smdscheme::foundation::static_vector<std::string_view, MaxList>
        params; ///< Formal parameter names (VARIABLE namespace).
    smdscheme::foundation::static_vector<
        smdscheme::foundation::arena_box<R, MaxNodes>, MaxList>
        body; ///< Body expressions, implicit progn (at least one).
};

/// A FUNCTION-namespace reference or an embedded lambda expression
/// (decision D4).
///
/// One node covers three source spellings that all denote "a function
/// value, resolved without evaluating an ordinary expression":
/// - a bare symbol used as an application head, e.g. the `f` in `(f x)`
///   (decision D4: application heads resolve in the FUNCTION namespace,
///   never the VARIABLE namespace @ref core_symbol denotes);
/// - `(function name)` or `#'name`, the same FUNCTION-namespace reference
///   written standalone rather than in application-head position;
/// - `(function (lambda ...))` or `#'(lambda ...)`, where @ref target holds
///   the embedded @ref core_lambda node directly rather than a name to look
///   up.
///
/// @ref detail::elaborate_function_position is the single place that
/// produces this node, used by @c function/@c #' elaboration, by ordinary
/// application-head elaboration, and by nothing else — ANSI CL permits only
/// a symbol or a lambda expression in function position.
///
/// @tparam R        Recursive self-reference.
/// @tparam MaxNodes Arena capacity.
template <typename R, int MaxNodes>
struct core_function {
    std::variant<std::string_view,
                 smdscheme::foundation::arena_box<R, MaxNodes>>
        target; ///< A FUNCTION-namespace name, or a handle to an embedded
                ///< @ref core_lambda node.
};

/// A function application: a function-position expression applied to
/// argument expressions.
///
/// @ref func always refers to a @ref core_function node (decision D4):
/// elaboration never leaves an application's function position as a bare
/// @ref core_symbol or as any other expression kind.
///
/// @tparam R        Recursive self-reference.
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum number of arguments.
template <typename R, int MaxNodes, int MaxList>
struct core_application {
    smdscheme::foundation::arena_box<R, MaxNodes>
        func; ///< Handle to a @ref core_function node.
    smdscheme::foundation::static_vector<
        smdscheme::foundation::arena_box<R, MaxNodes>, MaxList>
        args; ///< Argument expressions (VARIABLE-namespace context).
};

/// A Common Lisp `setq` assignment (step L12): mutates the nearest LEXICAL
/// variable binding for @ref name, returning the assigned value per ANSI CL
/// (not Scheme's `unspecified`) -- see
/// @ref smd::smdlisp::closure::env::set_value's docs for the mutation
/// mechanism (a shared @ref smd::smdlisp::closure::store, adapted from the
/// landed `set!`/@c store machinery in `smd::smdscheme::closure`) and for
/// why it errors rather than building any special-variable/dynamic-binding
/// behavior (explicitly deferred to step L16).
///
/// @ref name is a plain `std::string_view`, not an owned @ref
/// reader::folded_name like @ref core_symbol: `setq`'s name is always a
/// list ELEMENT (`(setq name expr)`), always reached through
/// `datum_arena.get(...)`, never the datum reader's un-arena-backed ROOT
/// return value -- the same "list elements are always arena-backed"
/// reasoning that already lets @ref core_function's bare-symbol
/// alternative and @ref core_lambda::params stay `string_view` (see this
/// file's L10 handoff note on that exact reasoning) applies here too.
///
/// @tparam R        Recursive self-reference.
/// @tparam MaxNodes Arena capacity.
template <typename R, int MaxNodes>
struct core_setq {
    std::string_view name; ///< The variable being assigned (VARIABLE
                           ///< namespace, lexical only -- see @ref
                           ///< smd::smdlisp::closure::env::set_value).
    smdscheme::foundation::arena_box<R, MaxNodes>
        value; ///< The new-value expression.
};

/// A top-level `defun` (step L12): binds @ref name in the FUNCTION
/// namespace (decision D4) to a closure over the lambda reached through
/// @ref lambda.
///
/// @ref lambda is built by reusing @ref detail::elaborate_lambda on a
/// synthetic formals/body view of the `defun` form's own elements (see
/// `elaborate.hpp`'s `DEFUN` case) rather than duplicating formals/body
/// parsing -- so @ref lambda always refers to an ordinary @ref core_lambda
/// node, exactly as if the same formals and body had been written inside a
/// literal `(lambda ...)` form.
///
/// @tparam R        Recursive self-reference.
/// @tparam MaxNodes Arena capacity.
template <typename R, int MaxNodes>
struct core_defun {
    std::string_view name; ///< Function name (FUNCTION namespace).
    smdscheme::foundation::arena_box<R, MaxNodes>
        lambda; ///< Handle to a @ref core_lambda node.
};

/// A top-level `defvar` (step L12): proclaims @ref name special (the mark
/// is stored now via @ref smd::smdlisp::closure::env::declare_special;
/// dynamic binding using it arrives in step L16) and, only if @ref name is
/// not already bound in the VARIABLE namespace, binds it to the evaluated
/// @ref value. Per ANSI CL, an already-bound `defvar` does not re-evaluate
/// or rebind @ref value -- see the evaluator's `core_defvar` case.
///
/// This baseline elaborator requires a value form (`(defvar name value)`);
/// the no-value-form (`(defvar name)`, proclaim-only) and docstring
/// variants ANSI CL also permits are out of scope for this step, matching
/// the established "in-scope subset, no divergence doc" pattern already
/// used for e.g. `lambda`'s "at least one body expression" restriction
/// (step L10).
///
/// @tparam R        Recursive self-reference.
/// @tparam MaxNodes Arena capacity.
template <typename R, int MaxNodes>
struct core_defvar {
    std::string_view name; ///< The variable being proclaimed special.
    smdscheme::foundation::arena_box<R, MaxNodes>
        value; ///< The initial-value expression (see restriction above).
};

/// A top-level `defparameter` (step L12): proclaims @ref name special (see
/// @ref core_defvar) and unconditionally (re)evaluates @ref value and
/// (re)binds @ref name in the VARIABLE namespace, unlike @ref core_defvar's
/// bind-only-if-unbound rule.
///
/// @tparam R        Recursive self-reference.
/// @tparam MaxNodes Arena capacity.
template <typename R, int MaxNodes>
struct core_defparameter {
    std::string_view name; ///< The variable being proclaimed special.
    smdscheme::foundation::arena_box<R, MaxNodes>
        value; ///< The (always evaluated) new-value expression.
};

/// Factory template that produces the open-recursive variant layer for the
/// core AST.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list/argument length.
template <int MaxNodes, int MaxList>
struct core_f_factory {
    /// One variant layer; @p R is the recursive self-reference.
    template <typename R>
    using type = std::variant<
        core_integer, core_symbol, core_keyword, core_nil, core_true,
        core_quote, core_cons<R, MaxNodes>, core_if<R, MaxNodes>,
        core_progn<R, MaxNodes, MaxList>, core_lambda<R, MaxNodes, MaxList>,
        core_function<R, MaxNodes>, core_application<R, MaxNodes, MaxList>,
        core_setq<R, MaxNodes>, core_defun<R, MaxNodes>,
        core_defvar<R, MaxNodes>, core_defparameter<R, MaxNodes>>;
};

/// The concrete recursive core AST type, formed as the fixed point of
/// @ref core_f_factory.
///
/// @tparam MaxNodes Maximum tree nodes (arena size).
/// @tparam MaxList  Maximum list/argument length.
template <int MaxNodes, int MaxList>
using core_type = smdscheme::foundation::fix<
    core_f_factory<MaxNodes, MaxList>::template type>;

/// The result of the elaboration phase: a core expression and its arena.
///
/// @c root holds the root node directly (not as a handle); child nodes
/// reference each other through @c arena.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list/argument length.
template <int MaxNodes, int MaxList>
struct elaborated_core {
    using core = core_type<MaxNodes, MaxList>;
    smdscheme::foundation::tree_arena<core, MaxNodes>
        arena; ///< Storage for all sub-nodes.
    core root; ///< The root expression.
};

} // namespace smd::smdlisp::elaborator
#endif
