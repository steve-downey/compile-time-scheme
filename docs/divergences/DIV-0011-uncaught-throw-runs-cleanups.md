# DIV-0011: an uncaught `throw` still runs intervening `unwind-protect` cleanups

- **Status:** open
- **Date:** 2026-07-26
- **Step:** L15 (catch / throw / unwind-protect)
- **Authority diverged from:** ANSI Common Lisp

## What diverged

When a `throw` finds no matching, still-active `catch`, this implementation
reports a diagnosed error (`throw: no catch for tag`) that then propagates
outward through the ordinary result channel.
Every `unwind-protect` between the `throw` and the top of the evaluation runs
its cleanup forms as that error passes through.

ANSI CL says the opposite: for a `throw` with no outstanding matching catcher,
"no unwinding occurs" and an error of type `control-error` is signaled at the
point of the `throw`.
In a conforming implementation the stack is still intact when the condition is
signaled, so intervening cleanups have *not* run; they run only if some handler
subsequently transfers control out.

The `throw` itself is correctly ordered: the tag is evaluated, then the result
form, and only then is the catcher searched for (`env_arena::find_catch`), so
the divergence is confined to what happens *after* the search fails.

## Why

There is no condition system, and there is no "signal without unwinding"
channel to signal into.
`smd::smdscheme::foundation::result<value<Core>>` is a frozen two-alternative
type (a value or a `parse_error`; `smd::smdscheme` is frozen for semantic
changes, decision D1), and every diagnosed error in both evaluators is a
`parse_error` returned up the C++ call stack.
Returning *is* unwinding here: there is no way to hold the evaluation in place
at the point of failure and hand control to a handler, because there are no
handlers and no reified stack to hold.

The alternative -- suppressing cleanups specifically for the uncaught-throw
error, by giving it a third marker that `unwind-protect` recognizes and skips
-- would buy conformance on a case that is already a program bug, at the cost
of an `unwind-protect` that sometimes does not protect.
Running cleanups unconditionally is the safer failure mode and keeps
`unwind-protect`'s implementation a single unconditional loop over one result
channel (see `core_unwind_protect`'s handling in `eval_direct.hpp` and
`cps_code.hpp`), which is the property the cleanup-ordering merge criterion
rests on.

## Consequences

- The L15 merge criterion "uncaught `throw` is a diagnosed error" is tested
  only for the *diagnosis* (`EvalDirectTest - UncaughtThrowIsDiagnosedError`
  and its CPS twin, in both the no-catch and wrong-tag shapes), never for the
  absence of unwinding, because the absence of unwinding is not what this
  implementation does.
- A program cannot use an uncaught `throw` to observe a not-yet-unwound state.
  Nothing in the plan does, and nothing can until there is a condition system.
- Step L20 (multiple values) and step L21 (sender backend for the CL core)
  inherit this shape: both must keep whatever error channel they use running
  cleanups on failure, or `unwind-protect` stops being a total guarantee.
- Any later step that adds `signal`/`handler-case`/`handler-bind` must revisit
  this, because that is the step at which "signal without unwinding" first
  becomes expressible.

## Revisit condition

Closed when there is a condition system with handlers that run *before* the
stack unwinds -- i.e. when signaling can be a call rather than a return -- at
which point an uncaught `throw` can signal `control-error` in place and leave
intervening cleanups untouched unless a handler transfers control.
