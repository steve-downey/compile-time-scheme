# Common Lisp limitations

This document is the single consolidated record of every way `smd::smdlisp` diverges from ANSI Common Lisp.
It rolls up the individual `docs/divergences/DIV-*.md` files (step L24's merge criterion: every divergence doc must be referenced from here) plus the decision-D10 "out of scope" list, which the plan asks to record here rather than as one divergence doc per omitted feature.

Organization is by theme, not by DIV number, because a reader debugging a program cares what broke, not when it was decided.
Each entry says what diverges, from what, and links to the DIV file if one exists.
See the DIV file itself for the full "why" and "revisit condition" — this document summarizes, it does not replace them.

Two companion views exist and answer different questions.
[`divergences/README.md`](divergences/README.md) classifies each divergence as `defect`, `scope-decision`, `artifact-of-D1`, `toolchain`, or `process`, which is the engineering-triage question rather than the debugging one — in particular, whether a test may legitimately pin the behaviour.
[`compiler_architecture.org`](compiler_architecture.org) is the design record for why the pipeline has the shape that produces these divergences.
This document is the behavioural baseline that `docs/cl-rebuild-plan.md` must beat: every entry below is either closed by the rebuild or restated there with a decision record justifying it.

## Out of scope entirely (decision D10)

`smdlisp` implements a deliberately small core of ANSI Common Lisp: enough control flow, binding, and macro machinery to demonstrate a compile-time compiler pipeline over three backends (direct evaluator, CPS closures, and Beman senders).
The following facilities are not implemented at all, by design, and have no per-feature divergence doc:

- **Strings.** No string type, no string literals as a distinct value kind.
- **Characters.** No character type or character literals (`#\a`).
- **Floats, ratios, bignums.** The only numeric type is a fixnum-range `int`; no numeric tower.
- **Vectors.** No `vector`/`make-array`/`aref` or any array type.
- **CLOS.** No `defclass`, generic functions, or method dispatch.
- **Conditions and restarts beyond `catch`/`throw`.** No `signal`, `handler-case`, `handler-bind`, `restart-case`, or condition types; `catch`/`throw`/`unwind-protect` (steps L15, L23) are the only nonlocal-exit and cleanup facilities.
- **`format`.** No format-directive printer.
- **`loop`.** No iteration macro; `tagbody`/`go` (step L23, optional per decision D8) is the only iteration primitive.
- **`setf` expanders beyond `setq`.** No generalized-place assignment; `setq` only assigns to already-bound variables.
- **Readtables.** No `*readtable*`, no configurable reader macros, no `set-macro-character`.
- **`eval-when`.** No compile-time/load-time/execute-time staging control; the whole pipeline runs at one stage (C++ constant evaluation, or C++ run time for the sender backend).

None of these has a DIV file because none of them is a divergence from a decision made about how to implement something — they are simply not implemented.
A future step that adds any of them should file a fresh divergence doc only if the addition itself diverges from ANSI CL; the omission itself needs no further recording once this list exists.

## Reader, symbols, and case

- **Single package, uppercase-fold-only reader** ([DIV-0001](divergences/DIV-0001-single-package-and-case.md), accepted-permanent).
  ANSI CL's package system (`defpackage`, `in-package`, `pkg:sym` markers, `COMMON-LISP`/`COMMON-LISP-USER`) and configurable readtable case are both absent.
  `smdlisp` has one implicit package plus `KEYWORD`, and unconditionally folds unescaped symbol names to uppercase with no `|…|`/`\` escape syntax.
  `foo`, `Foo`, and `FOO` are the same symbol; no symbol may contain lowercase letters or package markers.

- **Whole-token atom classification, not greedy-digit scanning** ([DIV-0003](divergences/DIV-0003-atom-maximal-munch.md), accepted-permanent).
  The reader classifies an entire maximal token as an integer or a symbol only after reading the whole thing, rather than greedily consuming leading digits the way the Scheme reader does.
  This is necessary, not merely different: Common Lisp symbols may start with a digit (`1+`), and a greedy-digit reader would misread `1+` as the integer `1` followed by a stray `+`.

## Evaluation model and environment

- **`eq` and `eql` are implemented identically** ([DIV-0002](divergences/DIV-0002-eq-eql-conflated.md), open).
  ANSI CL distinguishes them for numbers and characters; `smdlisp` has neither a numeric tower beyond fixnums nor characters (D10), so the two primitives currently cannot observe a difference and share one implementation.

- **Recursive `defun` is unsupported** ([DIV-0009](divergences/DIV-0009-recursive-defun-unsupported.md), open).
  A function's closure captures a *copy* of the environment before its own name is bound into it, so a self-call inside the body fails with `undefined function`.
  This is independent of `block`/`return-from`; a plain length-of-list recursion fails identically.
  Every worked example in this project and its documentation is therefore non-recursive by necessity, not by style.

- **Dynamically binding an unbound special variable is a diagnosed error** ([DIV-0014](divergences/DIV-0014-dynamic-binding-of-unbound-special-is-an-error.md), open).
  `(defvar *x*)` with no initial value, followed by `(let ((*x* 2)) ...)`, should establish a dynamic binding per ANSI CL; `smdlisp`'s shallow-binding model has no cell to save/restore for an unbound special, so it diagnoses the attempt instead.
  A program must give a special variable a value before ever dynamically rebinding it.
  Binding under a store-less functional environment is also a diagnosed error (`setq`'s pre-existing requirement, extended to `let` of a special).

- **A same-named inner `block`/`tagbody` shadows its outer one permanently** ([DIV-0021](divergences/DIV-0021-same-named-inner-exit-scope-shadows-permanently.md), open).
  ANSI CL scopes the shadowing to the inner form's own body; here the exit binding is appended to the ambient environment and nothing ever removes it, so the outer name stays unreachable for the rest of the enclosing body once an inner form of the same name has run.
  Both cases are diagnosed errors (`go`/`return-from` reports "already exited"), not silently wrong answers.
  The `block` half of this predates `tagbody` (it arrived with L14) but was unreachable until `tagbody` made bodies re-enterable.

## Control flow: nonlocal exit and unwind-protect

- **An uncaught `throw` still runs intervening `unwind-protect` cleanups** ([DIV-0011](divergences/DIV-0011-uncaught-throw-runs-cleanups.md), open).
  ANSI CL signals `control-error` at the `throw` site with the stack intact, so cleanups between the `throw` and the top run only if a handler later transfers control outward.
  `smdlisp` has no condition system and no "signal without unwinding" channel — returning *is* unwinding here — so an uncaught `throw` is reported as an ordinary diagnosed error that then propagates outward, running every intervening cleanup on the way.

- **`return-from` and `throw` carry only the primary value** ([DIV-0019](divergences/DIV-0019-nonlocal-exit-payloads-are-single-valued.md), open).
  Every other route out of a form (falling off the end, an `if` branch, `progn`'s last form, a function's last body form) carries all the values a multiple-value-producing subform returned.
  A nonlocal exit does not: `exit_record`/`catch_record` payloads are single `closure::value`s, not `value_list`s, so `(multiple-value-bind (a b) (block b (return-from b (values 1 2))) ...)` silently binds `b` to `nil` rather than 2.
  This is the sharpest-edged divergence multiple values (L20) leaves behind, because it is a wrong answer with no diagnostic, not a rejected program.
  `go` (step L23) is unaffected: it carries no value at all.

## Macros

- **Host macros use reserved fixed temp names, not `gensym`** ([DIV-0006](divergences/DIV-0006-macro-temp-name-capture.md), open).
  `or`, `cond`, and `case` each bind one fixed, reserved name (`%OR-TEMP`, `%COND-TMP`, `%CASE-TMP`) instead of a freshly generated uncapturable symbol, because `smdlisp` has no `gensym`.
  A user program that lexically binds a variable spelled exactly like one of these three names, inside the relevant macro's expansion scope, can be shadowed by the macro's own binding.
  `defmacro` (L19) supplied the *prerequisite* for `gensym` (a compile-time evaluator that can run macro-body code) but not the builtin itself, so this divergence is still open.

- **Nested backquote has no depth tracking** ([DIV-0008](divergences/DIV-0008-backquote-no-nested-depth-tracking.md), open).
  A `` ` `` template nested inside another `` ` `` template is lowered as opaque literal data, with no attempt to find or lower any `,`/`,@` escapes it contains, unlike ANSI CL's depth-counted nested-unquote rule.
  Single-level backquote (`` `(a ,x ,@ys) ``) works and is tested; nesting does not.

