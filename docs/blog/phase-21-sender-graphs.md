**DRAFT &mdash; pending author revision**

<div class="abstract" id="org62ea107">
<p>
Phase 15 argued that Common Lisp's control operators are the largest control vocabulary a structured-concurrency backend can express soundly, and Phase 19 built them in two evaluators that had never met a sender.
This is the step where they meet one.
The closure backends push four different things down a single <code>result&lt;value&gt;</code> wire and tell them apart with sentinel error messages compared by pointer identity; a sender already has three completion channels, so the sentinels get deleted instead of ported.
The payoff is <code>unwind-protect</code>, which stops being an evaluator special case and becomes an ordinary sender adapter: a receiver whose three completion functions all funnel into one cleanup call.
The counterweight is that dynamic-binding restore does not move at all, and stays in the C++ control flow around a recursive call.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 20 - defmacro ←](phase-20-defmacro.md)

</nav>


# Four things on one wire

An evaluation in `closure::eval_direct` or `closure::cps_code` returns a `result<value<Core>>`, and that type has two alternatives: a value, or a `parse_error`. Four things have to travel down it. An ordinary value, a diagnosed error, a `return-from` unwind, and a `throw` unwind. Three of the four are the error alternative, so telling them apart needs a discriminator carried out of band, and the one steps L14 and L15 used is a sentinel `parse_error` message compared by **pointer identity**.

The pointer identity is the part that bites. `block_unwind_marker` and `throw_unwind_marker` have to be named `inline constexpr char[]` objects, because a `constexpr char const *` initialized from a bare string literal gets folded to the literal's address at each use, and two equal string literals are not required to share an address. [Phase 19](phase-19-one-shot-control.md) has the whole story of that bug, which was green under constant evaluation and broken at run time. The mechanism works. It's still a wart, and it's a wart of exactly one kind: it exists because a two-alternative carrier is being asked to carry four things.


# Deleting the sentinels

A sender doesn't have that problem, because it was born with three completion channels (Dominiak, Michał and others, 2024). So decision D5's mapping writes itself, and the interesting thing is what the mapping lets you throw away:

```cpp
/// Which of a sender's three completion channels fired.
///
/// **This enum is the whole difference between the sender backend and the two
/// closure backends**, and it is worth being precise about what changed.
///
/// `closure::eval_direct` and `closure::cps_code` both return a single
/// @ref smd::smdscheme::foundation::result, so *four* different things --  an
/// ordinary value, a diagnosed error, a `return-from` unwind and a `throw`
/// unwind -- have to travel down one wire.  Telling them apart needs an
/// out-of-band discriminator, which is exactly what `closure/env.hpp`'s
/// @c block_unwind_marker and @c throw_unwind_marker are: sentinel
/// `parse_error` messages compared by *pointer identity*, which in turn is
/// why they have to be named `inline constexpr char[]` objects rather than
/// bare string literals (see that header's docs, and step L14's bug).
///
/// A sender has three completion channels natively, so the sentinels are not
/// ported here -- they are **deleted**.  The mapping (decision D5) is:
///
///  - an ordinary value            -> @c set_value(value<Core>)
///  - a diagnosed error            -> @c set_error(parse_error)
///  - a `return-from`/`throw` unwind in flight -> @c set_stopped()
///
/// Nothing is multiplexed, so nothing needs discriminating: an unwind cannot
/// be mistaken for an error because it does not arrive on the error channel.
/// What the target `block`/`catch` still needs to know -- "is this stop aimed
/// at *me*?" -- is answered exactly as the closure backends already answer it,
/// by testing @c live on its own @ref closure::exit_record /
/// @ref closure::catch_record.  The unwind's payload likewise stays where it
/// already lives, in that record; @c set_stopped() carries no value and does
/// not need to.
///
/// The two cases D5 calls out as *not* unwinds stay on the error channel and
/// are diagnosed, never UB: an uncaught `throw` (no live frame matches the
/// tag) and a `return-from` naming a block whose extent has ended.
enum class channel {
    value,  ///< @c set_value fired; @ref outcome::val is meaningful.
    error,  ///< @c set_error fired; @ref outcome::err is meaningful.
    stopped ///< @c set_stopped fired; the in-flight unwind's payload lives
            ///< in the targeted exit/catch record, not here.
};

/// The observed completion of one evaluation, as recorded by
/// @ref outcome_receiver.
///
/// This is the sender backend's carrier type -- the analogue of
/// @c result<value<Core>> in the closure backends, widened by exactly one
/// channel.  It is a plain aggregate rather than a variant so that the
/// stopped case can be represented without inventing a payload for it.
///
/// @tparam Core The core AST type.
template <typename Core>
struct outcome {
    channel kind{channel::value};                  ///< Which channel fired.
    closure::value<Core> val{};                    ///< Valid iff @c kind is
                                                   ///< @c channel::value.
    smd::smdscheme::foundation::parse_error err{}; ///< Valid iff @c kind is
                                                   ///< @c channel::error.

    /// True iff this completion arrived on the value channel.
    [[nodiscard]] constexpr auto is_value() const -> bool {
        return kind == channel::value;
    }
    /// True iff this completion arrived on the error channel.
    [[nodiscard]] constexpr auto is_error() const -> bool {
        return kind == channel::error;
    }
    /// True iff this completion arrived on the stopped channel, i.e. a
    /// nonlocal exit is in flight.
    [[nodiscard]] constexpr auto is_stopped() const -> bool {
        return kind == channel::stopped;
    }
};

/// Builds a value-channel @ref outcome carrying @p v.
template <typename Core>
[[nodiscard]] constexpr auto make_value(closure::value<Core> v)
    -> outcome<Core> {
    return outcome<Core>{channel::value, std::move(v), {}};
}

/// Builds an error-channel @ref outcome carrying @p e.
template <typename Core>
[[nodiscard]] constexpr auto
make_error(smd::smdscheme::foundation::parse_error e) -> outcome<Core> {
    return outcome<Core>{channel::error, {}, e};
}

/// Builds a stopped-channel @ref outcome: a nonlocal exit is in flight, and
/// its payload is in the exit/catch record it targets.
template <typename Core>
[[nodiscard]] constexpr auto make_stopped() -> outcome<Core> {
    return outcome<Core>{channel::stopped, {}, {}};
}

/// Lifts a closure-backend @c result into an @ref outcome, mapping a failed
/// result onto the error channel.
///
/// Used at the boundaries where the sender backend reuses code the closure
/// backends already own and that predates the third channel -- @ref
/// closure::apply_prim, @ref closure::env::lookup_value, @ref
/// closure::detail::bind_lambda_parameters.  None of those can produce an
/// unwind, so the two-channel-to-three-channel widening is total and lossless
/// here.
template <typename Core>
[[nodiscard]] constexpr auto
from_result(smd::smdscheme::foundation::result<closure::value<Core>> const &r)
    -> outcome<Core> {
    if (r.has_value())
        return make_value<Core>(r.value());
    return make_error<Core>(r.error());
}

/// Projects an @ref outcome back onto a closure-backend @c result, for
/// callers (tests, @ref sender_program) that want the two backends to be
/// compared with one @c REQUIRE.
///
/// A stopped completion that reaches the top of a program is an unwind with
/// no target left to catch it. The closure backends cannot produce that state
/// at top level either -- `block`/`catch` resolve their own unwinds, and an
/// unwind naming nothing live is already diagnosed at the `return-from`/
/// `throw` site -- so it is reported as a diagnosed error rather than being
/// silently flattened into a value.
template <typename Core>
[[nodiscard]] constexpr auto to_result(outcome<Core> const &o)
    -> smd::smdscheme::foundation::result<closure::value<Core>> {
    using Res = smd::smdscheme::foundation::result<closure::value<Core>>;
    switch (o.kind) {
    case channel::value:
        return Res{o.val};
    case channel::error:
        return Res{o.err};
    case channel::stopped:
        break;
    }
    return Res{smd::smdscheme::foundation::parse_error{
        {}, "nonlocal exit escaped the top level"}};
}
```

