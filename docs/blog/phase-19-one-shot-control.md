**DRAFT &#x2014; pending author revision**

<div class="abstract" id="orgcce6513">
<p>
Phase 15 argued that <code>call/cc</code> cannot ride on senders, and that Common Lisp's nonlocal exits &#x2014; one-shot, upward-only, dynamic extent &#x2014; are the control operators a sender backend can actually express.
Steps L14 and L15 are the first half of collecting on that argument: <code>block</code> and <code>return-from</code>, then <code>catch</code>, <code>throw</code>, and <code>unwind-protect</code>, in both the direct evaluator and the CPS backend.
The interesting part is not that they work. It is that the lexical/dynamic distinction Common Lisp draws in its semantics came out as two different data structures with two different lifetimes: a <code>block</code>'s exit record is found by name, capturable by a closure, and lives in an append-only slab; a <code>catch</code>'s frame is found by an evaluated tag, capturable by nothing, and lives on a stack whose slots get reused.
<code>unwind-protect</code> then turned out to be one unconditional loop where I had expected a four-way case analysis.
And an uncaught <code>throw</code> here runs cleanups that ANSI CL says should not run, which is filed as DIV-0011 and not papered over.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 18 - setq, defun, progn ←](phase-18-setq-defun-progn.md)

</nav>


# The argument, now with code under it

Phase 15 was the pivot post: `call/cc` stops here, because a sender's operation state completes once, through exactly one of `set_value`, `set_error`, or `set_stopped`, and a multishot continuation is a value you can invoke again after its extent has already returned (Dominiak, Michał and others, 2024) (Kiselyov, Oleg, 2005). That was an argument about contracts, made before any of the code existed. Common Lisp's escapes are all dynamic extent &#x2014; leave early, leave once, do not come back (Steele, Guy L., 1990). Two steps later there is something to point at.

I'm not going to relitigate the pivot; [Phase 15](phase-15-why-common-lisp.md) has it. What this post is about is what the argument cost to implement, and the one place where implementing it taught me something I had not worked out in advance.


# Two exits that look alike and are not

`(block b ... (return-from b 42) ...)` and `(catch 'c ... (throw 'c 42) ...)` read like the same feature with different spelling. Both leave a form early with a value. Both are one-shot. Both are upward-only.

The difference is where the target comes from. A `return-from` names its block, and the elaborator resolves that name &#x2014; an unknown block name is an elaboration error, before anything runs. A `throw` evaluates a tag, and the value it computes is matched against live `catch` frames with `eq`:

```cpp
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
```

Nothing about a `throw` is checkable at elaboration time, because a tag is an arbitrary expression. "Is there a catcher for this?" is a question you can only ask while running.


# Lexical and dynamic, as two data structures

Here is the part I did not see coming, and it is the best thing in the step.

An `exit_record` &#x2014; the `block` side, from L14 &#x2014; is installed in the environment under its name, in a third namespace alongside variables and functions. A `lambda` written inside the block captures that environment by value, and therefore carries the exit record's pointer out with it. That is deliberate: a `lambda` called while its block is still on the stack can `return-from` it, which is what makes a block an escape a helper can invoke rather than just a labelled early return. (The implicit `(block f ...)` that ANSI CL wraps around a `defun` body is here too, though `f` cannot yet call itself &#x2014; a `defun` captures its environment before its own name is bound, which is DIV-0009 and unrelated to any of this.) Capture is also how a closure can smuggle a `return-from` out past the block's own extent, which is why the record carries a `live` flag and a stale one is a diagnosed error instead of a jump into nothing.

A `catch_record` cannot be captured by anything, and the reason is the environment itself:

