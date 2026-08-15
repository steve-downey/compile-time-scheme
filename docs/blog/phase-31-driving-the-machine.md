**DRAFT &mdash; pending author revision**

<div class="abstract" id="org3f9c0a0">
<p>
R7 gives the rebuild tree a second backend over its core AST, alongside R5's abstract machine (decision D17): a Beman Execution sender that connects, runs <code>eval::machine</code>'s own trampoline to completion, and dispatches the result onto <code>set_value</code>, <code>set_error</code>, and <code>set_stopped</code>, D13's three channels spent a second time.
Two shapes were on the table before any of that could be written, and the more tempting one (forking a call's argument list into sibling senders, the way the pivot did) was rejected on a concrete cost: the machine is single-owner state, and reproducing that trick would mean rebuilding a recursive evaluator on the sender side, in the one place R5 spent a whole phase removing recursion from.
Nothing here is <code>constexpr</code>; DIV-0015 says a sender backend never gets to be, and that absence is stated in the API instead of turning up later in a test.
The one <code>when_all</code> demonstration joins two independent whole programs, and says nothing about the order in which one program's arguments run.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 30 - Asking Someone Else Whether the Answers Are Right ←](phase-30-asking-someone-else.md)

</nav>


# Ruling out the pivot's answer first

Decision D17 asks for a second backend over the rebuild's core tree, built on Beman Execution (Dominiak, Michał and others, 2024), next to the abstract machine R5 built and R6's conformance corpus now exercises. The pivot already has one of these, `smd::smdlisp::sender::sender_eval`, so the obvious move is to port it. The step brief rules that out before anything else: `sender_eval` is a Mendler fold (Mendler, Nax Paul, 1987) over a pointer tree it does not itself define. `Fix`, `Box`, and the `Comp` tree live in `smd::smdscheme::sender::comp_tree` (`sender_mendler_eval.hpp` and `fixpoint_eval.hpp` behind it, in the dead-ended Scheme front end), a second full spelling of every core node kind next to the columnar one this tree already has. DIV-0016 priced that at "a step's worth of work on its own and buys nothing this step needs," and `Comp`'s capacities live inside its types (`comp_lambda<A, MaxList>`, `env<CompT, 16>`), which is the D14 violation the rebuild exists to remove. Porting it would get the sender backend built by default rather than by decision, which is the thing the brief is most insistent this step not do.

So R7 opens with no inherited shape, only a vocabulary to borrow the spelling of.

```cpp
using beman::execution26::just;
using beman::execution26::just_error;
using beman::execution26::just_stopped;
using beman::execution26::sync_wait;
using beman::execution26::then;
using beman::execution26::when_all;

using beman::execution26::completion_signatures;
using beman::execution26::connect;
using beman::execution26::connect_result_t;
using beman::execution26::operation_state_tag;
using beman::execution26::receiver_tag;
using beman::execution26::sender_tag;
using beman::execution26::set_error;
using beman::execution26::set_error_t;
using beman::execution26::set_stopped;
using beman::execution26::set_stopped_t;
using beman::execution26::set_value;
using beman::execution26::set_value_t;
using beman::execution26::start;
```

A fresh copy, because both pivot trees are off limits for a different reason each: `smdscheme` is dead-ended, `smdlisp` is the behavioural oracle from R1 on; neither is a dependency `src/smd/cl` is allowed to take on.


# The shape that looked free

The 2026-08-06 note on DIV-0016 is the reason this step's choice wasn't obvious going in. It says the columnar tree **inverts** the pivot's own claim that a CPS trampoline linearizes argument independence: a branch's whole child list is one `static_vector`, visible at once, at `machine::evaluate_subforms`. Read on its own, that reads like an argument *for* forking `k_arguments` into a `when_all` over sibling senders: the pivot's trick, minus the pointer tree that made it necessary in the first place.

I want to grant that reading, because it's the honest one and it's what made the step worth stopping on instead of assuming. `eval::machine` is single-owner state: one frame stack, one heap, one control register, all mutated in place by `step`. Forking an argument list needs each sibling to make independent progress, and nothing about that state supports two siblings advancing it at once. The only ways to get there are a second frame stack per sibling (at which point the siblings aren't inside *this* machine any more, they're separate machines evaluating sub-terms, with every environment-sharing question that raises for `setq` and `defun` visibility reopened), or a recursive call back into a fresh evaluator built in C++ recursion instead of the frame stack. That second option spends the property R5 built to buy: no evaluation recursion is C++ recursion, so `limits::frames` and `limits::steps` are diagnosed capacities instead of stack depth. `limits::steps` defaults to 200000. Rebuilding a Mendler-shaped evaluator on the sender side to get `when_all` over one call's arguments turns 200000 steps into 200000 C++ activation records, in the one place a whole phase was spent removing them.

Not underdetermined, once the cost is stated. R7 drives the machine whole instead.


# One machine, run to completion

```cpp
/// The completion signatures every `machine_sender` reports: decision D13's
/// three channels, unchanged, mapped onto the three Execution26 completion
/// functions one for one.
using machine_completions = sender_v::completion_signatures<
    sender_v::set_value_t(eval::value),
    sender_v::set_error_t(foundation::parse_error), sender_v::set_stopped_t()>;

/// The operation state of a @ref machine_sender: owns the machine itself, and
/// runs it to completion when started.
///
/// Immovable, like every hand-written operation state in this style — a
/// `Receiver` may itself be non-trivial to relocate once storage for it has
/// been handed out, and nothing here needs to move one after @c connect.
///
/// @tparam Limits      The machine's storage and step budget.
/// @tparam CoreTree    The @ref core::core_tree instantiation to evaluate.
/// @tparam SymbolTable The symbol table the program resolves names in.
/// @tparam Receiver    The connected receiver.
template <eval::limits Limits, class CoreTree, class SymbolTable,
          class Receiver>
struct machine_operation_state {
    using operation_state_concept = sender_v::operation_state_tag;

    /// Builds the machine and stores where its result is going.
    machine_operation_state(CoreTree const &program, SymbolTable &symbols,
                            eval::standard_symbols known, int root,
                            Receiver receiver)
        : machine_{program, symbols, known}, root_{root},
          receiver_{std::move(receiver)} {}

    machine_operation_state(machine_operation_state const &) = delete;
    auto operator=(machine_operation_state const &)
        -> machine_operation_state & = delete;

    /// Runs the machine's trampoline to completion, then dispatches its
    /// @ref eval::outcome onto the matching completion channel.
    ///
    /// The one case worth naming: an unwind that reaches here is a nonlocal
    /// exit that no `block`/`catch` inside the program claimed — every live
    /// one already had first refusal, inside `machine::run` itself
    /// (`detail::k_block`'s `resume_visitor`, `machine.hpp`). There is
    /// nowhere left for its target and payload to go, and Execution26's
    /// `set_stopped` is niladic by contract, so both are dropped here,
    /// deliberately, on the one channel built to say exactly that: stopped,
    /// no data.
    auto start() & noexcept -> void {
        eval::outcome const completed = machine_.run(root_);
        if (completed.is_value()) {
            sender_v::set_value(std::move(receiver_), completed.as_value());
            return;
        }
        if (completed.is_error()) {
            sender_v::set_error(std::move(receiver_), completed.as_error());
            return;
        }
        sender_v::set_stopped(std::move(receiver_));
    }

  private:
    eval::machine<Limits, CoreTree, SymbolTable> machine_;
    int root_;
    Receiver receiver_;
};
```

`machine_completions` is D13's mapping, spent a second time: value to `set_value_t`, diagnosed error to `set_error_t`, unwind to `set_stopped_t`, one for one. `machine_operation_state` owns the machine itself, and `start` runs its trampoline (unchanged, still one loop, still bounded by the same `limits`) to completion before looking at what it produced. The one case the comment in the code calls out: an unwind that reaches here is a `return-from` or `throw` no `block` or `catch` inside the program claimed, meaning every live one already had first refusal at it inside `machine::run` itself. `set_stopped` is niladic by Execution26's own contract, so the unwind's target and payload are dropped, deliberately, on the one channel built to say just that: stopped, no data.


# The whole surface is two functions

```cpp
/// A sender that evaluates one elaborated core tree, from @p root, when
/// started — the sender backend's whole surface.
///
/// Holds the program and symbol table by pointer rather than by reference so
/// the type stays copyable, which @c connect's by-value-sender convention
/// wants; the pointees must outlive every connected operation, exactly as
/// @ref eval::machine's own reference members require of its caller.
///
/// @tparam Limits      The machine's storage and step budget.
/// @tparam CoreTree    The @ref core::core_tree instantiation to evaluate.
/// @tparam SymbolTable The symbol table the program resolves names in.
template <eval::limits Limits, class CoreTree, class SymbolTable>
struct machine_sender {
    using sender_concept = sender_v::sender_tag;

    CoreTree const *program;      ///< The elaborated program.
    SymbolTable *symbols;         ///< Where names resolve.
    eval::standard_symbols known; ///< Resolved `NIL` and `T`.
    int root;                     ///< The node to evaluate.

    /// Reports @ref machine_completions, independent of environment.
    template <typename Self, typename... Env>
    static consteval auto get_completion_signatures() {
        return machine_completions{};
    }

    /// Connects @p r, building the machine that @c start will run.
    template <typename Receiver>
    auto connect(Receiver r)
        && -> machine_operation_state<Limits, CoreTree, SymbolTable, Receiver> {
        return machine_operation_state<Limits, CoreTree, SymbolTable, Receiver>{
            *program, *symbols, known, root, std::move(r)};
    }
};

/// Builds a sender that evaluates @p program's node @p root.
///
/// @tparam Limits      The machine's storage and step budget.
/// @param  program     The elaborated program; must outlive every connected
///                      operation.
/// @param  symbols     The symbol table; must outlive every connected
///                      operation.
/// @param  known       Resolved `NIL` and `T`.
/// @param  root        The node to evaluate.
template <eval::limits Limits = eval::limits{}, class CoreTree,
          class SymbolTable>
[[nodiscard]] auto evaluate(CoreTree const &program, SymbolTable &symbols,
                            eval::standard_symbols known, int root)
    -> machine_sender<Limits, CoreTree, SymbolTable> {
    return machine_sender<Limits, CoreTree, SymbolTable>{&program, &symbols,
                                                         known, root};
}

/// Builds a sender that evaluates @p program's root node.
///
/// @tparam Limits      The machine's storage and step budget.
/// @param  program     The elaborated program; must outlive every connected
///                      operation.
/// @param  symbols     The symbol table; must outlive every connected
///                      operation.
/// @param  known       Resolved `NIL` and `T`.
template <eval::limits Limits = eval::limits{}, class CoreTree,
          class SymbolTable>
[[nodiscard]] auto evaluate(CoreTree const &program, SymbolTable &symbols,
                            eval::standard_symbols known)
    -> machine_sender<Limits, CoreTree, SymbolTable> {
    return evaluate<Limits>(program, symbols, known, program.root());
}
```

`machine_sender` holds its program and symbol table by pointer, because `connect`'s by-value convention wants a copyable sender, and the pointees have to outlive every connected operation; the same contract `eval::machine` already puts on its own caller, restated here and not loosened. `evaluate` is the entry point, and the second overload defaulting to `program.root()` is the one every test below actually calls: name a program and a symbol table, get back something you can `connect`, `start`, or fold into a larger graph with `when_all` or `then`.


# The inverse direction, for a caller that wants an outcome back

```cpp
/// A receiver that records which of three completion channels fired, as an
/// @ref eval::outcome.
///
/// The inverse of @ref machine_operation_state::start (`machine_sender.hpp`):
/// that turns an @c eval::outcome into a completion, this turns a completion
/// back into one. Both directions exist for the same reason -- D13's claim is
/// that a value/error/unwind outcome and a value/error/stopped completion are
/// the same fact seen from two sides -- so both halves of the round trip are
/// worth having as named code rather than as a claim in a comment.
///
/// A stopped completion carries nothing back (Execution26's `set_stopped` is
/// niladic, and by the time one reaches the top of a program every live
/// `block`/`catch` has already had first refusal at it), so it is reported as
/// a diagnosed error: an unwind with nowhere left to land is not a value, and
/// there is no record left of which exit it even was.
struct outcome_receiver {
    using receiver_concept = sender_v::receiver_tag;

    eval::outcome *out; ///< Where the observed completion is written.

    /// Records a value completion.
    auto set_value(eval::value v) && noexcept -> void {
        *out = eval::outcome{std::move(v)};
    }
    /// Records an error completion.
    auto set_error(foundation::parse_error e) && noexcept -> void {
        *out = eval::outcome{e};
    }
    /// Records a stopped completion: an unwind reached this driver with no
    /// scope left to claim it.
    auto set_stopped() && noexcept -> void {
        *out = eval::outcome{
            foundation::parse_error{{},
                                    "an unwind reached the top of the "
                                    "program uncaught"}};
    }
};

/// Connects @p s to an @ref outcome_receiver, starts it, and returns the
/// completion it observed.
///
/// Every sender this backend builds completes synchronously and inline —
/// DIV-0015 records that nothing here can suspend across a real scheduler
/// boundary yet — so the operation state can live on the stack for the
/// duration of this call and the observed outcome is guaranteed written by
/// the time @c start returns.
///
/// @tparam S A sender whose completions are exactly @ref machine_completions'
///           three: `set_value_t(eval::value)`, `set_error_t(parse_error)`,
///           `set_stopped_t()`.
template <typename S>
[[nodiscard]] auto run_sender(S &&s) -> eval::outcome {
    eval::outcome observed{eval::value{}};
    auto op =
        sender_v::connect(std::forward<S>(s), outcome_receiver{&observed});
    sender_v::start(op);
    return observed;
}
```

`machine_operation_state::start` turns an `eval::outcome` into a completion; `outcome_receiver` turns a completion back into one. Both directions exist as named code rather than as a claim in a comment, because D13's actual claim is that a value/error/unwind outcome and a value/error/stopped completion are the same fact seen from two sides, and a claim like that is worth being able to run in both directions and check. A stopped completion is reported back as a diagnosed error: there's no payload left to recover, and no record of which exit it even was, so "an unwind reached the top of the program uncaught" is the most `run_sender` can say.

None of this is `constexpr`. DIV-0015 is accepted-permanent: Beman Execution's `connect`, `start`, and `sync_wait` are not constant-evaluable, so a sender graph has no compile-time value to produce, so there is nothing here for a `static_assert` to be about. `eval::machine` itself stays as constexpr-capable as R5 left it; only the sender wrapped around it loses that property, and it loses it in the type, not just in the tests: `start` is an ordinary noexcept member function, nothing about its signature invites constant evaluation. Only `eval::machine` is a compile-time evaluator. The sender backend never was.


# Two programs, never one program's arguments

```cpp
TEST_CASE("MachineSenderTest - WhenAllJoinsTwoIndependentPrograms") {
    // What this test is not: a claim about evaluation order inside one
    // program. R7's step brief is explicit that left-to-right evaluation is
    // ANSI's conforming order (3.1.2.1.2.3) the moment an argument can have
    // an effect, so nothing in this backend reorders a call's arguments.
    //
    // What it is: two *separate* top-level programs -- separate symbol
    // tables, separate elaborated trees, separate machines and heaps once
    // connected, no shared mutable state at all -- joined with `when_all`.
    // That is a true structural-independence claim, the one `when_all`
    // honestly supports here, and it is the shape a sender backend adds
    // that `eval::machine::run` alone does not offer: composing whole
    // programs as ordinary sender values.
    compiled first = compile_source("(+ 2 3)");
    compiled second = compile_source("(* 4 5)");

    auto joined = sender_v::then(
        sender_v::when_all(evaluate<standard_limits>(
                               first.program, first.symbols, first.known),
                           evaluate<standard_limits>(
                               second.program, second.symbols, second.known)),
        [](value a, value b) noexcept -> value {
            int const lhs = std::get<value_fixnum>(a).value;
            int const rhs = std::get<value_fixnum>(b).value;
            return value{value_fixnum{lhs + rhs}};
        });

    auto const completed = run_sender(std::move(joined));
    REQUIRE(completed.is_value());
    CHECK(completed.as_value() == value{value_fixnum{25}});
}
```

This is what driving the machine whole still buys, once the forking shape is off the table: two whole-program evaluations, each its own machine and its own heap, share no mutable state at all, so joining them with `when_all` is a true structural-independence claim. It says nothing about the order in which `(+ 2 3)`'s own two arguments would run, because nothing here reorders `k_arguments` at all; left to right stays left to right, as R5 left it. Left-to-right argument evaluation is ANSI's conforming order the moment an argument can have an effect (3.1.2.1.2.3) (Steele, Guy L., 1990), and D19 aims at all of ANSI, so that ordering was never R7's to renegotiate. Two separate `(+ 2 3)` and `(* 4 5)` calls, in two separate machines, joined and summed by `then`: that's the whole demonstration, and it claims no more than the test's own name says.


# What's still owed

The corpus R6 built doesn't run through this backend yet; every case in it still checks `eval::machine::run` directly, and a sender-side differential pass against the same seventy-five entries is a straightforward thing nobody has written. Multiple values are still unreachable from anywhere in this tree, so there's no widened completion signature to worry about yet the way the pivot eventually had to. And the kit R8 goes looking for next (`static_vector`, `result`, the parser combinators, the pieces three different front ends have each grown their own copy of) lives underneath everything R7 touched, which is why R7 didn't have to touch it.

R8 is next.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 30 - Asking Someone Else Whether the Answers Are Right](phase-30-asking-someone-else.md)

</nav>


# References

Dominiak, Michał and others (2024). *P2300: std::execution*, C++ Standards Committee Papers.

Mendler, Nax Paul (1987). *Recursive Types and Type Constraints in Second-Order Lambda Calculus*.

Steele, Guy L. (1990). *Common Lisp the Language*, Digital Press.