An ordinary value goes to `set_value`, a diagnosed error to `set_error`, an unwind in flight to `set_stopped`. Nothing is multiplexed, so nothing needs discriminating, and the two markers have no work left to do. They aren't ported into this backend. They're deleted.

What a `block` still has to answer is a different question, and one the sentinels were never answering anyway: is this unwind aimed at *me*? That was always decided by the `live` flag on the block's own exit record, and it still is. Here is the `block` arm with the comparison gone:

```cpp
[&](elaborator::core_block<Core, MaxNodes, MaxList> const &cb)
    -> Out {
    // One fresh, live exit record per dynamic activation,
    // installed into the *ambient* environment so a sibling
    // setq/defun elsewhere in this body still mutates the one
    // object every statement shares.
    auto *rec = ctx.envs->alloc_exit(closure::symbol{cb.name});
    ctx.environment->define_block(closure::symbol{cb.name},
                                  rec);

    Out last =
        make_error<Core>(parse_error{{}, "block: empty body"});
    for (int i = 0; i < cb.body.size(); ++i) {
        last = child(cb.body[i]);
        if (last.is_value())
            continue;
        // Note what is NOT here: the closure backends compare
        // `last.error().message` against
        // `closure::block_unwind_marker` before testing the
        // record, because in those backends an unwind and an
        // ordinary error arrive on the same channel and are
        // otherwise indistinguishable.  Here they cannot be
        // confused -- an error arrives on the error channel
        // and an unwind on the stopped channel -- so the
        // sentinel comparison is deleted, and `live` alone
        // answers the only remaining question: is this unwind
        // aimed at *this* activation?  A `throw` passing
        // through never touches an exit record, so it leaves
        // `live` set and propagates untouched.
        if (last.is_stopped() && !rec->live)
            return make_value<Core>(rec->payload);
        // Either an ordinary error, or an unwind aimed at an
        // enclosing scope: this extent is ending either way.
        rec->live = false;
        return last;
    }
    // Falling off the end also ends the extent (D5).
    rec->live = false;
    return last;
},
[&](elaborator::core_return_from<Core, MaxNodes> const &rf)
    -> Out {
    Out val_o = child(rf.expr);
    if (!val_o.is_value())
        return val_o;
    auto rec_r =
        ctx.environment->lookup_block(closure::symbol{rf.name});
    if (!rec_r.has_value())
        return make_error<Core>(rec_r.error());
    auto *rec = rec_r.value();
    if (!rec->live)
        // Using a dead exit is a diagnosed error, never UB
        // (D5) -- and it stays on the *error* channel, since
        // it is not an unwind at all.
        return make_error<Core>(parse_error{
            {}, "return-from: block has already exited"});
    rec->payload = val_o.val;
    rec->live = false;
    // The escape itself: complete the enclosing scope's
    // sender early, with no value.
    return make_stopped<Core>();
},
[&](elaborator::core_catch<Core, MaxNodes, MaxList> const &cc)
    -> Out {
    // The dynamic counterpart of core_block: the tag is an
    // ordinary expression evaluated once on entry, and the
    // frame lives on the env_arena's catch stack rather than
    // in the environment, because `catch` is found by
    // evaluated tag value at run time.
    Out tag_o = child(cc.tag);
    if (!tag_o.is_value())
        return tag_o;

    auto *rec = ctx.envs->push_catch(tag_o.val);
    Out last =
        make_error<Core>(parse_error{{}, "catch: empty body"});
    for (int i = 0; i < cc.body.size(); ++i) {
        last = child(cc.body[i]);
        if (last.is_value())
            continue;
        if (last.is_stopped() && !rec->live) {
            Out const caught = make_value<Core>(rec->payload);
            ctx.envs->pop_catch();
            return caught;
        }
        rec->live = false;
        ctx.envs->pop_catch();
        return last;
    }
    rec->live = false;
    ctx.envs->pop_catch();
    return last;
},
[&](elaborator::core_throw<Core, MaxNodes> const &ct) -> Out {
    Out tag_o = child(ct.tag);
    if (!tag_o.is_value())
        return tag_o;
    Out res_o = child(ct.result);
    if (!res_o.is_value())
        return res_o;
    auto *rec = ctx.envs->find_catch(tag_o.val);
    if (rec == nullptr)
        // D5: an uncaught throw surfaces on the ERROR
        // channel, not the stopped one. It is not an unwind
        // looking for a target; it is the diagnosis that no
        // target exists.
        return make_error<Core>(
            parse_error{{}, "throw: no catch for tag"});
    rec->payload = res_o.val;
    rec->live = false;
    return make_stopped<Core>();
},
```

