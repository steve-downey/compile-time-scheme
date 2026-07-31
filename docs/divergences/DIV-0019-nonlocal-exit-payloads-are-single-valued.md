# DIV-0019: `return-from` and `throw` carry one value, not multiple

- **Status:** open
- **Date:** 2026-07-27
- **Step:** L20 (multiple values)
- **Authority diverged from:** ANSI Common Lisp

## What diverged

In ANSI CL a nonlocal exit transmits multiple values: `(block b (return-from b (values 1 2)))` returns two values, and so does `(catch 'c (throw 'c (values 1 2)))`.

Here both transmit only the **primary** value.
`(multiple-value-bind (a b) (block b (return-from b (values 1 2))) ...)` binds `a` to 1 and `b` to `nil`.

Every other route out of a form does carry all the values: falling off the end of a `block` or `catch` body, an `if` branch, a `progn`'s last form, a function's last body form, an `unwind-protect`'s protected form, and `funcall`/`apply` through to the callee.

## Why

An in-flight unwind does not travel in the evaluators' return channel; it travels in a record.
`closure::exit_record<Core>::payload` and `closure::catch_record<Core>::payload` (`env.hpp`, steps L14/L15) are each a single `closure::value<Core>`, and the unwind itself is signalled by a sentinel error message while the payload waits in the record for whichever frame turns out to own it.

Step L20 widened the *return* channel, which is what every non-escaping form uses.
It did not widen the records, because doing so multiplies their size by `default_max_values` across `default_max_exits` + `default_max_catches` = 256 slots in every `env_arena` -- an arena that is a stack local in every test and in every `constexpr` evaluation of a compiled program.
A `value<Core>` is a variant of eight alternatives; making 256 records eight-wide is a large constant cost paid by every program, to serve a corner of the language no merge criterion for this step names and no test in the project reaches.

The narrower reason is that it would also require threading `MaxValues` into `env_arena`'s template parameter list, which the three backends and every test alias `env_arena<Core, MaxBindings, MaxEnvs>` without.

## Consequences

- `(multiple-value-bind (a b) (block b (return-from b (values 1 2))) ...)` silently binds `b` to `nil` rather than 2.
  There is no diagnostic: this is a wrong answer, not a rejected program, which makes it the sharpest-edged divergence L20 leaves behind.
- The three sites that narrow are all marked in code: the `core_block` and `core_catch` arms of `eval_direct_values` (`eval_direct.hpp`) and of `cps_dispatch` (`cps_code.hpp`), which wrap `rec->payload` in `single_value`, and the `core_return_from`/`core_throw` arms, which store `primary_value(...)` into it.
- Step L23 (`tagbody`/`go`) is unaffected: `go` transmits no value at all.
- Step L24 (documentation consolidation) should state the rule as "multiple values propagate through returns, not through escapes".

## Revisit condition

Closed when `exit_record::payload` and `catch_record::payload` hold a `closure::value_list` and the two escape forms store all the values they were given.
The cheap version of that -- given how small `default_max_values` already is -- is to widen the payload without adding a template parameter, accepting the fixed size increase; the careful version threads `MaxValues` through `env_arena`.

## Classification (2026-07-31, rebuild phase R0)

Appended, not edited in place, per the append-only rule for these docs.

**Class: `defect`. The only silent wrong answer among the twenty.**
This doc says so itself: `(multiple-value-bind (a b) (block b (return-from b (values 1 2))) ...)` binds `b` to `nil` with no diagnostic.
The blocker it names — that widening the payload multiplies 256 records by `default_max_values` — is exactly what D14 removes by taking capacity out of type identity.
