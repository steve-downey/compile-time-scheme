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

/// A `setq` assignment: `(setq name1 expr1 name2 expr2 ...)`.
///
/// Adapted from the landed Scheme `core_set`/`set!` machinery (PR #23) as
/// the basis for `setq` (step L12, per `docs/cl-pivot-plan.md`). Unlike
/// Scheme's `set!` (a single name/value pair), ANSI CL's `setq` accepts any
/// number of name/value pairs, assigning each in turn, left to right, and
/// yielding the value of the *last* assignment -- @ref names and
/// @ref exprs are therefore parallel sequences rather than a single pair.
/// Each name always names a VARIABLE-namespace binding (decision D4);
/// `setq` never touches the function namespace. `setq` mutates the
/// nearest *lexical* binding only -- special-variable dynamic-rebinding
/// interaction is deferred to step L16, so a `setq` of a name marked
/// special by `defvar`/`defparameter` behaves identically to any other
/// lexical `setq` at this step (an evaluator-level error if the name is
/// not currently lexically bound, regardless of specialness).
///
/// @ref names holds plain @c string_view (not an owned @ref
/// reader::folded_name): every name here is a list element, never the
/// elaboration root, so it is always arena-backed -- the same reasoning
/// already applied to @ref core_lambda::params and @ref core_function's
/// symbol-name alternative (see those types' docs).
///
/// @tparam R        Recursive self-reference.
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum number of name/value pairs.
template <typename R, int MaxNodes, int MaxList>
struct core_setq {
    smdscheme::foundation::static_vector<std::string_view, MaxList>
        names; ///< VARIABLE-namespace targets, in assignment order.
    smdscheme::foundation::static_vector<
        smdscheme::foundation::arena_box<R, MaxNodes>, MaxList>
        exprs; ///< Value expressions, parallel to @ref names.
};

/// A top-level `defun` function definition: `(defun name (params...)
/// body...)`.
///
/// Defines @ref name in the FUNCTION namespace (decision D4) as a closure
/// over the environment in force when the `defun` form is evaluated.
/// @ref lambda_node holds the elaborated lambda -- built by the same
/// formals/body elaboration @ref detail::elaborate_lambda uses for an
/// ordinary `(lambda ...)` form (see @ref detail::elaborate_lambda_body),
/// so `defun` differs from `(function (lambda (params...) body...))` only
/// in that *evaluating* it also performs the FUNCTION-namespace definition
/// side effect (and, per ANSI CL, yields @ref name itself rather than the
/// closure value).
///
/// @ref name is a plain @c string_view: it is always a list element (the
/// second element of the `defun` form), never the elaboration root, so it
/// is always arena-backed (see @ref core_setq's docs for the identical
/// reasoning).
///
/// @tparam R        Recursive self-reference.
/// @tparam MaxNodes Arena capacity.
template <typename R, int MaxNodes>
struct core_defun {
    std::string_view name; ///< FUNCTION-namespace name being defined.
    smdscheme::foundation::arena_box<R, MaxNodes>
        lambda_node; ///< The elaborated `(lambda (params...) body...)`.
};

/// A `defvar`/`defparameter` special-variable declaration.
///
/// Defines @ref name in the VARIABLE namespace (decision D4) and marks it
/// special (the mark is recorded now, per step L12's scope; dynamic-binding
/// *behavior* for special variables arrives in step L16 -- no such behavior
/// exists yet). @ref is_parameter distinguishes the two ANSI CL forms:
///
///  - `defvar`: initializes @ref name from @ref init only if @ref name is
///    not already bound in the VARIABLE namespace (a bare `(defvar name)`
///    with no init form, @ref has_init false, only marks @ref name special
///    without binding it).
///  - `defparameter`: always requires @ref init and always (re)initializes
///    unconditionally, even if @ref name is already bound.
///
/// @ref name is a plain @c string_view for the same reason as
/// @ref core_setq's @c names (always a list element, never the elaboration
/// root).
///
/// @tparam R        Recursive self-reference.
/// @tparam MaxNodes Arena capacity.
template <typename R, int MaxNodes>
struct core_defvar {
    std::string_view name; ///< VARIABLE-namespace name being declared.
    smdscheme::foundation::arena_box<R, MaxNodes>
        init; ///< The initial-value expression; valid iff @ref has_init.
    bool has_init = false;     ///< False only for a bare `(defvar name)`.
    bool is_parameter = false; ///< True for `defparameter`, false for
                               ///< `defvar`.
};