Two of those arms are the reason I like this mapping. A `return-from` naming a block whose extent has already ended returns an *error*, and an uncaught `throw` returns an error too. Neither of them is an unwind. An unwind is a transfer of control to somewhere; a `throw` with no live catcher is the diagnosis that there is nowhere to transfer to, which is a different fact about the program and deserves a different channel. D5 says a dead exit is a diagnosed error and never UB (Steele, Guy L., 1990), and on this backend that sentence is a channel choice instead of a convention you have to remember.


# One leaf sender, and why the stock ones won't do

Everything above returns an `outcome`, which is data. Getting data to become a completion needs a sender, and none of the stock factories can do it: `just` always completes with a value, `just_error` always with an error, `just_stopped` always stopped, and which of the three a `smdlisp` form completes on isn't known until the form has run. `let_value` can't select between them either, because all of its branches have to have one type.

So there's one hand-written leaf:

```cpp
/// The completion signature set shared by every sender in this backend.
///
/// Fixing one set for the whole backend, rather than computing it per
/// adapter, is what keeps the hand-written senders small: @ref
/// unwind_protect_sender can forward its child's completions verbatim
/// because the child's completions are always exactly these three.  It is
/// also the type-level statement of decision D5 -- a `smdlisp` computation
/// either produces a value, is diagnosed, or is unwinding -- so it is worth
/// having as a name rather than as a repeated triple.
///
/// @tparam Core The core AST type.
template <typename Core>
using lisp_completions = sender_v::completion_signatures<
    sender_v::set_value_t(closure::value<Core>),
    sender_v::set_error_t(smd::smdscheme::foundation::parse_error),
    sender_v::set_stopped_t()>;

/// The operation state of a @ref defer_outcome_sender: holds the thunk and
/// the receiver, and does the channel dispatch when started.
///
/// @tparam Core     The core AST type.
/// @tparam F        A nullary invocable returning @c outcome<Core>.
/// @tparam Receiver The connected receiver.
template <typename Core, typename F, typename Receiver>
struct defer_outcome_state {
    using operation_state_concept = sender_v::operation_state_tag;

    F fn;              ///< The deferred evaluation.
    Receiver receiver; ///< Whom to complete.

    /// Runs @ref fn and routes its @ref outcome to the matching completion
    /// function.
    ///
    /// This three-way `switch` is the *only* place in the backend where an
    /// @ref outcome becomes a real completion, which is what makes the
    /// mapping in @ref channel's docs a single auditable statement rather
    /// than a convention spread over twenty visitor arms.
    auto start() & noexcept -> void {
        outcome<Core> o = fn();
        switch (o.kind) {
        case channel::value:
            sender_v::set_value(std::move(receiver), std::move(o.val));
            return;
        case channel::error:
            sender_v::set_error(std::move(receiver), o.err);
            return;
        case channel::stopped:
            sender_v::set_stopped(std::move(receiver));
            return;
        }
    }
};

/// A lazy sender whose completion channel is decided at @c start time by
/// running a thunk.
///
/// This is the backend's one leaf sender, and it exists because none of the
/// stock factories can do the job: @c just always completes with a value,
/// @c just_error always with an error, @c just_stopped always stopped, and
/// which of the three a `smdlisp` form completes on is not known until the
/// form has been evaluated.  @c let_value cannot select between them either,
/// since all of its branches must have one type.
///
/// Being lazy matters, and is not incidental: nothing in @p fn runs until the
/// operation state is started, so wrapping this sender in @ref
/// unwind_protect_sender genuinely places the protected form's evaluation
/// *inside* the adapter's operation, which is what makes cleanup-on-every-
/// channel a property of the sender graph rather than of the C++ control flow
/// around it.
///
/// @tparam Core The core AST type.
/// @tparam F    A nullary invocable returning @c outcome<Core>.
template <typename Core, typename F>
struct defer_outcome_sender {
    using sender_concept = sender_v::sender_tag;

    F fn; ///< The deferred evaluation.

    /// Reports the fixed backend-wide completion set (@ref lisp_completions).
    template <typename Self, typename... Env>
    static consteval auto get_completion_signatures() {
        return lisp_completions<Core>{};
    }

    /// Connects to @p r, moving the thunk into the operation state.
    template <typename Receiver>
    auto connect(Receiver r) && -> defer_outcome_state<Core, F, Receiver> {
        return defer_outcome_state<Core, F, Receiver>{std::move(fn),
                                                      std::move(r)};
    }
};

/// Builds a @ref defer_outcome_sender from @p fn.
///
/// @tparam Core The core AST type.
/// @param  fn   A nullary invocable returning @c outcome<Core>.
template <typename Core, typename F>
[[nodiscard]] auto defer_outcome(F fn)
    -> defer_outcome_sender<Core, std::decay_t<F>> {
    return defer_outcome_sender<Core, std::decay_t<F>>{std::move(fn)};
}
```

