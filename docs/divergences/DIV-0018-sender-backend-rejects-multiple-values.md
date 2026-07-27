# DIV-0018: the sender backend diagnoses `values` and `multiple-value-bind` rather than supporting them

- **Status:** open
- **Date:** 2026-07-27
- **Step:** L20 (multiple values)
- **Authority diverged from:** ANSI Common Lisp

## What diverged

Step L20 gives the two closure backends full ANSI multiple-value semantics: `values`, `multiple-value-bind`, primary-value narrowing, `nil` padding, surplus truncation, and propagation out of every tail position.

The sender backend (`src/smd/smdlisp/sender/**`) gets none of it.
A program containing `values` or `multiple-value-bind` compiles under `sender::compile_to_sender` and then completes on the **error** channel with `"values: multiple values are not supported by the sender backend"` or `"multiple-value-bind is not supported by the sender backend"`.
ANSI CL says both forms work.

This is the same gap DIV-0017 records from the other side: DIV-0017 is L21 saying "L20 had not landed yet"; this is L20 saying "L21 had landed and I did not widen it".

## Why

The two backends carry a form's result in different shapes, and the shapes are not interchangeable.

The closure backends' result travels in the return value, so step L20 widened that return value: `closure::value_list<Core, MaxValues>` is a `static_vector` of values, `eval_direct_values` returns one, and a CPS continuation is invoked with one.
Nothing else had to change, because every arm of both evaluators already had to produce a result and now produces a slightly wider one.

The sender backend's result travels in a *completion signature*.
`sender::lisp_completions` (`defer_outcome.hpp`) names `set_value_t(closure::value<Core>)`, and `outcome<Core>` holds exactly one `closure::value<Core>` on its value channel.
Widening that reaches `outcome`, `outcome_receiver`, `detail::complete_with`, `lisp_completions`, both hand-written senders (`defer_outcome_sender`, `unwind_protect_sender`), and every arm of `eval_algebra` that constructs a value -- and it changes the *type* of the value channel that `unwind_protect` and `when_all` are instantiated over, which is where the interesting design questions are.
That is a step's worth of work with its own decisions, and this step's scope was explicitly the closure backends.

Diagnosing is the honest option, and it is not free-form: silently returning the primary value would make the sender backend disagree with the other two on a program that *looks* like it worked, which is exactly the failure mode a three-backend project cannot afford.
An error the caller can see is strictly better than a wrong answer nobody notices.

## Consequences

- The three backends are no longer at parity on the object language.
  `sender_program.test.cpp` records the divergence as two tests
  (`SenderProgramTest - ValuesIsDiagnosedRatherThanTruncated`, `SenderProgramTest - MultipleValueBindIsDiagnosed`) so a future widening will fail them and have to delete them deliberately.
- DIV-0017 remains open.
  Its revisit condition -- "the sender backend's `outcome` carries the same n-ary payload the closure backends' continuations do" -- is not met by this step, and this document supersedes nothing.
- Step L24 (documentation consolidation) must describe the sender backend's coverage as "everything the closure backends do except multiple values", and should check both DIV-0017 and this document before doing so.
- Nothing else in `sender/**` changed in this step.
  The two new arms are additions to existing exhaustive switches; adding the `core_multiple_value_bind` variant alternative and the `builtin_op::values` enumerator is what forced them, and nothing else in the backend needed adjustment.

## Revisit condition

Closed when `sender::outcome`'s value channel carries a `closure::value_list` (or an equivalent n-ary payload), `lisp_completions` names it, and the L20 test set in `eval_direct.test.cpp` is mirrored into `sender_program.test.cpp` the way the L11-L16 set already is.
That work also closes DIV-0017.