/// A Common Lisp `block` form: `(block name body...)`.
///
/// Establishes a lexical, one-shot, non-local exit point named @ref name
/// (decision D5, step L14): a `return-from` textually and lexically inside
/// @ref body -- including inside a nested `lambda`, since CL's `block`
/// scope is an ordinary lexical scope, not reset by an intervening `lambda`
/// -- can transfer control directly to the end of this block, skipping any
/// intervening evaluation. Block/tag names are their own namespace
/// (decision D4): a `block` named the same as an in-scope variable or
/// function does not shadow, or get shadowed by, either.
///
/// A zero-form `(block name)` is legal per ANSI CL and evaluates to `nil`;
/// @ref detail::elaborate_list synthesizes a single @ref core_nil body
/// element in that case, so @ref body always holds at least one expression,
/// matching @ref core_lambda's and @ref core_progn's identical invariant.
///
/// `defun` bodies get an implicit `(block function-name ...)` per ANSI CL
/// (@ref detail::elaborate_list's `DEFUN` case) -- @ref core_defun's own
/// docs describe how that wrapping is built.
///
/// The runtime side of `block` -- allocating a fresh, live exit record per
/// dynamic activation, and killing it whether the block falls off the end
/// or is the target of a matching `return-from` -- lives in the evaluators
/// (`eval_direct.hpp`/`cps_code.hpp`), not here: elaboration only resolves
/// *which* block a `return-from` refers to (a lexical question); whether
/// that block's dynamic extent is still active is a runtime one (see
/// @ref core_return_from's docs).
///
/// @tparam R        Recursive self-reference.
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum number of body expressions.
template <typename R, int MaxNodes, int MaxList>
struct core_block {
    std::string_view name; ///< Block-namespace tag (decision D4); always a
                           ///< list element, never the elaboration root,
                           ///< so always arena-backed (see @ref
                           ///< core_setq's docs for the identical
                           ///< reasoning).
    smdscheme::foundation::static_vector<
        smdscheme::foundation::arena_box<R, MaxNodes>, MaxList>
        body; ///< Body expressions, implicit progn (at least one; see
              ///< above).
};

/// A Common Lisp `return-from` form: `(return-from name [value])`.
///
/// Transfers control to the end of the lexically enclosing @ref core_block
/// named @ref name, which @ref detail::elaborate_list has already validated
/// exists (an unresolvable @ref name is an elaboration-time error, decision
/// D5) -- resolving *which* block a given `return-from` targets is a purely
/// lexical (compile-time) question. Whether that block's *dynamic extent*
/// is still active when this node actually evaluates is not: a
/// `return-from` reached only via a closure invoked after its target
/// block's evaluation has already completed (the closure smuggled the
/// `return-from` out of the block's extent) is a diagnosed runtime error,
/// never UB -- see the evaluators' (`eval_direct.hpp`/`cps_code.hpp`)
/// `exit_record::live` check (`env.hpp`).
///
/// A bare `(return-from name)` (no value form) elaborates with an implicit
/// @ref core_nil value, mirroring @ref core_if's implicit-`nil`-alternative
/// synthesis.
///
/// @tparam R        Recursive self-reference.
/// @tparam MaxNodes Arena capacity.
template <typename R, int MaxNodes>
struct core_return_from {
    std::string_view name; ///< Target block-namespace tag; always a list
                           ///< element (see @ref core_block::name).
    smdscheme::foundation::arena_box<R, MaxNodes>
        expr; ///< The value expression (implicit `nil` if omitted).
};