The `switch` in `start` is the only place in the backend where an `outcome` turns into a real completion, which is what keeps the channel mapping one auditable statement instead of a convention spread over twenty visitor arms. And being lazy is load-bearing. Nothing in the thunk runs until the operation state is started, so wrapping this in an adapter really does put the protected form's evaluation *inside* the adapter's operation.


# unwind-protect is an adapter now

This is the part of the step I actually wanted to see, and it's the part where the sender model earns something the closure model gets for free.

Under direct or CPS evaluation, `unwind-protect` runs its cleanups on every exit path because there is only one exit path. The four outcomes have already been merged into one `result` by the time `core_unwind_protect` sees them, so a single unconditional loop covers all of them, and Phase 19 was pleased about it. A sender backend can't inherit that from C++ control flow. Three channels really are three functions, called from three different places inside the child operation, and there is no arrangement of the surrounding code that makes them one. The property has to be rebuilt, and the natural place to rebuild it is a receiver:

```cpp
/// The receiver that makes `unwind-protect` an *adapter* rather than a
/// special case in the evaluator.
///
/// Decision D5 says cleanup runs on **all three** completion channels, and
/// this is that sentence as code: @c set_value, @c set_error and
/// @c set_stopped are three separate functions that immediately funnel into
/// one @ref finish.
///
/// It is worth being clear about what this buys, because the closure
/// backends already get the same guarantee by a different route.  In
/// `closure/eval_direct.hpp` and `closure/cps_code.hpp` a value, an ordinary
/// error, a `return-from` unwind and a `throw` unwind are *already* merged
/// into one @c result before `core_unwind_protect` sees them, so "run the
/// cleanups on every path" is a single unconditional loop -- they achieve
/// restore-on-every-path by arranging that there is only one path.  A sender
/// backend cannot inherit that from C++ control flow: the three channels are
/// genuinely three functions, called from three places inside the child
/// operation.  So the property has to be *reconstructed*, and reconstructing
/// it is the whole substance of porting `unwind-protect` to senders.  Having
/// each channel do nothing but call @ref finish is how the reconstruction
/// stays checkable by eye.
///
/// ANSI CL's precedence rule is preserved: a cleanup that itself completes
/// abnormally supersedes whatever was in flight; a cleanup that completes
/// normally has its value discarded and the original completion continues.
///
/// @tparam Core     The core AST type.
/// @tparam Cleanup  A nullary invocable returning @c outcome<Core>, which
///                   runs the cleanup forms.
/// @tparam Receiver The downstream receiver.
template <typename Core, typename Cleanup, typename Receiver>
struct unwind_protect_receiver {
    using receiver_concept = sender_v::receiver_tag;

    Cleanup cleanup;   ///< Runs the cleanup forms.
    Receiver receiver; ///< Downstream.

    /// The protected form produced a value.
    auto set_value(closure::value<Core> v) && noexcept -> void {
        std::move(*this).finish(make_value<Core>(std::move(v)));
    }
    /// The protected form was diagnosed.
    auto set_error(smd::smdscheme::foundation::parse_error e) && noexcept
        -> void {
        std::move(*this).finish(make_error<Core>(e));
    }
    /// The protected form is unwinding (`return-from` or `throw`).
    auto set_stopped() && noexcept -> void {
        std::move(*this).finish(make_stopped<Core>());
    }

    /// Runs the cleanup, then completes downstream with whichever of the two
    /// outcomes ANSI CL says wins.
    ///
    /// @param in_flight The protected form's completion, as data.
    auto finish(outcome<Core> in_flight) && noexcept -> void {
        outcome<Core> const cleaned = cleanup();
        detail::complete_with<Core>(std::move(receiver),
                                    cleaned.is_value() ? std::move(in_flight)
                                                       : cleaned);
    }
};

/// The operation state of an @ref unwind_protect_sender.
///
/// Holds the child operation by value and is immovable, so that the child's
/// operation state -- which holds a pointer to the @ref
/// unwind_protect_receiver stored inside it -- is never relocated after
/// @c connect.
///
/// @tparam Core     The core AST type.
/// @tparam Child    The protected form's sender type.
/// @tparam Cleanup  A nullary invocable returning @c outcome<Core>.
/// @tparam Receiver The downstream receiver.
template <typename Core, typename Child, typename Cleanup, typename Receiver>
struct unwind_protect_state {
    using operation_state_concept = sender_v::operation_state_tag;
    using inner_receiver_t = unwind_protect_receiver<Core, Cleanup, Receiver>;

    /// Connects the protected form to the cleanup-running receiver.
    unwind_protect_state(Child child, Cleanup cleanup, Receiver receiver)
        : inner_(sender_v::connect(
              std::move(child),
              inner_receiver_t{std::move(cleanup), std::move(receiver)})) {}

    unwind_protect_state(unwind_protect_state const &) = delete;
    auto operator=(unwind_protect_state const &)
        -> unwind_protect_state & = delete;

    /// Starts the protected form.  Everything else happens on the way out.
    auto start() & noexcept -> void { sender_v::start(inner_); }

  private:
    sender_v::connect_result_t<Child, inner_receiver_t> inner_;
};

/// A sender that runs @p cleanup when its child completes, on **every**
/// completion channel.
///
/// @tparam Core    The core AST type.
/// @tparam Child   The protected form's sender type.
/// @tparam Cleanup A nullary invocable returning @c outcome<Core>.
template <typename Core, typename Child, typename Cleanup>
struct unwind_protect_sender {
    using sender_concept = sender_v::sender_tag;

    Child child;     ///< The protected form.
    Cleanup cleanup; ///< The cleanup forms.

    /// Reports the fixed backend-wide completion set (@ref lisp_completions).
    ///
    /// The child always has exactly these three completions too, so the
    /// adapter forwards rather than transforms: a cleanup can change *which*
    /// channel fires, never which channels are possible.
    template <typename Self, typename... Env>
    static consteval auto get_completion_signatures() {
        return lisp_completions<Core>{};
    }

    /// Connects downstream, wrapping @p r in an @ref unwind_protect_receiver.
    template <typename Receiver>
    auto connect(
        Receiver r) && -> unwind_protect_state<Core, Child, Cleanup, Receiver> {
        return unwind_protect_state<Core, Child, Cleanup, Receiver>(
            std::move(child), std::move(cleanup), std::move(r));
    }
};

/// Builds an @ref unwind_protect_sender protecting @p child with @p cleanup.
///
/// @tparam Core    The core AST type.
/// @param  child   The protected form's sender.
/// @param  cleanup A nullary invocable returning @c outcome<Core>.
template <typename Core, typename Child, typename Cleanup>
[[nodiscard]] auto unwind_protect(Child child, Cleanup cleanup)
    -> unwind_protect_sender<Core, std::decay_t<Child>, std::decay_t<Cleanup>> {
    return unwind_protect_sender<Core, std::decay_t<Child>,
                                 std::decay_t<Cleanup>>{std::move(child),
                                                        std::move(cleanup)};
}
```

