# DIV-0014: dynamically binding a special variable that has no value is a diagnosed error

- **Status:** open
- **Date:** 2026-07-26
- **Step:** L16 (special variables and dynamic binding)
- **Authority diverged from:** ANSI Common Lisp

## What diverged

`(defvar *x*)` with no initial value proclaims `*x*` special and leaves it unbound, exactly as ANSI CL says.
A subsequent `(let ((*x* 2)) ...)` should then establish a dynamic binding of `*x*` to 2 and make it visible for the extent of the `let`.

This implementation instead reports a diagnosed error, `dynamic binding: special variable is unbound`, and never enters the body.
The same error is reported for any binding form -- `let`, `let*`, a `lambda` or `defun` parameter list -- whose parameter names a special that currently has no value.

A second, narrower case falls out of the same cause: dynamic binding requires an environment with a backing variable `store` (`closure::env`'s mutable mode, `default_env(heap, store)`).
Under a store-less *functional* environment, binding a special reports `setq: environment has no mutable store` rather than establishing a binding.

## Why

`smdlisp` implements dynamic binding by **shallow binding**, which is a save/overwrite/restore over the variable's *one existing value cell*.
That cell is the `store` location the variable's `defvar`/`defparameter` allocated (`closure/value.hpp`'s `store`), and the reason the mechanism works at all is that every environment naming the variable -- including every closure capture made before the binding -- resolves the name to that same location.
Overwriting the cell is therefore instantly visible everywhere, which is precisely the dynamic-scope property; and the innermost dynamic binding *is* the cell, which is why `setq` of a special needs no special case (see `env::set_value`'s docs).

An unbound special has no cell.
Creating one at binding time would not help: a fresh `env::define_value` allocates a location and records it in *this* environment's binding list, and every other environment -- notably the captured environment of a function defined earlier, which is the only party that makes dynamic binding observable at all -- would go on resolving the name through its own copy, find nothing, and report `unbound variable`.
There is no place to put a new cell such that a previously captured environment can find it, because `closure::env` has no parent link and no global value table: a nested scope is a *copy* with bindings appended (see `env`'s class docs), which is the same copy-and-extend design that makes lexical capture correct and recursive `defun` impossible (DIV-0009).

Making it work would mean giving special variables a name-keyed global cell table that `env::lookup_value` consults ahead of its binding list -- a real design change to the environment, and one that mostly buys conformance on a shape (`defvar` with no value, then bind it) that the plan's own merge criteria do not use.
Diagnosing it is the honest small option: it is never silently wrong, and it fails at the binding, not at some later read.

## Consequences

- Every `smdlisp` program that dynamically binds a special must give that special a value first, with `(defvar *x* <init>)` or `(defparameter *x* <init>)`.
  All of the L16 merge-criteria programs do.
- Tested directly, in both backends, as `EvalDirectTest - DynamicBindingOfUnboundSpecialIsDiagnosedError` and `CpsCodeTest - DynamicBindingOfUnboundSpecialIsDiagnosedError`, so the diagnosis is pinned rather than incidental.
- A program using special variables must be run under a mutable environment (`default_env(heap, store)`), not the store-less functional one.
  This is not new -- `setq` has required it since L12 -- but L16 extends the requirement to `let` of a special, which does not look like an assignment in the source.
- Step L21 (sender backend for the CL core) inherits the shallow-binding model as-is: the save/restore records live on `env_arena`'s dynamic stack (`push_dynamic`/`pop_dynamic`), not in `env`, so a backend that does not have one C++ frame per Lisp call can still pair them.
- Anything that later introduces a global value table for symbols -- a real symbol object with a value slot, say -- closes this divergence as a side effect.

## Revisit condition

Closed when special variables get a name-keyed value cell reachable from *any* environment rather than only from environments that happen to carry a binding for the name -- i.e. when a symbol has a value slot of its own.
At that point `let` of an unbound special can allocate the cell on the spot and every previously captured environment will still find it.

## Classification (2026-07-31, rebuild phase R0)

Appended, not edited in place, per the append-only rule for these docs.

**Class: `defect`.**
Diagnosed rather than silent, which makes it a safe defect, but it is still ANSI-nonconforming behaviour rather than a chosen limit.
This doc's own revisit condition — "when a symbol has a value slot of its own" — is precisely D12.
The two `DynamicBindingOfUnboundSpecialIsDiagnosedError` tests must not be carried into `src/smd/cl/`.
