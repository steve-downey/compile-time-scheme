# DIV-0009: recursive `defun` is unsupported (closure captures env before self-binding)

- **Status:** open
- **Date:** 2026-07-25
- **Step:** L14 (block / return-from)
- **Authority diverged from:** ANSI Common Lisp

## What diverged

In this baseline evaluator a function defined with `defun` cannot call itself.

ANSI CL: a `defun`-defined function is globally visible in the function
namespace, so a call to the function's own name inside its body resolves at
call time and recursion works.

Here, both evaluators (`eval_direct.hpp`'s and `cps_code.hpp`'s `core_defun`
handling) evaluate the embedded `lambda` first -- which captures a *copy* of
the current environment (`env_arena::alloc(environment)`, the same
copy-capture an ordinary `(lambda ...)` uses) -- and only *afterward* call
`environment.define_function(name, closure)`.
The captured copy therefore does not contain the function's own name binding.
Any self-call inside the body fails at run time with `undefined function`.
This is independent of `block`/`return-from`: a plain
`(defun len (lst) (if (null lst) 0 (+ 1 (len (cdr lst)))))` fails identically.

## Why

The environment model is copy-and-extend with by-value closure capture
(decision D4; see `env.hpp`'s class docs and `eval_direct.hpp`'s `core_lambda`
case). There is no mutable self-reference cell, no `letrec`-style pre-binding,
and no separate global function table whose cell a closure could resolve
through lazily. Copy capture happens strictly before the name is bound, so the
name is invisible to the body. Making `defun` recursive is a real feature
(a pre-binding / shared function cell), out of scope for L14, whose merge
criteria (plan §9) are `(block b 1 (return-from b 42) 3) => 42`,
`(defun f (x) (if x (return-from f 0) 1))` with `(f t) => 0`, and a diagnosed
dead-exit -- none of which recurse.

## Consequences

- L14's "each activation of a `block` gets its own `exit_record`" property is
  witnessed **non-recursively**: the tests
  `EvalDirectTest - SameNameNestedBlockReturnFromTargetsInnermost` and its CPS
  twin use two simultaneously-live activations of the same block name via
  lexical nesting (`(block b (+ 100 (block b (return-from b 5))))` => `105`)
  instead of a self-recursive function. The predecessor's original recursive
  tests (`RecursiveBlockReturnFromTargetsOwnActivation`) were removed because
  they exercised this unimplemented recursion, not the block machinery.
- Any later step whose merge criterion needs recursion (e.g. a fibonacci /
  list-length demo, or `labels`/`flet` self- and mutual-recursion) must first
  add a self-reference mechanism to `defun` (and/or `flet`/`labels`). Until
  then, iterative or explicitly Y-combinator-style formulations are the only
  options.

## Revisit condition

Closed when `defun` (and any `flet`/`labels`) install a self-visible binding
before the body's environment is captured -- e.g. a mutable/shared function
cell, or pre-binding the name to a placeholder that the just-built closure is
then stored into -- so that a function can resolve its own name at call time.

## Classification (2026-07-31, rebuild phase R0)

Appended, not edited in place, per the append-only rule for these docs.

**Class: `defect`. The headline defect of the pivot implementation.**
A Lisp in which a function cannot call itself is not yet a Lisp, and this doc records that L14's block tests were rewritten to avoid recursion because of it.
Closes under D12 (function slot resolved at call time) and is the named acceptance witness for phase R5: `(defun len (l) (if (null l) 0 (+ 1 (len (cdr l)))))`.

## Resolution (2026-08-04, step R5)

Appended, not edited in place, per the append-only rule for these docs.
The `Status: open` field above is left as written; this section is the current status.

**Closed for `src/smd/cl/**`. Permanently open for the pivot tree.**
R5's evaluator resolves a callee in the interned symbol's function slot at the moment of the call, rather than capturing the environment at definition time, so a body that names itself finds the definition being installed.
The witness this doc named is now a compile-time `static_assert` in `src/smd/cl/eval/machine.test.cpp`, which is stronger than the runtime demonstration the revisit condition asked for: `(defun len (l) (if (null l) 0 (+ 1 (len (cdr l)))))` followed by `(len '(a b c))` constant-evaluates to `3`.
That is D12's function slot doing the job D12 was recorded for.

Nothing here is fixed in `src/smd/smdlisp/**`, and nothing will be.
That tree is the rebuild's behavioural oracle and is never edited (`AGENTS.md` § "Which trees you may edit"), so everything above the classification section goes on describing it accurately.
Where the two trees disagree about recursion the old one is a wrong oracle by design: this is a `defect`, which a test must never pin (D16).