`set_value`, `set_error` and `set_stopped` each do one thing, which is call `finish`. That's the whole reconstruction, and it's checkable by looking at it. `finish` then applies ANSI CL's precedence rule: a cleanup that completes abnormally supersedes whatever was in flight, and a cleanup that completes normally has its value discarded.

The evaluator arm for `unwind-protect` is now the one arm that doesn't evaluate anything:

```cpp
[&](elaborator::core_unwind_protect<Core, MaxNodes,
                                    MaxList> const &up) -> Out {
    // The one form in the backend that is not evaluated by
    // this algebra at all: it is *delegated to a sender
    // adapter*.  The protected form becomes a lazy child
    // sender, the cleanup forms become the adapter's cleanup
    // thunk, and which of the three channels the child
    // completes on -- and therefore what `unwind-protect`
    // must do about it -- is decided inside
    // `unwind_protect_receiver`, not here.  That is decision
    // D5's "cleanup on all three completion channels" made
    // structural: this arm cannot forget a channel, because
    // it does not enumerate them.
    return run_sender<Core>(unwind_protect<Core>(
        defer_outcome<Core>(
            [&] { return child(up.protected_form); }),
        [&]() -> Out {
            for (int i = 0; i < up.cleanup.size(); ++i) {
                Out cl_o = child(up.cleanup[i]);
                // ANSI CL: a cleanup that itself exits
                // non-locally supersedes what was in flight.
                if (!cl_o.is_value())
                    return cl_o;
            }
            // Cleanup values are discarded; returning a value
            // outcome is how this thunk says "nothing to
            // report", and the adapter then forwards the
            // protected form's own completion unchanged.
            return make_value<Core>(Val{closure::nil_t{}});
        }));
}
```