- **`defmacro` is per-top-level-form scoped, with a simplified macro lambda list** ([DIV-0010](divergences/DIV-0010-defmacro-scope-and-lambda-list.md), open).
  Four simplifications from ANSI CL: (1) a macro is visible only for the rest of the top-level form it was defined in, not the whole session; (2) a `defmacro` form is replaced by `(quote name)` rather than installing a global side effect; (3) the macro lambda list supports only required parameters plus one `&rest`/`&body` name — no `&optional`, `&key`, `&aux`, or destructuring; (4) a raw backquote datum in macro-argument position, or a macro expansion producing an improper (dotted) list, is a diagnosed error rather than being structurally reified.

## Multiple values

- **Only `values` and `multiple-value-bind` are implemented, and values are capped at 8** ([DIV-0020](divergences/DIV-0020-multiple-value-operator-set-is-two-forms.md), open).
  ANSI CL's multiple-value family also includes `values-list`, `multiple-value-list`, `multiple-value-call`, `multiple-value-setq`, `multiple-value-prog1`, and `nth-value`; none of these six are bound.
  `closure::default_max_values` is 8, below ANSI's required minimum of 20 (`multiple-values-limit`); `(values 1 ... 9)` is a diagnosed error, not a nine-value return.
  There is deliberately no `core_values` AST node: `values` is an ordinary builtin function (`closure::builtin_op::values`), so `#'values`, `(funcall #'values 1 2)`, and `(apply #'values '(1 2))` all work for free.
  `multiple-value-bind` lowers to a `core_lambda` application, which is how it inherits implicit progn, arity checking, closure capture, and the special-variable dynamic-binding rule without restating any of them.
  The honest one-line summary: multiple values propagate through returns, not through escapes (see DIV-0019 above), and not into the sender backend (see below).

