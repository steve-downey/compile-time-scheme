# DIV-0010: defmacro is per-top-level-form scoped, with a simplified macro lambda list

- **Status:** open
- **Date:** 2026-07-25
- **Step:** L19 (defmacro)
- **Authority diverged from:** ANSI Common Lisp

## What diverged

Step L19 gives `smdlisp` object-language macros: `(defmacro name (lambda-list)
body...)` registers a macro whose expander is Lisp code, compiled by
`elaborator::elaborate` and run by `closure::eval_direct` during the expansion
pass — the compiler running the language it compiles, at compile time. Four
deliberate simplifications diverge from ANSI CL:

1. **Registration scope is one top-level form, not the session.** A
   `macro_context` (`src/smd/smdlisp/macroexpand/expander.hpp`) is constructed
   fresh per top-level `expand_and_elaborate` / context-less `expand_datum`
   call. A `defmacro` is visible for the rest of *that* call's recursive
   expansion, but not to a separate, later top-level form. In ANSI CL a
   `defmacro` at the REPL or in a file is visible to every later form.

2. **A `defmacro` form is replaced by a quoted-symbol datum.** ANSI CL's
   `defmacro` returns the macro name and installs a global macro definition.
   `smdlisp` has no elaborator-level representation for "a compile-time-only
   definition" (unlike `defun`/`defvar`, a `defmacro` leaves nothing for the
   real program's evaluator to run), so `detail::process_defmacro` consumes
   the form entirely and rewrites it to `(quote name)`, which evaluates to the
   macro's name — matching the return value, not the global side effect.

3. **The macro lambda list supports only required parameters plus one
   `&rest`/`&body` name.** `&optional`, `&key`, `&aux`, and nested
   destructuring macro lambda lists are unsupported (`detail::macro_formals` /
   `detail::parse_macro_formals`). `&body` is accepted as a spelling synonym
   for `&rest`; ANSI CL distinguishes them only for pretty-printer
   indentation, which this pipeline has no use for.

4. **Reification is restricted at both ends.** A raw `` ` ``/`,`/`,@` datum in
   macro-argument position (an unexpanded backquote passed *as an argument* to
   a macro call) is a diagnosed error rather than a structural reification
   (`detail::datum_to_value`). A macro expansion that produces an improper
   (dotted) pair chain is a diagnosed error, because the datum layer has no
   dotted-list representation (decision D6, `detail::value_to_datum`).

## Why

The merge criterion for this step is behavioral: a `defmacro`-defined
`my-when` must behave identically to the host `when` macro, plus an
expansion-budget-exceeded diagnostic. Every test this step adds keeps the
`defmacro` and its use sites inside one top-level `progn`, so per-form scope is
sufficient to demonstrate the milestone (the datum⇄value reification pair and a
compiled expander run at compile time) without a session-global macro
environment, which the pipeline — one datum in, one core tree out — has no
structure to hold. `my-when` needs only required parameters and `&body`, so the
full ANSI macro lambda list is out of scope. The reification restrictions match
limits that already exist one layer down: the reader has no dotted-list datum
(DIV, decision D6) and no nested-backquote depth tracking (DIV-0008).

## Consequences

- Two separate top-level programs cannot share a `defmacro`; a macro and its
  callers must live in one form (in practice, one `progn`).
- `&optional`/`&key`/`&aux`/destructuring macro lambda lists are rejected.
- `(defmacro m (x) ...)` used with a backquoted argument, and a macro whose
  expansion is a dotted list, are diagnosed errors, not silent misbehavior.
- The limitations doc (step L24) must list this divergence.

## gensym (DIV-0006) is still open

DIV-0006 named L18/L19 as "when real `gensym` machinery arrives," because
`or`/`cond`/`case` use fixed reserved temp names (`%OR-TEMP` etc.) instead of
uncapturable fresh symbols. L19 supplies the *prerequisite* for `gensym` — a
compile-time evaluator that runs macro-body code — but does **not** add a
`gensym` builtin: `my-when` needs none, and a `gensym` builtin would have to be
added to the value variant, the builtin dispatch, and `default_env`, all in the
`closure/` files this lane treats as read-only (value.hpp / eval_direct.hpp /
env.hpp). Adding it is therefore left to a step that owns those files.
**DIV-0006 remains open.** DIV-0008 (nested backquote) also remains open; a
`defmacro` body may use single-level backquote (tested), but a body that nests
backquote inside another template is still opaque literal data per DIV-0008.

## Revisit condition

Revisit if a later step needs a session-global macro environment (macros shared
across top-level forms), a fuller macro lambda list (`&optional`/`&key`/nested
destructuring), or a dotted-list datum representation. Close the `gensym` half
by reference to DIV-0006 once a step that owns `closure/` adds a `gensym`
builtin the compile-time evaluator can call from a macro body.