It builds a lazy child sender, hands over a cleanup thunk, and stops. Which of the three channels the child completes on, and what `unwind-protect` owes that channel, is decided inside the receiver. So this arm can't forget a channel, because it never enumerates them. The closure backends' guarantee is a loop somebody has to keep unconditional; this one is a shape.


# The driver is connect and start

Every sender here completes synchronously and inline, so running one is a stack-allocated operation state and a recording receiver:

```cpp
/// A receiver that records which completion channel fired, instead of doing
/// anything with it.
///
/// This is the sender backend's *only* driver, and it is deliberately not
/// @c sync_wait.  @c sync_wait collapses the three channels back into two:
/// it returns an engaged @c optional for a value, a disengaged one for
/// stopped, and it **throws** on @c set_error.  A backend whose entire thesis
/// is that the three channels are distinct cannot observe its own
/// completions through an adapter that merges two of them into an exception,
/// and this project does not use exceptions for diagnosed errors anywhere
/// else either.  @c connect + @c start with a recording receiver is both
/// simpler and the honest primitive.
///
/// @tparam Core The core AST type.
template <typename Core>
struct outcome_receiver {
    using receiver_concept = sender_v::receiver_tag;

    outcome<Core> *out; ///< Where the observed completion is written.

    /// Records a value completion.
    auto set_value(closure::value<Core> v) && noexcept -> void {
        *out = make_value<Core>(std::move(v));
    }
    /// Records an error completion.
    auto set_error(smd::smdscheme::foundation::parse_error e) && noexcept
        -> void {
        *out = make_error<Core>(e);
    }
    /// Records a stopped completion, i.e. an unwind reaching this driver.
    auto set_stopped() && noexcept -> void { *out = make_stopped<Core>(); }
};

/// Connects @p s to an @ref outcome_receiver, starts it, and returns the
/// completion it observed.
///
/// Every sender the evaluator builds completes synchronously and inline, so
/// the operation state can live on the stack for the duration of this call
/// and the recorded outcome is guaranteed to have been written by the time
/// @c start returns.
///
/// @tparam Core The core AST type.
/// @param  s    The sender to run.
/// @return Which of the three channels @p s completed on.
template <typename Core, typename S>
[[nodiscard]] auto run_sender(S &&s) -> outcome<Core> {
    outcome<Core> observed{};
    auto op = sender_v::connect(std::forward<S>(s),
                                outcome_receiver<Core>{&observed});
    sender_v::start(op);
    return observed;
}
```

`sync_wait` would have been shorter and is the wrong tool. It collapses three channels back into two: an engaged `optional` for a value, a disengaged one for stopped, and a *throw* on `set_error`. A backend whose entire claim is that the three channels are distinct can't observe its own completions through an adapter that merges two of them into an exception, and nothing else in this project uses exceptions for diagnosed errors.

One place still calls `sync_wait`, and I'd rather say so than have someone find it. The `cons` arm builds a `when_all` over its two children, because `cons` is the one node whose children are structurally independent, and drains it with `sync_wait`. That's safe because both children have already been evaluated in source order by then, so the `when_all` joins two `just` senders holding values that can't fail. Common Lisp fixes left-to-right evaluation order, and a `when_all` over genuinely deferred children would start the second one even after the first had unwound, so the join says something true about how the results combine and nothing at all about concurrency. The same shape, and the same caveat, as the Scheme backend's builtin case.


# The part that doesn't move

Now the honest half, because it would be easy to write this post as though senders won on points.

Step L16's dynamic binding of a special variable has to be undone when the `let` that established it exits, on every path out, which sounds like exactly the problem `unwind-protect` just solved. It isn't. A `let` elaborates to an immediate lambda application here, so the restore lives in the callee's body loop in `apply_function_value`, and that loop still breaks rather than returns, so that a value, an error and an unwind all leave through the one statement that calls `unwind_dynamic_bindings`. That's the closure backends' only-one-path trick, still doing its job here.

The reason is about what's being bracketed. A cleanup is attached to a *completion*, so a receiver can hold it. A dynamic binding brackets a callee's whole extent, which isn't a completion of anything; it's a region of the recursion, and the natural place to close a region is the C++ scope that opened it. A sender backend is better exactly where the thing being expressed is a completion, and no better anywhere else. I've written that down in `docs/compiler_architecture.org` under Phase 6, because it outlives this step and applies to any backend built on senders.

Worth adding that the shallow-binding helpers themselves are *called*, not reimplemented. `closure::detail::bind_lambda_parameters` and `closure::detail::unwind_dynamic_bindings` are templates over the environment and arena types with no dependency on `eval_direct`, so the sender backend gets special variables by using the same two functions the other two backends use. A second implementation of shallow binding is a second place for it to be wrong.


# Drawing it

Phase 9 walked a Scheme sender type with P2996 reflection and printed the graph (FIXME: Childers, Wyatt and others, 2025). The same trick works here, with two changes forced by what a `smdlisp` graph contains:

```cpp
/// Classifies a sender type by reflecting on the head of its type.
///
/// A port of @ref smd::smdscheme::sender::classify_sender with two
/// extensions, both forced by what a `smdlisp` graph actually contains.
///
/// **First: this backend's own senders are not `basic_sender`s.**
/// @ref unwind_protect_sender and @ref defer_outcome_sender are hand-written,
/// so the tag-based path cannot see them at all and the Scheme classifier
/// would label the two most interesting nodes in any `smdlisp` graph
/// @c "unknown".  They are matched by template identity first.
///
/// **Second: the completion channel lives in the tag's own template
/// argument, not in the tag's name.**  This is the part that is easy to get
/// wrong, and the Scheme classifier has it wrong -- harmlessly, because the
/// Scheme backend only ever built value-channel senders.  In this vendored
/// Beman revision the three families are spelled
///
/// @code
///   just_t<Completion>  then_t<Completion>  let_t<Completion>
/// @endcode
///
/// with `just_error_t` = `just_t<set_error_t>`, `upon_stopped_t` =
/// `then_t<set_stopped_t>`, `let_value_t` = `let_t<set_value_t>`, and so on.
/// So @c identifier_of(template_of(tag)) returns @c "just_t" for all three of
/// @c just, @c just_error and @c just_stopped; distinguishing them needs the
/// tag's argument.  A `smdlisp` graph is largely *about* the non-value
/// channels, so collapsing them would defeat the point of drawing it.
///
/// @c when_all_t is a plain class with no template arguments, and is matched
/// by its own identifier.
///
/// @param type A P2996 reflection of the sender type (dealiased by the
///             caller).
/// @return The node label, or @c "unknown".
consteval auto classify_lisp_sender(std::meta::info type) -> std::string_view {
    // This backend's own senders, matched by template rather than by tag.
    if (std::meta::has_template_arguments(type)) {
        auto const tmpl =
            std::meta::identifier_of(std::meta::template_of(type));
        if (tmpl == "unwind_protect_sender")
            return "unwind_protect";
        if (tmpl == "defer_outcome_sender")
            return "defer";
    }

    auto args = std::meta::template_arguments_of(type);
    if (args.empty())
        return "unknown";
    auto tag = std::meta::dealias(args[0]);

    // A tag with no template arguments names its algorithm directly.
    if (!std::meta::has_template_arguments(tag)) {
        if (std::meta::identifier_of(tag) == "when_all_t")
            return "when_all";
        return "unknown";
    }

    auto const family = std::meta::identifier_of(std::meta::template_of(tag));
    auto const tag_args = std::meta::template_arguments_of(tag);
    if (tag_args.empty())
        return "unknown";
    auto const completion =
        std::meta::identifier_of(std::meta::dealias(tag_args[0]));

    if (family == "just_t") {
        if (completion == "set_error_t")
            return "just_error";
        if (completion == "set_stopped_t")
            return "just_stopped";
        return "just";
    }
    if (family == "then_t") {
        if (completion == "set_error_t")
            return "upon_error";
        if (completion == "set_stopped_t")
            return "upon_stopped";
        return "then";
    }
    if (family == "let_t") {
        if (completion == "set_error_t")
            return "let_error";
        if (completion == "set_stopped_t")
            return "let_stopped";
        return "let_value";
    }
    return "unknown";
}

/// Recursively builds a @ref lisp_plan_tree by reflecting on a sender type.
///
/// Two child-selection rules, because there are two shapes of sender in play:
///
///  - @c basic_sender<Tag,Data,Child...> -- args[0] is the tag, args[1] the
///    data, args[2...] the children, exactly as the Scheme port has it;
///  - @ref unwind_protect_sender<Core,Child,Cleanup> -- args[1] is the single
///    child (args[0] is the core type, args[2] the cleanup callable), and
///    @ref defer_outcome_sender is a leaf.
///
/// P2996 reflections preserve aliases, so a sender type obtained via
/// @c decltype must be dealiased before @c template_arguments_of will
/// recognise it as a specialisation.
///
/// @tparam MaxNodes    Maximum tree capacity (default 64).
/// @param  sender_type P2996 reflection of the sender type to traverse.
/// @param  tree        Output tree; nodes are appended in pre-order.
/// @return The integer id of the root node allocated for @p sender_type.
template <int MaxNodes = 64>
consteval auto build_lisp_tree(std::meta::info sender_type,
                               lisp_plan_tree<MaxNodes> &tree) -> int {
    auto type = std::meta::dealias(sender_type);
    lisp_node_data node{};
    node.sender_algo = classify_lisp_sender(type);
    auto id = tree.allocate(node);

    auto args = std::meta::template_arguments_of(type);
    if (node.sender_algo == "unwind_protect") {
        auto child_id = build_lisp_tree<MaxNodes>(args[1], tree);
        tree.get(id).child_ids.push_back(child_id);
        return id;
    }
    if (node.sender_algo == "defer")
        return id; // A leaf: its thunk is opaque to reflection.

    for (std::size_t i = 2; i < args.size(); ++i) {
        auto child_id = build_lisp_tree<MaxNodes>(args[i], tree);
        tree.get(id).child_ids.push_back(child_id);
    }
    return id;
}
```

The first change is that the two most interesting nodes in any graph here are hand-written senders and not `basic_sender` specializations, so the tag-based path can't see them and they're matched by template identity.