- See also DIV-0019 (nonlocal exit payloads are single-valued) in the control-flow section above, which is the other half of the multiple-values story.

## Sender backend

- **The sender backend has runtime evidence only, no `static_assert` twins** ([DIV-0015](divergences/DIV-0015-sender-backend-has-no-constexpr-twin.md), accepted-permanent).
  Beman Execution's `connect`/`start` (and `sync_wait`) are not constant-evaluable, so a sender graph cannot produce a compile-time value and there is nothing for a `static_assert` to assert.
  The two closure backends (`eval_direct.hpp`, `cps_code.hpp`) both carry a `static_assert` plus a runtime `TEST_CASE` twin for every merge criterion; the sender backend (`sender_program.test.cpp`) carries the runtime half only, for 64 cases transcribed verbatim from the closure backends' test sources plus three native cases that witness completion-channel behavior the closure backends cannot express.
  **Do not describe all three backends as compile-time evaluators** — only the closure backends are.

- **The sender backend does not cover multiple values** ([DIV-0017](divergences/DIV-0017-sender-backend-does-not-cover-l20-multiple-values.md) and [DIV-0018](divergences/DIV-0018-sender-backend-rejects-multiple-values.md), both open).
  A program containing `values` or `multiple-value-bind` compiles under `sender::compile_to_sender` and then completes on the *error* channel (`"values: multiple values are not supported by the sender backend"` / `"multiple-value-bind is not supported by the sender backend"`), rather than working per ANSI CL.
  The cause is structural: the closure backends widened their *return channel* to an n-ary `value_list` (cheap, because every arm already produced a result), while the sender backend's result travels in a *completion signature* (`sender::lisp_completions` names `set_value_t(closure::value<Core>)`), and widening that reaches every hand-written sender and every arm of the evaluation algebra that constructs a value.
  As of step L23 this is the *only* place the three backends disagree on the object language: `tagbody`/`go` (below) needed no such gap and is fully supported by all three.

- **`tagbody`/`go` is fully supported by the sender backend, with no constexpr twin (per DIV-0015 above).**
  Not a divergence in itself, but worth stating plainly because it is easy to assume otherwise given the multiple-values gap just above: a `go` is a nonlocal transfer, `set_stopped` already *is* that channel, and the `live` flag on the tagbody's own `exit_record` answers "aimed at me?" with no new machinery.

## Build and constexpr limits

- **`compile_to_closure` takes the datum arena by caller-owned reference** ([DIV-0007](divergences/DIV-0007-closure-program-datum-arena-caller-owned.md), open).
  Elaborated core nodes hold plain `std::string_view` fields viewing the datum atoms they were read from; letting the datum arena go out of scope while the core arena survives leaves those views dangling.
  Unlike `smd::smdscheme::closure::compile_to_closure` (fully self-contained, one argument), `smd::smdlisp::closure::compile_to_closure` requires the caller to declare and keep alive its own datum arena for as long as the returned program is used.
  `smd::smdlisp::lisp_program` (the public API, step L22) exists specifically to own this arena as a data member, so ordinary users of `compiled_lisp<Source>` never see the requirement directly.

