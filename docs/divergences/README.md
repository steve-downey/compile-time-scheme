# Divergences

One record per deliberate divergence from ANSI Common Lisp, from the active plan, or from a project rule.
Written from `TEMPLATE.md`.
The docs are append-only: a correction is appended with a date, never edited in place (see DIV-0015's appended correction for the established form).

## Classification

Added by phase R0 of `docs/cl-rebuild-plan.md` (2026-07-31).

The classification lives here rather than in each doc's header because the docs are append-only and rewriting twenty headers would be exactly the kind of in-place edit that rule exists to prevent.
Docs carrying a `defect` classification have had a short dated note appended to them; the rest are unchanged.

Five classes:

- **`defect`** — behaviour that is wrong, not a chosen limit.
  Includes anything ANSI-nonconforming that was accepted because the fix was out of a step's scope.
  **A test must never pin a defect.**
  Per D16, a test that asserts defective behaviour is an anti-specification: it makes the fix cost a test deletion, which is backwards.
- **`scope-decision`** — a deliberate limit on what the project implements.
  A test may pin one, because the behaviour is intended.
- **`artifact-of-D1`** — existed only because the `smdscheme` freeze blocked the obvious fix.
  Closes under D11 (`docs/cl-rebuild-plan.md`).
- **`toolchain`** — an external compiler or tool defect, not this project's.
- **`process`** — about workflow or tooling, with no bearing on the object language.

| Doc | Subject | Class | Disposition under the rebuild |
|---|---|---|---|
| DIV-0001 | single package, uppercase fold only | `scope-decision` | permanent (D7); partly reconsidered by D12, since a package is a symbol table |
| DIV-0002 | `eq` and `eql` conflated | `scope-decision` | closes under D12 — `eq` becomes id identity |
| DIV-0003 | whole-token atom classification | `scope-decision` | **correct ANSI behaviour; must not be regressed** in R3 |
| DIV-0004 | orgit transclusion worktree path | `process` | already resolved, then superseded by tag pinning; it is the evidence D1 is obsolete |
| DIV-0006 | reserved temp names instead of `gensym` | `defect` | macro capture is a real bug; closes under D12 |
| DIV-0007 | datum arena is caller-owned | `defect` | dangling-view lifetime design; closes under D12 |
| DIV-0008 | nested backquote has no depth tracking | `defect` | silently wrong: an inner unquote becomes literal data |
| DIV-0009 | recursive `defun` unsupported | `defect` | **the headline defect**; closes under D12, witnessed in R5 |
| DIV-0010 | `defmacro` scope and lambda list | mixed | per-form scope is `defect` (closes under D12); the lambda-list subset and reification limits are `scope-decision` |
| DIV-0011 | uncaught `throw` runs cleanups | `scope-decision` | needs a condition system; diagnosed, never silent |
| DIV-0012 | `source_literal` copied not shared | `artifact-of-D1` | closes under D11 |
| DIV-0013 | constexpr null compare under `-fsanitize=null` | `toolchain` | GCC defect; re-verify against the current trunk in R1 |
| DIV-0014 | binding an unbound special is an error | `defect` | ANSI-nonconforming; closes under D12 |
| DIV-0015 | sender backend has no constexpr twin | `scope-decision` | accepted-permanent and structural; D17 makes it a reason to decouple backends |
| DIV-0016 | `mendler_para` re-derived locally | `artifact-of-D1` | closes under D11; `fold_fix` is deleted in R1 |
| DIV-0017 | sender backend does not cover L20 | `scope-decision` | subsumed by D17 — backends may lag |
| DIV-0018 | sender backend rejects multiple values | `scope-decision` | subsumed by D17; diagnosed, never a wrong answer |
| DIV-0019 | escapes carry one value, not multiple | `defect` | **the only silent wrong answer**; needs D14 |
| DIV-0020 | multiple-value operator set and cap | mixed | the two-form operator subset is `scope-decision`; the cap of 8 is a `defect` — ANSI requires at least 20 |
| DIV-0021 | same-named inner exit scope shadows permanently | `defect` | ANSI-nonconforming, and the environment grows without bound in a loop; needs D13 |

There is no DIV-0005; the number was never issued.

## Tests that pin a defect

These assert defective behaviour and must not be carried into `src/smd/cl/`.
Under D16 they are anti-specifications.
They stay untouched in `src/smd/smdlisp/**`, which is frozen as a behavioural oracle and is never edited.

| Test | Divergence |
|---|---|
| `EvalDirectTest - SameNamedInnerScopeShadowsForTheRestOfTheBody` | DIV-0021 |
| `EvalDirectTest - MoreValuesThanMaxValuesIsDiagnosed` | DIV-0020 (the cap half) |
| `EvalDirectTest - DynamicBindingOfUnboundSpecialIsDiagnosedError` | DIV-0014 |
| `CpsCodeTest - DynamicBindingOfUnboundSpecialIsDiagnosedError` | DIV-0014 |

By contrast, `ReturnFromOfDeadBlockIsDiagnosedError` and `GoToAFinishedTagbodyIsDiagnosed` pin D5 — one-shot, upward-only exits with a diagnosed dead exit — which is the project thesis rather than a limitation.
Those carry over.

## Rules

- Classify every new divergence on creation.
- A `defect` needs a revisit condition that is a fix, not an acceptance.
- If a divergence would be `defect` but for a decision record that makes the behaviour intended, cite the decision record and classify it `scope-decision`.
- Update this table when a doc is added, and when the rebuild closes one.