```cpp
/// One-shot **dynamic** exit bookkeeping for one activation of a `catch`
/// form (decision D5, step L15).
///
/// The dynamic twin of @ref exit_record, and the whole difference between
/// `block`/`return-from` and `catch`/`throw`:
///
///  - an @ref exit_record is found by *name*, through the environment's
///    lexical block namespace (@ref env::lookup_block), so a `return-from`
///    reaches only blocks that lexically enclose it -- and can therefore be
///    smuggled out of its own extent by a closure, which is the case
///    @ref exit_record::live diagnoses;
///  - a @ref catch_record is found by *evaluated tag value*, compared with
///    `eq` (@ref env_arena::find_catch), searching the dynamically active
///    `catch` frames innermost-first. Nothing lexical is consulted, and
///    nothing captures a @ref catch_record: it is reachable only while its
///    `catch` frame is on @ref env_arena's catch stack, which is precisely
///    the form's dynamic extent.
///
/// Because a @ref catch_record cannot be smuggled anywhere, "the exit is
/// dead" never shows up as a stale-pointer question the way it does for
/// `block`; it shows up as @ref env_arena::find_catch simply not finding a
/// matching frame, which is the diagnosed *uncaught throw* error. @ref live
/// still exists, and still means "an unwind has fired against this frame",
/// so that (a) the owning `catch` frame can tell an unwind aimed at *it*
/// from one merely passing through, exactly as @ref exit_record does, and
/// (b) a `throw` raised from an `unwind-protect` cleanup that runs *while*
/// this frame is already unwinding does not re-target the frame that is
/// already on its way out.
///
/// @tparam Core The core AST type (used to type @ref tag and @ref payload).
template <typename Core>
struct catch_record {
    value<Core> tag{};         ///< The evaluated catch tag, compared by
                               ///< `eq` (`value<Core>`'s `operator==`, the
                               ///< same comparison `pairs.hpp`'s `eq`
                               ///< primitive performs).
    exit_id id = invalid_exit; ///< Unique per dynamic activation.
    bool live = false;         ///< False once a `throw` has fired against
                               ///< this frame, or the frame's own
                               ///< evaluation has completed.
    value<Core> payload{};     ///< The thrown value; only meaningful while
                               ///< an unwind targeting this frame is in
                               ///< flight.
};
```

`env` is copied to make a lexical capture. That is the whole mechanism of lexical scoping in this design &#x2014; there is no parent pointer, a nested scope is a copy with more bindings appended. So anything living in `env` gets copied along with every closure built under it, and a dynamically scoped frame must not. Put a `catch` frame in the environment and a closure created inside the `catch` body would still be pointing at that frame long after the form it belongs to has returned.

The frames go on a stack owned by `env_arena` instead, which is where the two lifetimes stop matching:

```cpp
/// Pushes a fresh, live @ref catch_record for one activation of a
/// `catch` form keyed by the already-evaluated @p tag (step L15) and
/// returns a pointer to it, valid until the matching @ref pop_catch.
///
/// **This is a stack, not an append-only arena** -- the opposite of
/// @ref alloc_exit, and deliberately so. An @ref exit_record must
/// survive its `block`'s evaluation because a closure can capture a
/// pointer to it (@ref env::define_block), so records accumulate and
/// the arena's capacity bounds the total number of activations. Nothing
/// captures a @ref catch_record: it is reachable only through this
/// stack, only while its frame is active, so the storage slot is reused
/// once the frame pops and @c MaxCatches bounds only the nesting depth.
///
/// Slots are reused rather than truly popped (the shared
/// @ref smd::smdscheme::foundation::static_vector has no @c pop_back and
/// `smd::smdscheme` is frozen for semantic changes, decision D1), so
/// @ref depth_ -- not @c frames_.size() -- is the authority on how much
/// of the stack is live. Popping a frame therefore never invalidates a
/// pointer to a frame *below* it, which is exactly the guarantee an
/// in-flight `throw` needs: intervening `catch` frames pop themselves as
/// the unwind passes through them, while the target frame's pointer, held
/// since @ref find_catch, stays good.
constexpr auto push_catch(value<Core> tag) -> catch_record<Core> * {
        exit_id const id = next_exit_id_++;
        catch_record<Core> rec{std::move(tag), id, true, value<Core>{}};
        if (depth_ < frames_.size())
            frames_[depth_] = std::move(rec);
        else
            frames_.push_back(std::move(rec));
        return &frames_[depth_++];
}
```

Exit records accumulate: one per `block` activation, never reclaimed, so the arena's capacity bounds how many times a program may enter a `block` at all. Catch frames reuse their slots, so the capacity bounds nesting depth and nothing else &#x2014; a loop that runs a `catch` a million times never gets past depth one. Two operators that ANSI CL describes with nearly the same paragraph, and the semantic difference between them shows up as a slab versus a stack.