- **Under `-fsanitize=null`, `compiled_lisp` cannot evaluate closure-building programs in a constant expression** ([DIV-0013](divergences/DIV-0013-constexpr-null-compare-through-static-object-under-ubsan.md), open).
  This is a GCC 16 trunk limitation, not an `smdlisp` defect: under the project's default `Asan` config, GCC refuses to fold a null-pointer comparison against the address of a subobject of a namespace-scope `constexpr` object, which is exactly the shape of `compiled_lisp<Source>`'s stored program and `cps_code.hpp`'s closure-guard check.
  A `static_assert` that evaluates a `lambda`/`function`/`defun`-containing program through `compiled_lisp<Source>` therefore fails to compile; the fix is to evaluate through a function-local `lisp_program<Source>{}` instead and let the runtime `TEST_CASE` exercise the public `compiled_lisp` variable.
  Nothing is wrong at run time; `src/examples/godbolt_lisp.cpp` uses `compiled_lisp` with a `defun` program and passes under Asan.

## Known internal duplications (structural, not user-visible)

These do not change any program's observable behavior; they are recorded so a later reader does not mistake deliberate copies for accidents.

- **`source_literal` is copied, not aliased, from `smd::smdscheme`** ([DIV-0012](divergences/DIV-0012-source-literal-copied-not-aliased.md), open).
  Reusing the Scheme original would drag the whole Scheme compiler target (and its `-freflection` requirement) onto every `smdlisp` consumer, and `smd::smdscheme` is frozen for edits (decision D1) so the type cannot be moved to a shared location.
  Two character-for-character-identical but nominally distinct types now exist; changing one does not change the other.

- **Two `mendler_para` implementations exist with the same semantics** ([DIV-0016](divergences/DIV-0016-mendler-para-rederived-for-foundation-fix.md), open).
  `smd::fixpoint::mendler_para` operates on `smd::fixpoint::Fix<F>`; the `smdlisp` core AST is `smd::smdscheme::foundation::fix`, a structurally identical but nominally distinct fixed-point type that the frozen Scheme tree cannot be given a conversion to.
  `smd::smdlisp::sender::detail::mendler_para` (`sender_eval.hpp`) is therefore a local, eight-line re-derivation rather than a call to the shared one.
  Anyone changing the recursion scheme must change both, or explicitly decide not to.

## Tooling and documentation

- **Blog transclusion anchors originally pointed at the authoring worktree, not `main`** ([DIV-0004](divergences/DIV-0004-orgit-transclusion-worktree-path.md), superseded).
  This was a transitional issue in how published blog posts resolved `orgit:` links against a step's own worktree before merge; it is now fully superseded by `docs/epistolary-pinning-plan.md`'s `orgit-file:` tag-pinning scheme, which resolves posts against a `blog/phase-NN` git tag instead of a filesystem path.
  `docs/compiler_architecture.org` (this consolidation's other deliverable) was not unaffected, as an earlier version of this note claimed: it did transclude `smdlisp` code, in "The Common Lisp Layer" section, via the same worktree-resolved plain `file:` links the living document still uses for its current section.
  As of step A1 of the tree-retirement plan (`tmp/plan/`), that section moved to `docs/history/architecture-iterations.org`, pinned against the `iteration/smdlisp-final` tag taken immediately before the `smdlisp` tree left trunk — the living document keeps only the prose for `smd::cl`, the current rebuild, and its worktree-resolved `file:` links (see `scripts/verify-transclusions.sh`) now name only trees that still exist.

## Cross-reference: every divergence doc

Every file under `docs/divergences/DIV-*.md` is referenced above, organized by theme rather than by number: DIV-0001, DIV-0002, DIV-0003, DIV-0004, DIV-0006, DIV-0007, DIV-0008, DIV-0009, DIV-0010, DIV-0011, DIV-0012, DIV-0013, DIV-0014, DIV-0015, DIV-0016, DIV-0017, DIV-0018, DIV-0019, DIV-0020, and DIV-0021.
DIV-0005 does not exist (a gap in the numbering, not a missing reference); the sequence goes directly from DIV-0004 to DIV-0006.
