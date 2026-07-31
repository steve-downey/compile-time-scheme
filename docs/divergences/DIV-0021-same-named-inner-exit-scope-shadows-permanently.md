# DIV-0021: a same-named inner `block`/`tagbody` shadows its outer one for the rest of the enclosing body

- **Status:** open
- **Date:** 2026-07-29
- **Step:** L23 (`tagbody` / `go`)
- **Authority diverged from:** ANSI Common Lisp

## What diverged

In ANSI CL an inner `block` or `tagbody` shadows a same-named outer one only within its own body.
Once the inner form has finished, the outer name is visible again.

Here the shadowing is permanent for the remainder of the enclosing body.

```lisp
(tagbody a (tagbody a) (go a))   ; ANSI: loops forever on the outer tag
                                 ; smdlisp: "go: tagbody has already exited"

(block a (block a nil) (return-from a 1))  ; ANSI: returns 1 from the outer block
                                           ; smdlisp: "return-from: block has
                                           ;           already exited"
```

Both are diagnosed errors, not wrong answers.

## Why

A `block` installs its `exit_record` into the **ambient** environment (`env::define_block`), not into a copy made for its body, and a `tagbody` does the same with its tags (`env::define_tag`).
That is deliberate and load-bearing: a `setq`/`defun`/`defvar` in a sibling statement of the same body sequence has to mutate the one environment object every statement shares, so a `block` in the middle of that sequence cannot be given a private copy.

Nothing removes the binding when the form's extent ends -- there is no scope-exit hook, and the environment's binding lists have no `pop` (they are `smd::smdscheme::foundation::static_vector`, and `smd::smdscheme` is frozen, decision D1).
So the inner form's binding stays in front of the outer form's for as long as the environment lives.

The `block` half of this is pre-existing: it arrived with step L14 and was simply never reachable in a test, because before `tagbody` nothing could re-enter a body sequence after an inner exit form had completed.
L23 makes it easy to reach, so it is recorded now rather than discovered later.

One related decision belongs here because it is *not* an additional divergence.
`env::define_tag` overwrites an existing binding for the same tag rather than pushing a new one, unlike the other three namespaces.
That is observationally identical -- a shadowed binding can never be looked up again, exactly per the paragraph above -- and it is what keeps a `tagbody` nested inside a *looping* `tagbody` from appending one binding per iteration until it exhausts `MaxBindings`.

## Consequences

- Pinned by `EvalDirectTest - SameNamedInnerScopeShadowsForTheRestOfTheBody` (`closure/eval_direct.test.cpp`), which asserts both diagnostics so that closing this divergence has to delete a test rather than quietly change behaviour.
- A `block` inside a `tagbody` loop still grows the ambient environment's block list by one binding per iteration, because `define_block` was left alone (it is L14 code, and changing it is not this step's business).
  A loop that runs more than `MaxBindings` (16) iterations around an inner `block` will trip `static_vector`'s capacity assertion -- loudly, and at compile time for a constant-evaluated program, never silently.
  The same program written with `tagbody` nested in `tagbody` is fine, because of the overwrite rule above.
- Step L24 should describe the environment as append-only-per-activation when it explains `block`/`tagbody` scoping, rather than implying ordinary lexical shadowing.

## Revisit condition

Closed when a form's exit binding is removed at the end of its extent -- e.g. by recording the binding-list length on entry and truncating back to it on every exit path, which is the same "one exit path" shape `unwind-protect` and the dynamic-binding stack already use.
That needs a truncation operation the frozen `static_vector` does not have, so it would arrive as a length field owned by `env` rather than as an edit to `smd::smdscheme`.

## Classification (2026-07-31, rebuild phase R0)

Appended, not edited in place, per the append-only rule for these docs.

**Class: `defect`.**
Diagnosed rather than silent, but ANSI-nonconforming, and this doc records a second consequence that is not merely a conformance gap: a `block` inside a `tagbody` loop grows the ambient environment by one binding per iteration until `MaxBindings` trips.
The truncation operation it says `static_vector` lacks is now an ordinary change under D11.
`EvalDirectTest - SameNamedInnerScopeShadowsForTheRestOfTheBody` pins this and must not be carried forward.