The stack is a stack by index, not by `pop_back`: `static_vector` has no `pop_back`, and `smd::smdscheme` is frozen for semantic changes under decision D1, so a separate `depth_` counter is the authority on the live prefix. That accident is load-bearing. Popping a frame does not invalidate the frames below it, which is what an in-flight `throw` needs: intervening `catch` frames pop themselves as the unwind passes through, while the target frame's pointer &#x2014; taken before any of that started &#x2014; stays good.


# Two markers, and why both are named objects

An unwind has to travel somewhere. The plan for L14 said it would travel through an exit table threaded by the CPS dispatcher, with `return-from` invoking the block's continuation directly. That isn't what got built. `result<value<Core>>` is a frozen two-alternative type &#x2014; a value or a `parse_error` &#x2014; and it cannot grow a third alternative, so an unwind travels as an ordinary error whose message is a single shared pointer compared by identity. Every call site in both evaluators already checks `has_value()` before invoking a continuation. Which means every call site already skipped intervening frames for a `return-from` before anyone wrote a line for it.

L15 needed a second marker:

```cpp
/// The shared, static, identity-compared marker used for a `throw` unwind
/// travelling through the same frozen `result<value<Core>>` error channel
/// (step L15).
///
/// Everything @ref block_unwind_marker's docs say applies verbatim, with one
/// difference: a `throw` unwind is aimed at a *dynamic* exit found by tag
/// (@ref catch_record, @ref env_arena::find_catch), not at a lexically
/// resolved block. A distinct marker object -- rather than reusing
/// @ref block_unwind_marker -- is what keeps the two mechanisms from
/// intercepting each other: an enclosing `block` must let a `throw` unwind
/// pass straight through it (its own @ref exit_record is still live and it
/// has no business claiming the value), and an enclosing `catch` must
/// likewise let a `return-from` unwind pass. Both frames decide that by
/// comparing the marker first.
///
/// **This must stay a named `inline constexpr char[]` object**, exactly like
/// @ref block_unwind_marker_storage, and for exactly the reason spelled out
/// there: a `constexpr char const *` initialized from a bare string literal
/// is folded to the literal's address per use site, so with string-literal
/// merging disabled (the Asan build) the pointer-identity comparison holds
/// during constant evaluation and silently fails at run time.
inline constexpr char throw_unwind_marker_storage[] = "smdlisp: throw unwind";
inline constexpr char const *throw_unwind_marker = throw_unwind_marker_storage;
```

Two distinct markers, because each mechanism has to let the other's unwind through untouched. A `block` sitting between a `throw` and its `catch` has no business claiming the value, and a `catch` between a `return-from` and its block has none either. Both frames decide by comparing the marker pointer before they look at anything else, so `(block b (catch 'c (return-from b 3)) 99)` is 3, with the catch stack back at depth zero on the way past.

The "must stay a named `inline constexpr char[]`" warning in those docs is L14's scar tissue. A `constexpr char const *` initialized from a bare string literal gets folded to the literal's address at each use, and with literal merging off &#x2014; the Asan build &#x2014; equal content lands at different addresses. The identity check then holds during constant evaluation and fails at run time, and a `return-from` escapes its own block. Constexpr-green is not run-time-green, which is why the marker has a run-time test of its own now, checking both that the pointer is stable and that it is distinguishable from the block marker.


# unwind-protect is one loop

I expected `unwind-protect` to be the hard one, on the theory that four exit paths &#x2014; a value, an ordinary error, a `return-from`, a `throw` &#x2014; meant four cases to get right.

They are one case:

```cpp
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
```

All four already arrive at the same place as one `result`, because the unwinds were built to ride the error channel and the error channel is where errors already were. Evaluate the protected form, keep whatever came back, run every cleanup in order unconditionally, then propagate what you kept. A cleanup that itself errors or exits non-locally supersedes what was in flight, per ANSI CL, and that is the loop's only `if`.

Innermost-first ordering isn't encoded anywhere. It falls out of nesting: an inner `unwind-protect` regains control before the unwind reaches an outer one, so its cleanups run first by construction.

Under CPS there is one thing to be careful about. The protected form is dispatched with identity continuations rather than the caller's `cont` and `k`, so that this frame gets control back before the value goes anywhere. Wrap only the escape path and the cleanups silently stop running on normal completion &#x2014; an `unwind-protect` that protects three exits out of four. `catch` does the same thing for the same reason: it is a continuation barrier, and it has to be one so its frame pops exactly once on every path out, including the boring one.


# What the tests had to witness

Cleanup ordering, with a decimal log so the order is readable in the answer:

```lisp
(let ((log 0))
  (catch 'c
    (unwind-protect
      (unwind-protect (throw 'c 99) (setq log (+ (* log 10) 1)))
      (setq log (+ (* log 10) 2))))
  log)                                        ; => 12
```

Inner cleanup first, then outer. And the thrown value has to arrive at the `catch` with the cleanups already done, which `+` observes by evaluating left to right:

```lisp
(let ((log 0))
  (+ (catch 'c (unwind-protect (throw 'c 99) (setq log 1))) log))  ; => 100