// e5bf324e-a6a5-4d14-817b-69b0c88b114b
/// A Common Lisp `catch` form: `(catch tag-form body...)`.
///
/// Establishes a **dynamic**, one-shot, tag-keyed exit (decision D5, step
/// L15). @ref tag is an ordinary expression, evaluated at run time; the
/// resulting value -- not any name -- is what a `throw` matches against,
/// with `eq`. That is the entire distinction from @ref core_block, and it is
/// why nothing here is checked at elaboration time: unlike
/// @ref core_return_from's block name, a `throw` tag is not knowable
/// statically, so "is there a catch for this tag?" is necessarily a runtime
/// question (see @ref core_throw).
///
/// A zero-form `(catch tag)` is legal per ANSI CL and evaluates to `nil`;
/// @ref detail::elaborate_list synthesizes a single @ref core_nil body
/// element in that case, so @ref body always holds at least one expression,
/// matching @ref core_block's and @ref core_lambda's identical invariant.
///
/// @tparam R        Recursive self-reference.
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum number of body expressions.
template <typename R, int MaxNodes, int MaxList>
struct core_catch {
    smdscheme::foundation::arena_box<R, MaxNodes>
        tag; ///< The tag expression, evaluated once on entry.
    smdscheme::foundation::static_vector<
        smdscheme::foundation::arena_box<R, MaxNodes>, MaxList>
        body; ///< Body expressions, implicit progn (at least one; see
              ///< above).
};
// e5bf324e-a6a5-4d14-817b-69b0c88b114b end

/// A Common Lisp `throw` form: `(throw tag-form result-form)`.
///
/// Evaluates @ref tag, then @ref result, then transfers control to the
/// innermost dynamically active @ref core_catch whose tag is `eq` to the
/// evaluated @ref tag, which becomes that `catch`'s value.
///
/// Both subforms are required, per ANSI CL -- there is no implicit-`nil`
/// result the way @ref core_return_from has an implicit-`nil` value, because
/// ANSI CL's `throw` genuinely requires two arguments while `return-from`'s
/// second is genuinely optional.
///
/// If no matching, still-active `catch` exists when this node evaluates, that
/// is a *diagnosed runtime error* (decision D5: CL's "undefined consequences"
/// become checked errors here), not undefined behavior and not a silent
/// no-op -- see the evaluators' (`eval_direct.hpp`/`cps_code.hpp`)
/// `env_arena::find_catch` check (`env.hpp`).
///
/// @tparam R        Recursive self-reference.
/// @tparam MaxNodes Arena capacity.
template <typename R, int MaxNodes>
struct core_throw {
    smdscheme::foundation::arena_box<R, MaxNodes> tag;    ///< Tag expression.
    smdscheme::foundation::arena_box<R, MaxNodes> result; ///< Value
                                                          ///< expression.
};

// 46e1d48b-cfc0-44dc-a723-1f701ed49bc0
/// A Common Lisp `unwind-protect` form:
/// `(unwind-protect protected-form cleanup...)`.
///
/// Evaluates @ref protected_form, then evaluates every @ref cleanup form in
/// order on **every** way out of that extent -- normal completion, an
/// ordinary error, a `return-from` unwind, and a `throw` unwind alike -- and
/// yields @ref protected_form's value (or re-propagates whatever was in
/// flight) once the cleanups have run.
///
/// @ref cleanup may be empty: `(unwind-protect x)` is a legal, if pointless,
/// ANSI CL form, so unlike @ref core_block's and @ref core_catch's bodies no
/// implicit @ref core_nil is synthesized here -- there is nothing whose value
/// would be observed.
///
/// Innermost-first cleanup ordering is not encoded in this node; it falls out
/// of nesting, because an inner `unwind-protect` regains control (and so runs
/// its cleanups) strictly before the unwind reaches an outer one.
///
/// @tparam R        Recursive self-reference.
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum number of cleanup forms.
template <typename R, int MaxNodes, int MaxList>
struct core_unwind_protect {
    smdscheme::foundation::arena_box<R, MaxNodes>
        protected_form; ///< The form whose extent is protected.
    smdscheme::foundation::static_vector<
        smdscheme::foundation::arena_box<R, MaxNodes>, MaxList>
        cleanup; ///< Cleanup forms, run in order on every exit path;
                 ///< possibly empty (see above).
};
// 46e1d48b-cfc0-44dc-a723-1f701ed49bc0 end