The second one caught me out. The completion channel lives in the tag's own template argument and not in the tag's name. In this vendored Beman revision `just_error_t` *is* `just_t<set_error_t>`, `upon_stopped_t` is `then_t<set_stopped_t>`, `let_value_t` is `let_t<set_value_t>`, and so on down the three families. So `identifier_of(template_of(tag))` answers `"just_t"` for all three of `just`, `just_error` and `just_stopped`, and the classifier has to read `tag_args[0]` to tell them apart. The Scheme classifier from Phase 9 doesn't do that, and gets the right answer anyway, because the Scheme backend never built a sender that completed on any channel but `set_value`. It was never wrong about a graph it was ever shown. A `smdlisp` graph is largely *about* the other two channels, so here it would have collapsed the whole point into one label.

`src/examples/lisp_sender_graph_demo.cpp` runs Phase 19's cleanup-ordering program through this backend and prints both the answer and the graph:

```
// smdlisp sender backend
// program: (let ((log 0))  (catch 'c    (unwind-protect      (unwind-protect (throw 'c 99) (setq log (+ (* log 10) 1)))      (setq log (+ (* log 10) 2))))  log)
// cleanup log (innermost first): 12
// Pipe to: dot -Tpng -o plan.png

digraph LispExecutionPlan {
  node [shape=Mrecord, style=filled, fillcolor=lightyellow];
  n0 [label="unwind_protect\n[]"];
  n0 -> n1;
  n1 [label="unwind_protect\n[]"];
  n1 -> n2;
  n2 [label="defer\n[]"];
}
```

12, so the inner cleanup ran before the outer one, and the thrown 99 still reached the `catch` with both of them already done. The stop crossed two adapters and each one ran its cleanup on the way past.

The graph is three nodes, and that's the honest size of it. The `catch`, the `throw`, the `let` and the arithmetic are all evaluated by the Mendler algebra as ordinary C++, so none of them is a node. Only `unwind-protect` is delegated to a sender adapter, because `unwind-protect` is the only form in this language whose meaning *is* "do something on completion". A picture of what this backend expresses as a graph is a picture of the cleanups.


# What the tests can and can't say

There's no `static_assert` anywhere in this backend, and there can't be. `connect` and `start` aren't constant-evaluable, and `sync_wait` runs a `run_loop` over a mutex and a condition variable, so a sender can't be started during constant evaluation and there's no compile-time value for an assertion to be about. The closure backends state each merge criterion twice, once inside a `static_assert` and once as a run-time `TEST_CASE`; this one has the run-time half only, which is DIV-0015.

I want to be careful about what that costs, because it's tempting to overstate it in either direction. The standing rule since L14 is "every `static_assert` needs a run-time twin, because a constexpr-green mechanism can still be broken at run time," and that rule exists to stop anyone trusting compile-time evidence on its own. Here there's no compile-time evidence to over-trust. The backend has only the kind of evidence the rule says to insist on. What's genuinely weaker is the *parity* claim: 68 run-time cases, 65 of which copy their source string verbatim out of `cps_code.test.cpp` or `eval_direct.test.cpp` so the claim is checkable by diffing the sources, and it's parity between programs and their answers, which says nothing about which evaluation phase produced them. The other three have no counterpart in the closure backends and can't have one, because the thing they witness doesn't exist there: which of three channels a program completed on. Those are this backend's own merge criterion, and the third of them is the interesting one, since it pins that no well-formed program completes stopped at top level. A stop only gets created after a `return-from` or `throw` has already found a live target, so the scope that will consume it is always still below on the stack. `sender_program::run` is left unmarked `constexpr` on purpose, so the constraint shows in the API and doesn't live only in the tests. 754 tests green at the merge.

Two more divergences to record. The Mendler paramorphism is re-derived locally, eight lines of it, because `smd::fixpoint::mendler_para` takes a `Fix<F>` and the `smdlisp` core is a `smdscheme::foundation::fix`, which is the same shape under a different name in a frozen tree. There are now two of them in the repository with the same semantics and different parameter types, which is DIV-0016 and reads like an accident if you find it cold. And the backend doesn't cover multiple values, because L20 was being built in a sibling worktree and hadn't landed; `outcome`'s value channel carries exactly one `value<Core>`, and widening it before L20 settles its representation would have meant guessing and then disagreeing. DIV-0017, and L20 itself is where it closes.


# Collecting on the argument

Phase 15 made a claim about contracts before any of the code existed: one-shot, upward-only, dynamic extent is what a sender's completion contract can carry, and `call/cc` isn't. Two steps of implementation later there was a Common Lisp core with all of that in it and no sender anywhere near it. Now there is.

The thing I didn't expect is which direction the win came from. I thought the sender backend would be harder and would buy correctness by being checked more strictly. What it actually bought was one form, `unwind-protect`, turning from a loop I had to keep unconditional into a receiver that can't enumerate its channels wrongly because it doesn't enumerate them. Everything else either transcribed straight across or, in the dynamic-binding case, refused to move at all.

Next up is L20's multiple values, which this backend is deliberately unprepared for.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 20 - defmacro](phase-20-defmacro.md)

</nav>


# References

Dominiak, Michał and others (2024). *P2300: std::execution*, C++ Standards Committee Papers.

FIXME: Childers, Wyatt and others (2025). *P2996: Reflection for C++26*, C++ Standards Committee Papers.

Steele, Guy L. (1990). *Common Lisp the Language*, Digital Press.