```

Then the dynamic search, innermost-first, skipping a live frame with the wrong tag:

```lisp
(catch 'a (catch 'b (throw 'a 7)) 99)         ; => 7
(catch 'c 1 (throw 'c 42) 3)                  ; => 42
(catch 'c)                                    ; => nil
(throw 'c 1)                                  ; error: throw: no catch for tag
(catch 'a (throw 'b 1))                       ; error: throw: no catch for tag
```

Every one of those runs twice, once through the direct evaluator and once through the CPS backend, with the same answer required from both. The catch stack is checked back at depth zero afterwards on all three shapes &#x2014; normal completion, caught throw, uncaught throw &#x2014; because a frame left on the stack after its extent ended is a `throw` that finds a catcher which isn't there any more. 614 tests green at the merge.

Each of the L15 merge criteria is asserted twice as well, once inside a `static_assert` and once as an ordinary run-time test case. That is not belt and braces for its own sake. L14 is the reason: the marker bug above was constexpr-green and run-time-broken, and nothing in the constant evaluator was ever going to catch it.


# Where this doesn't match the standard

An uncaught `throw` here runs every intervening `unwind-protect` cleanup on its way out. ANSI CL says no unwinding occurs at all: a `throw` with no outstanding matching catcher signals `control-error` at the point of the `throw`, with the stack intact, and cleanups run only if some handler later transfers control out.

There is no condition system in `smdlisp`, and no channel to signal into that is not also a return. Returning *is* unwinding here. Diagnosing the uncaught `throw` is easy &#x2014; the search fails, and the error says so &#x2014; but by the time that error is a `result` propagating outward, it is indistinguishable to an `unwind-protect` from any other error passing through, and `unwind-protect` runs cleanups on errors.

I could have given the uncaught-throw error a third marker that `unwind-protect` recognizes and skips. That buys conformance on a case which is already a program bug, and pays for it with an `unwind-protect` that sometimes doesn't protect. Running cleanups unconditionally is the safer failure, and it's what keeps the loop above a single unconditional loop. Filed as DIV-0011, closed when there is a condition system &#x2014; that is, when signaling can be a call rather than a return.


# Half of the payoff

`block`, `catch`, `throw`, `unwind-protect`: the escapes are in, one-shot and upward-only, in both evaluators. The half that is still owed is the one the whole argument was about. None of this has met a sender yet. Step L21 is where the CL core gets a sender backend, and where "one-shot and upward-only" stops being a discipline I imposed on myself and starts being one the type system imposes on me.

Step L16 is next, and it wants all of this immediately: a dynamic binding of a special variable has to be saved and restored on every exit path out of its `let`, which is `unwind-protect`'s problem with a different payload. `defvar`'s special mark has been recorded and doing nothing since Phase 18.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 18 - setq, defun, progn](phase-18-setq-defun-progn.md)

</nav>


# References

Dominiak, Michał and others (2024). *P2300: std::execution*, C++ Standards Committee Papers.

Kiselyov, Oleg (2005). *An argument against call/cc*.

Steele, Guy L. (1990). *Common Lisp the Language*, Digital Press.