// 98a22b58-964f-4853-be88-20a1d4a9b9d6
/// A Common Lisp `multiple-value-bind` form:
/// `(multiple-value-bind (var...) values-form body...)`.
///
/// Evaluates @ref values_form, then binds the variables named by
/// @ref lambda_node's parameter list to the values it produced -- padding
/// with `nil` when it produced fewer than there are variables, and dropping
/// the surplus when it produced more (ANSI CL) -- and evaluates the body with
/// those bindings in force.
///
/// **There is deliberately no `core_values` node beside this one.** `values`
/// is a *function* in ANSI CL, not a special operator, and once the
/// evaluators' return channel is n-ary (`closure::value_list`, step L20)
/// nothing is gained by pretending otherwise: `values` is an ordinary
/// builtin in the FUNCTION namespace (`closure::builtin_op::values`), so
/// `(values 1 2)` elaborates to a plain @ref core_application, and
/// `#'values`, `(funcall #'values 1 2)` and `(apply #'values '(1 2))` all
/// work with no further machinery.  `multiple-value-bind` cannot be given
/// the same treatment, because it is a *binding* form: its variables are not
/// argument expressions but names, and the values they take are only known
/// once @ref values_form has run.
///
/// **@ref lambda_node is a real @ref core_lambda**, holding the variable
/// names as its parameters and the body forms as its body, and the evaluators
/// bind by *calling* it. This is the same "lower a binding form to a lambda
/// application" strategy @ref detail::elaborate_list already uses for `let`
/// and `let*`, and it is what makes `multiple-value-bind` inherit, for free
/// and without restating any of it, the whole of: implicit progn, arity
/// checking, closure capture, and -- crucially -- the special-variable rule
/// that a bound name marked by `defvar`/`defparameter` binds *dynamically*
/// rather than lexically (`eval_direct.hpp`'s
/// @c detail::bind_lambda_parameters, step L16). The one thing it cannot be
/// lowered to is a bare @ref core_application, because the arguments are not
/// expressions at all: they are the elements of a value list that only
/// exists once @ref values_form has been evaluated, padded and truncated to
/// the parameter count.
///
/// A zero-form body is legal and evaluates to `nil`;
/// @ref detail::elaborate_list synthesizes a single @ref core_nil body element
/// in that case, matching @ref core_block's and @ref core_catch's identical
/// invariant.
///
/// @tparam R        Recursive self-reference.
/// @tparam MaxNodes Arena capacity.
template <typename R, int MaxNodes>
struct core_multiple_value_bind {
    smdscheme::foundation::arena_box<R, MaxNodes>
        values_form; ///< The form whose (possibly multiple) values are bound.
    smdscheme::foundation::arena_box<R, MaxNodes>
        lambda_node; ///< A @ref core_lambda over the bound names; see above.
};
// 98a22b58-964f-4853-be88-20a1d4a9b9d6 end

// 8e268a26-7eaa-470f-86e5-3597d376b700
/// A `tagbody` label, and the target of a `go` (step L23).
///
/// ANSI CL says a `tagbody` statement that is a symbol *or an integer* is a
/// tag rather than a form, and that integer tags are compared with `eql`.
/// Both spellings are supported, so @ref key is a two-alternative variant
/// rather than a bare name -- and the pairing is not an accident: at run time
/// a tag is keyed by an ordinary @c closure::value, whose @c operator== is
/// exactly `eq` for symbols and `eql` for integers (the same comparison
/// @ref core_catch's tag already uses). Nothing extra had to be invented to
/// get ANSI's comparison rule; it fell out of using the value type.
///
/// The symbol alternative is a plain @c std::string_view, not an owned
/// @ref reader::folded_name, for the reason @ref core_setq's names are: a tag
/// is always a list element inside the `tagbody`/`go` form, never the
/// elaboration root, so it is always arena-backed.
struct core_tag {
    std::variant<int, std::string_view> key; ///< Symbol spelling (folded per
                                             ///< D2) or integer value.
    friend constexpr auto operator==(core_tag const &lhs, core_tag const &rhs)
        -> bool = default;
};

/// One entry of a @ref core_tagbody's label table: a tag and the index of the
/// first statement of the basic block it names.
struct core_tag_label {
    core_tag tag;  ///< The label.
    int index = 0; ///< Index into @ref core_tagbody::statements of the block's
                   ///< first statement; equal to the statement count when the
                   ///< tag is the last thing in the body (an empty trailing
                   ///< block, which simply falls off the end).
};

/// A Common Lisp `tagbody` form: `(tagbody {tag | statement}*)`.
///
/// **Tags are separated from statements at elaboration time.** The source
/// body is a single interleaved sequence, but a tag is not a form and is
/// never evaluated, so what the evaluators receive is already split: the
/// forms in order in @ref statements, and a table in @ref labels mapping each
/// tag to the index where its basic block begins. That is the whole of the
/// "lower `tagbody` to basic blocks indexed by tag" lowering, and doing it
/// here means no evaluator has to re-derive it, in any backend.
///
/// `tagbody` always evaluates to `nil` and every statement's value is
/// discarded, so a statement is a single-value context and there is nothing
/// in this node for a value to come out of.
///
/// **Tag resolution is lexical; tag extent is dynamic.** @ref
/// detail::elaborate_list validates that every `go` names a tag of some
/// lexically enclosing `tagbody` (an unknown tag is an elaboration error,
/// decision D5, exactly as `return-from`'s unknown block name is), and a
/// `lambda` does not reset that scope -- so a closure created inside a
/// `tagbody` may contain a `go` to its tags. Whether the `tagbody`'s
/// *activation* is still running when such a `go` fires is a runtime
/// question, answered by the same @c closure::exit_record @c live flag
/// `block`/`return-from` use; see @ref core_go.
///
/// @tparam R        Recursive self-reference.
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum number of statements (and of labels).
template <typename R, int MaxNodes, int MaxList>
struct core_tagbody {
    smdscheme::foundation::static_vector<
        smdscheme::foundation::arena_box<R, MaxNodes>, MaxList>
        statements; ///< The body's forms, in order, tags removed; possibly
                    ///< empty (`(tagbody)` is legal and yields `nil`).
    smdscheme::foundation::static_vector<core_tag_label, MaxList>
        labels; ///< The basic-block table; see @ref core_tag_label.
};

/// A Common Lisp `go` form: `(go tag)`.
///
/// Transfers control to @ref tag's basic block in the lexically enclosing
/// @ref core_tagbody, which @ref detail::elaborate_list has already validated
/// exists. Unlike @ref core_return_from, a `go` does not end its target's
/// extent: the `tagbody` resumes executing statements at the tag and the
/// activation stays live, which is exactly what makes `tagbody` an iteration
/// construct rather than a second spelling of `block`. Each individual
/// transfer is still one-shot in the L14 sense -- the unwind in flight is
/// spent when the owning `tagbody` picks it up.
///
/// A `go` reached only through a closure invoked after its target `tagbody`
/// has finished is a diagnosed runtime error, never UB, precisely as a
/// `return-from` to a dead block is (decision D5); see the evaluators'
/// `live` check.
struct core_go {
    core_tag tag; ///< The target label.
};
// 8e268a26-7eaa-470f-86e5-3597d376b700 end

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
        core_setq<R, MaxNodes, MaxList>, core_defun<R, MaxNodes>,
        core_defvar<R, MaxNodes>, core_block<R, MaxNodes, MaxList>,
        core_return_from<R, MaxNodes>, core_catch<R, MaxNodes, MaxList>,
        core_throw<R, MaxNodes>, core_unwind_protect<R, MaxNodes, MaxList>,
        core_multiple_value_bind<R, MaxNodes>,
        core_tagbody<R, MaxNodes, MaxList>, core_go>;
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
