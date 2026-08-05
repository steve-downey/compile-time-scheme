**DRAFT &#x2014; pending author revision**

<div class="abstract" id="org72d7078">
<p>
Step R5 gives the rebuild an evaluator.
An evaluated form finishes in one of three ways: with a value, with a diagnosed error, or with an unwind in flight, and <code>eval::outcome</code> makes those three alternatives of one type, so control flow never travels in the error channel.
That's decision D13, and the pivot's sentinel messages compared by pointer identity are deleted rather than ported.
<code>unwind-protect</code> is the form the split breaks, and it gets rebuilt as a continuation frame.
The evaluator itself is a small-step abstract machine, because evaluation is the one traversal in this pipeline that can't be a fold.
And <code>(defun len (l) (if (null l) 0 (+ 1 (len (cdr l)))))</code> works, inside a <code>static_assert</code>.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 27 - The Elaborator Never Recurses ←](phase-27-elaboration.md)

</nav>


# The sentinels that had to be objects

Four things travel down the closure backends' one wire: a value, a diagnosed error, and two kinds of unwind, one for `return-from` and one for `throw`. All four arrive as a `result<value>`, so telling them apart needs something out of band, and what the pivot used was a pair of `parse_error` messages compared by pointer identity. Compared by pointer, which is why they had to be named `inline constexpr char[]` objects and not bare string literals: two equal string literals are not required to share an address, so `==` on the pointers is only meaningful if there's exactly one object to point at. Phase 21 already recorded that the sender backend doesn't do any of this, because a sender has three completion channels natively and there's nothing to multiplex.

D13 says the two backends that got it wrong should be doing what the one that got it right does, and R5 is where that becomes a type.

```cpp
/// A non-local exit in flight: which exit point it is aimed at, and the
/// value it is carrying there.
///
/// The target is the block's name. "Is this unwind aimed at me?" is asked
/// and answered by the block's own frame, which is where the question
/// belongs — no evaluator-wide register, and no sentinel.
struct unwind {
    symbol::symbol_id target{}; ///< The block being returned from.
    value payload{};            ///< The value it carries.

    // HIDDEN FRIEND
    friend constexpr auto operator==(unwind const &, unwind const &)
        -> bool = default;
};

/// How one evaluation completes: with a value, with a diagnosed error, or
/// with an unwind in flight. Decision D13.
///
/// The three are distinct alternatives of one type, so control flow does
/// not travel in the error channel. The closure backends of the pivot
/// multiplexed all of this onto a two-alternative @c result and told the
/// cases apart with a pair of sentinel @c parse_error messages compared by
/// pointer identity — which is why those sentinels had to be named
/// @c inline @c constexpr @c char[] objects rather than bare literals,
/// since equal string literals need not share an address. None of that
/// machinery is ported. There is nothing to discriminate.
///
/// This is the generalisation of what the sender backend already got right
/// (`docs/compiler_architecture.org`, "A sender has three completion
/// channels, and that is a semantic resource"): a value to @c set_value, an
/// error to @c set_error, an unwind to @c set_stopped. What that section
/// warns must be *reconstructed* rather than inherited is
/// `unwind-protect`'s run-the-cleanup-on-every-exit-path property, because
/// with the channels split there is no longer a single merged exit path for
/// one unconditional statement to sit on. In this machine it is
/// reconstructed by a continuation frame that intercepts all three
/// alternatives and funnels them through the same cleanup —
/// see @c detail::k_protect in <smd/cl/eval/machine.hpp>.
class outcome {
  public:
    /// Completes with @p v.
    constexpr outcome(value v);

    /// Completes with the diagnosed error @p e.
    constexpr outcome(foundation::parse_error e);

    /// Completes with the unwind @p u in flight.
    constexpr outcome(unwind u);

    /// Returns true if this completed with a value.
    [[nodiscard]] constexpr auto is_value() const -> bool;

    /// Returns true if this completed with a diagnosed error.
    [[nodiscard]] constexpr auto is_error() const -> bool;

    /// Returns true if this completed with an unwind in flight.
    [[nodiscard]] constexpr auto is_unwind() const -> bool;

    /// Returns the value.
    /// @pre is_value()
    [[nodiscard]] constexpr auto as_value() const -> value const &;

    /// Returns the error.
    /// @pre is_error()
    [[nodiscard]] constexpr auto as_error() const
        -> foundation::parse_error const &;

    /// Returns the unwind.
    /// @pre is_unwind()
    [[nodiscard]] constexpr auto as_unwind() const -> unwind const &;

    // HIDDEN FRIEND
    friend constexpr auto operator==(outcome const &, outcome const &)
        -> bool = default;

  private:
    std::variant<value, foundation::parse_error, unwind> data_;
};
```

An `unwind` carries the name of the exit point it's aimed at and the value it's carrying there, and that's all the state a nonlocal exit needs. There's no evaluator-wide "returning" register, because the question *is this unwind aimed at me?* is asked by the block's own frame, which is the only place that knows the answer. There's a `to_outcome` that widens an ordinary `result<value>` into the three-channel form; the other direction isn't a function at all, and that's the whole point of the split. An unwind in flight has nowhere to go in a type with two alternatives, and pretending otherwise is what the sentinels were for.


# unwind-protect is what the split costs

D13's record says what the split would cost before the step opened it. Under a one-wire result, `unwind-protect` gets "run the cleanup on every exit path" free: the outcomes are already merged into one value by the time it sees them, so one unconditional statement covers every path there is. Split the channels and there's no merged path left to hang that statement on. The property has to be rebuilt.

```cpp
/// Waiting for a protected form, then for each cleanup in turn.
///
/// This frame is where `unwind-protect`'s run-the-cleanup-on-every-exit-path
/// property is *reconstructed*, which
/// `docs/compiler_architecture.org` names as the consequence of splitting
/// the channels that has to be planned for rather than discovered. Under a
/// one-wire result, the four outcomes are merged before `unwind-protect`
/// sees them, so a single unconditional statement suffices. Here they are
/// not merged, so the frame saves whichever of the three arrived, runs the
/// cleanups, and re-emits the saved one. Cleanup on all three channels is
/// then a property of the frame's shape rather than a checklist item.
struct k_protect {
    int node = -1; ///< The `core_unwind_protect` node.
    value env{};   ///< The environment its children run in.
    int stage = 0; ///< Which child is running: 0 protected, 1+ cleanup.
    outcome saved{value{}}; ///< How the protected form completed.
};
```

The frame saves whichever of the three arrived from the protected form, runs the cleanups in order, and re-emits the saved one. Cleanup-on-all-three-channels is then a property of the frame's shape, and there's no list of cases anywhere that a fourth channel could get left off of.

One case doesn't re-emit. If a cleanup form itself completes with an error or an unwind, that outcome replaces what it was cleaning up after, and there's nothing left to put back. The frame pops and lets the new one travel. That's one arm of an `if` in the resume visitor, and it reads as an ordinary rule.

I'd rather not oversell the split. D13 says so too: restoring a dynamic binding stays in ordinary C++ control flow, because it brackets a callee's whole extent instead of a single completion, and nothing about having three channels helps with it. Splitting is better where the thing being expressed is a completion. There are no special variables in this tree yet, so I haven't had to prove that half.


# Evaluation is not a fold

Phase 27 ended on a question: whether an evaluator over the same core tree could stay in the shape the elaborator is in: three named schemes and no recursive descent, because a `tagged_tree` built children-before-parent has its nodes in topological order already. I wrote that I didn't see why it couldn't, and that this wasn't the same as knowing. It couldn't, and it isn't close. But having `cata` on the shelf is what makes that a checkable claim instead of a mood: the fold the machine isn't is a named thing with laws, and the reasons it isn't one are the reasons below.

```cpp
/// A small-step abstract machine that evaluates an elaborated core tree.
///
/// **Why a machine and not a recursive walk.** The rules say a recursion
/// over a tree is a catamorphism, and R4's elaborator is three folds over
/// node indices for exactly that reason. Evaluation is not one of those
/// folds and cannot be made into one: a fold visits every node in a fixed
/// order, and evaluation visits `if`'s arms selectively, a function body
/// once per call rather than once per node, and — with `return-from` — not
/// at all past the point of an unwind. What it *is* is continuation-passing
/// style with the continuations defunctionalised: @c detail::k_if and its
/// siblings are the continuation constructors, the frame stack is the
/// continuation, and @c step is the transition function. That leaves one
/// loop, and it lives in @ref foundation::trampoline rather than here.
///
/// A consequence worth having: no evaluation recursion is C++ recursion, so
/// the depth a program may reach is a diagnosed capacity (@c limits::frames)
/// rather than the C++ stack, at compile time as at run time.
///
/// **How a step completes.** With one of the three alternatives of
/// @ref outcome, per decision D13. A frame receives one and decides: most
/// pass an error or a foreign unwind straight through, @c detail::k_block
/// catches an unwind aimed at its own name, and @c detail::k_protect
/// intercepts all three.
///
/// @tparam Limits      Storage and budget.
/// @tparam CoreTree    The @ref core::core_tree instantiation to evaluate.
/// @tparam SymbolTable The symbol table; its function slot must hold a
///                     @ref function_binding.
template <limits Limits, class CoreTree, class SymbolTable>
class machine {
  public:
    /// This machine's object storage.
    using heap_type = heap<Limits.cells, Limits.string_chars, Limits.closures>;

    /// Evaluates trees of @p program, resolving names in @p symbols.
    constexpr machine(CoreTree const &program, SymbolTable &symbols,
                      standard_symbols known);

    /// Evaluates the node @p root and returns how it completed.
    ///
    /// A spent step budget is the one failure the machine reports as an
    /// error of its own rather than as a program outcome.
    [[nodiscard]] constexpr auto run(int root) -> outcome;

    /// Evaluates the program's root node.
    /// @pre the program has a root
    [[nodiscard]] constexpr auto run() -> outcome;

    /// Returns the heap, so a caller can read the objects a value names.
    [[nodiscard]] constexpr auto store() const -> heap_type const &;

  private:
    using branch_type = typename CoreTree::branch_type;
    using k_args = detail::k_arguments<Limits.call_args>;
    using frame =
        std::variant<detail::k_if, detail::k_sequence, k_args, detail::k_block,
                     detail::k_return_from, detail::k_protect>;
    using arg_list = foundation::static_vector<value, Limits.call_args>;

    struct leaf_visitor;
    struct branch_visitor;
    struct resume_visitor;
    struct combine_visitor;

    /// Advances the machine one transition.
    constexpr auto step() -> foundation::step_status;

    /// Evaluates the node in control position.
    constexpr auto evaluate() -> void;

    /// Hands the pending outcome to the innermost frame.
    constexpr auto resume() -> void;

    /// Completes the current evaluation with @p o.
    constexpr auto complete(outcome o) -> void;

    /// Schedules @p node for evaluation in @p env.
    constexpr auto go_to(int node, value env) -> void;

    /// Pushes @p f, or completes with a diagnosed control-stack overflow.
    /// @return true if the frame was pushed
    [[nodiscard]] constexpr auto push(frame f) -> bool;

    /// Evaluates @p node's children left to right and then combines them;
    /// used by calls and by hermetic pairs alike.
    constexpr auto evaluate_subforms(int node, branch_type const &branch)
        -> void;

    /// Evaluates @p node's children as a body: left to right, for the last
    /// one's value, with the empty body evaluating to `nil`.
    constexpr auto evaluate_body(int node, branch_type const &branch, value env)
        -> void;

    /// Completes with a diagnosed malformed core tree.
    constexpr auto malformed() -> void;

    /// Combines @p node's already-evaluated subforms @p args.
    constexpr auto combine(int node, arg_list const &args) -> void;

    /// Calls whatever the function slot of @p callee holds.
    constexpr auto call_function(symbol::symbol_id callee, arg_list const &args)
        -> void;

    /// Enters the closure @p ref with @p args bound to its parameters.
    constexpr auto enter_closure(closure_ref ref, arg_list const &args) -> void;

    /// Returns @p env extended by binding @p params to @p args pairwise.
    [[nodiscard]] constexpr auto bind_parameters(
        foundation::static_vector<symbol::symbol_id,
                                  core::max_lambda_params> const &params,
        arg_list const &args, value env) -> foundation::result<value>;

    CoreTree const &program_;
    SymbolTable &symbols_;
    standard_symbols known_;
    heap_type store_{};
    foundation::static_vector<frame, Limits.frames> konts_{};
    int control_ = -1;
    value env_{};
    outcome pending_{value{}};
};
```

A catamorphism visits every node once, in an order the structure fixes (Meijer, Erik and Fokkinga, Maarten and Paterson, Ross, 1991). Evaluation visits one arm of an `if` and never the other, visits a function body once per call and not once per node, and past a `return-from` visits nothing at all. Those aren't awkward cases to be handled inside a fold; they're the reason the fold doesn't typecheck as a description of what's happening. So the coding rules' preference for a traversal typeclass over hand-written recursion runs out here, and what replaces it is the other classical answer: continuation-passing style with the continuations defunctionalized (Reynolds, John C., 1972) (Danvy, Olivier and Nielsen, Lasse R., 2001). `k_if`, `k_sequence`, `k_arguments`, `k_block`, `k_return_from` and `k_protect` are the continuation constructors, a `static_vector` of them is the continuation, and `step` is `apply`.

That leaves exactly one loop in the whole evaluator, and it isn't in the evaluator:

```cpp
template <class State, class Step>
    requires machine_step<Step, State>
[[nodiscard]] constexpr auto trampoline(State &state, int max_steps, Step step)
    -> result<int> {
    int taken = 0;
    while (taken < max_steps) { // substrate generic algorithm
        ++taken;
        if (std::invoke(step, state) == step_status::done) {
            return taken;
        }
    }
    return parse_error{{}, "evaluation step limit exceeded"};
}
```

The budget isn't a safety net bolted on afterwards. An object program that doesn't terminate must not turn into a translation that doesn't terminate: an unbounded loop under constant evaluation is a compiler that hangs, with no output and nothing to read. So the step count is part of the contract, and running out of it is diagnosed the same ordinary way every capacity in this tree is diagnosed. The same is true of depth. No evaluation recursion is C++ recursion here, so how deep a program may go is `limits::frames`, a number in a struct. The compiler's own stack doesn't come into it.

`foundation/` grew two things for this and no more: `static_vector::pop_back`, because a continuation stack is the substrate's first client that shrinks, and `trampoline.hpp` itself. Whether `trampoline` is the right shape is a question a second backend answers, and the second backend is R7.


# Five new tags, and D18's bar

```cpp
/// A function definition: the parameters it binds, and one child, the body
/// it evaluates with them bound.
///
/// Not an expression yet. Nothing in the R5 object language evaluates a
/// @c core_lambda — @ref core_defun reads this node without evaluating it,
/// which is why there is no function object among the evaluator's values.
/// The step that makes `lambda` an expression and `#'f` executable gives
/// this node an evaluation rule and adds that value alternative; it does
/// not change this node.
struct core_lambda {
    foundation::static_vector<symbol::symbol_id, max_lambda_params>
        params{}; ///< The parameter names, left to right.

    // HIDDEN FRIEND
    friend constexpr auto operator==(core_lambda const &, core_lambda const &)
        -> bool = default;
};

/// Installs its one child — a @ref core_lambda — in a symbol's function
/// slot, and evaluates to that symbol.
///
/// This is the node that closes DIV-0009. The recursion works because the
/// name is resolved in the function slot at *call* time rather than
/// captured at definition time, so a body that calls its own name finds the
/// definition that is being installed. Decision D18 asks for a lowering
/// before a tag: there is none to be had, because "put this function in
/// that symbol's function slot" is not something any other core form says.
struct core_defun {
    symbol::symbol_id name{}; ///< The symbol whose function slot to set.

    // HIDDEN FRIEND
    friend constexpr auto operator==(core_defun, core_defun) -> bool = default;
};

/// A named exit point; children are the body forms, evaluated for the last
/// one's value.
///
/// A block is where the question "is this unwind aimed at me?" is asked,
/// and it is asked against this node's own name (decision D13).
struct core_block {
    symbol::symbol_id name{}; ///< The block's name.

    // HIDDEN FRIEND
    friend constexpr auto operator==(core_block, core_block) -> bool = default;
};

/// Puts an unwind in flight toward the named block, carrying its one
/// child's value, or `nil` if it has no child.
struct core_return_from {
    symbol::symbol_id name{}; ///< The block to return from.

    // HIDDEN FRIEND
    friend constexpr auto operator==(core_return_from, core_return_from)
        -> bool = default;
};

/// Evaluates its first child, then its remaining children as cleanup —
/// whichever way the first child completed.
///
/// The one form whose meaning is stated in terms of all three channels at
/// once, which is why it arrives in the phase that split them.
struct core_unwind_protect {
    // HIDDEN FRIEND
    friend constexpr auto operator==(core_unwind_protect, core_unwind_protect)
        -> bool = default;
};

/// A core-tree branch tag: what a compound expression is. The elaborator
/// grows this variant as it lowers more special operators; decision D18
/// keeps that growth slow — prefer lowering onto existing forms over new
/// tags.
using core_tag =
    std::variant<core_call, core_if, core_progn, core_cons, core_lambda,
                 core_defun, core_block, core_return_from, core_unwind_protect>;
```

D18 wants a new operator lowered onto forms the core already has before it gets a node kind of its own, and phase 27 spent a section on the one exception the decision states for itself. Five more went past the bar this step, each with its own reason. `core_defun`, because "put this function in that symbol's function slot" is not a thing any other form says. `core_lambda` separately from `core_defun`, because keeping parameter binding on its own node is what lets `let` and `multiple-value-bind` lower to a lambda application later, which is the pivot's proven strategy and D18's own stated mitigation. `core_block` and `core_return_from`, because without them the third channel has no producer, and a channel nothing can put anything into is untestable. `core_unwind_protect`, because it's the one form whose meaning is stated in terms of all three channels at once, which is why it belongs in the phase that split them.

Nine tags now, where phase 27 had four. That's a faster rate of growth than D18 wants sustained, and the next step to add one should have to say why in the same terms.


# defun is three nodes

```cpp
/// Emits `(defun name (parameter...) form...)`.
///
/// Three nodes, and the nesting is the whole of what `defun` means: the body
/// goes inside a @ref core::core_block named for the function, which is the
/// implicit block ANSI 3.1.2.1.2.2 requires and which is what makes
/// `return-from` usable in a function body without a `block` of its own; the
/// block goes inside a @ref core::core_lambda, which binds the parameters;
/// and the lambda goes inside a @ref core::core_defun, which puts it in the
/// symbol's function slot.
///
/// Nothing here captures the function under its own name, and that is the
/// point. The recursive call in the body is an ordinary
/// @ref core::core_call, which the machine resolves through the function
/// slot at the moment of the call — so a body that names itself finds the
/// definition being installed. That is DIV-0009 closed.
template <class Ctx>
[[nodiscard]] constexpr auto
emit_definition(Ctx &ctx, typename Ctx::source_children const &source_children,
                typename Ctx::source_children const &children)
    -> foundation::result<int> {
    if (children.size() < 3) {
        return foundation::parse_error{
            {}, "defun takes a name, a lambda list, and a body"};
    }
    auto const name = name_at(ctx, source_children[1]);
    if (!name.valid()) {
        return foundation::parse_error{{}, "a function name must be a symbol"};
    }
    return foundation::and_then(
        read_lambda_list(ctx, source_children[2]),
        [&ctx, &children, name](parameter_list const &params) {
            return foundation::and_then(
                emit_named_block(ctx, children, 3, name),
                [&ctx, &params, name](int body) {
                    return foundation::and_then(
                        emit_wrapper(ctx,
                                     core::core_tag{core::core_lambda{params}},
                                     body),
                        [&ctx, name](int function) {
                            return emit_wrapper(
                                ctx, core::core_tag{core::core_defun{name}},
                                function);
                        });
                });
        });
}
```

A `defun` over a `lambda` over a `block`, and the nesting is all of what `defun` means. The block is the implicit one ANSI 3.1.2.1.2.2 requires (Steele, Guy L., 1990), named for the function, which is what makes `return-from` usable in a body that never wrote a `block` of its own. The lambda binds the parameters. The defun puts the lambda in the slot. Each of the three says one thing, and the reason to want that is the next section.

The role pass from phase 27 needed one addition to get here. A form now says how many of the children after its head are **names** and not subforms (two for `defun`, one for `block` and `return-from`), and a name is simply left with the role every node is born with, `unreachable`, so no later pass mistakes it for an expression. The emission pass reads it straight out of the source tree. Nothing about that needs a stack, and the pass is still one descending sweep of the node array.


# The recursion works

```lisp
(defun len (l) (if (null l) 0 (+ 1 (len (cdr l)))))
(len '(a b c))  ; => 3
```

Phase 25 opened with that program not working. Call `len` in the pivot tree and you get `undefined function`, because the closure captures a copy of the environment made before `defun` binds the function's own name into it, so the self-call looks up a name that copy never had. It's DIV-0009, classified a defect, and it's the divergence D12 was recorded to close.

```cpp
template <limits Limits, class CoreTree, class SymbolTable>
constexpr auto machine<Limits, CoreTree, SymbolTable>::call_function(
    symbol::symbol_id callee, arg_list const &args) -> void {
    auto const &slot = symbols_.function(callee);
    if (!slot) {
        // DIV-0009's failure mode, and where it is now impossible: this
        // lookup happens per call, so a function being defined is already
        // in its own slot by the time its body runs.
        complete(outcome{foundation::parse_error{{}, "undefined function"}});
        return;
    }
    if (auto const *op = std::get_if<builtin_op>(&*slot)) {
        complete(apply_builtin(*op, args, store_, known_));
        return;
    }
    enter_closure(std::get<closure_ref>(*slot), args);
}

template <limits Limits, class CoreTree, class SymbolTable>
constexpr auto machine<Limits, CoreTree, SymbolTable>::enter_closure(
    closure_ref ref, arg_list const &args) -> void {
    closure const entered = store_.closure_at(ref);
    auto const &definition = program_.branch(entered.lambda_node);
    auto const *lambda = std::get_if<core::core_lambda>(&definition.tag);
    if (lambda == nullptr || definition.children.empty()) {
        complete(outcome{
            foundation::parse_error{{}, "malformed function definition"}});
        return;
    }
    if (lambda->params.size() != args.size()) {
        complete(
            outcome{foundation::parse_error{{}, "wrong number of arguments"}});
        return;
    }
    auto const bound = bind_parameters(lambda->params, args, entered.env);
    if (!bound.has_value()) {
        complete(outcome{bound.error()});
        return;
    }
    // No frame: the body's value is the call's value, so whatever frame is
    // already underneath continues with its own environment. Entering a
    // function therefore costs no continuation frame of its own. It is not
    // yet a tail call in the usual sense, because the implicit `block` a
    // `defun` body sits in does push one and cannot pop it until the body
    // finishes — a self-call in tail position costs one frame per call
    // rather than none.
    go_to(definition.children[0], bound.value());
}
```

The whole fix is that the lookup is in the first two lines of `call_function`, at the moment of the call. A closure holds the index of the node its body lives at and the environment it captured, and it doesn't hold its own name, so there's no copy to be stale. A body that names itself resolves through the function slot, and by the time the body runs, the slot has the definition in it. That's what an interned symbol with slots gives you, and seven divergence records were separately complaining about its absence.

Two consequences fall out of the same line. A builtin isn't a special case in the call path: `(car x)` is an ordinary `core_call` whose callee happens to have a `builtin_op` in its slot where a closure would go, so `(defun car ...)` overwrites `car` the way it would overwrite anything else, and there's a test pinning that. And entering a function costs no continuation frame, because the body's value is the call's value and whatever frame is underneath continues with its own environment.

That second one is less good than it sounds, and the comment in the code says so. The implicit block **does** push a frame, and it can't pop it until the body finishes, so a self-call in tail position costs one frame per call rather than none. Omitting the block when a body contains no `return-from` is a real optimization, it's obvious, and it isn't done. `len` over a three-element list is nowhere near `limits::frames`, so nothing forced the issue.

The acceptance witness is the part to be careful about. It isn't that the demo runs; it's a `static_assert` in `machine.test.cpp`, one of twenty-six. Reading, elaborating, allocating cons cells, building an environment, entering a closure three times and doing the arithmetic all happen inside the constant evaluator, and the compiler refuses to produce an object file if the answer isn't `3`. The revisit condition DIV-0009 wrote for itself asked for a runtime demonstration, and this is stronger, so the record gets a dated note saying it's closed for this tree. The pivot tree keeps the defect. It's the oracle, it's never edited, and where the two trees disagree about recursion the old one is a wrong answer on purpose (DIV-0009's resolution note; D16 is why no test may pin it).


# One arena, several forms

Two top-level forms, run in order, turn out to need something the elaborator didn't offer.

```cpp
template <int MaxNodes, int MaxChildren, int DatumNodes, int DatumList,
          class SymbolTable>
[[nodiscard]] constexpr auto
elaborate_into(reader::datum_tree<DatumNodes, DatumList> const &form,
               SymbolTable &symbols,
               core::core_tree<MaxNodes, MaxChildren> &out)
    -> foundation::result<int> {
    static_assert(MaxChildren >= 3,
                  "a core node must hold an if's three arms and a pair's two "
                  "halves");
    using context = detail::emit_context<MaxNodes, MaxChildren, DatumNodes,
                                         DatumList, SymbolTable>;
    return foundation::and_then(
        detail::lower_atoms(form, symbols),
        [&symbols,
         &out](detail::lowered_tree<DatumNodes, DatumList> const &lowered)
            -> foundation::result<int> {
            if (lowered.root() < 0) {
                return foundation::parse_error{{}, "form has no root"};
            }
            auto const plans =
                detail::plan_nodes(lowered, detail::resolve_operators(symbols));
            context ctx{lowered, plans, symbols, out};
            // The paramorphism: bottom-up over the source, each node's
            // carrier its emitted core index, each layer arriving with
            // its child slots already emitted. Its return value is the
            // root's index, which is the one fact the caller needs.
            // para_short's value is the root's carrier: the emitted
            // form's root index, which is the one fact the caller needs.
            return foundation::para_short<int>(
                lowered,
                [&ctx](typename context::source_tree const &tree, int index,
                       typename context::layer const &layer) {
                    return detail::emit_node(ctx, tree, index, layer);
                });
        });
}

/// Elaborates one datum @p form into the core tree it means, interning into
/// @p symbols the quote-family operators that quoted data needs and the
/// reader never spelled.
///
/// Recognizes `quote` (spelled either way), `if`, `progn`, `defun`, `block`,
/// `return-from` and `unwind-protect`; every other compound form in an
/// expression position is a call, with the callee resolved in the function
/// namespace (Lisp-2). Self-evaluating atoms are fixnums, characters,
/// strings, keywords, `NIL` and `T`; a numeric-tower literal is readable but
/// not yet executable (decision D19), as are vectors, `#'f` and the
/// backquote family.
///
/// @tparam MaxNodes    Core-tree node capacity.
/// @tparam MaxChildren Maximum subexpressions per core node.
/// @tparam DatumNodes  The form's node capacity (deduced).
/// @tparam DatumList   The form's per-list capacity (deduced).
/// @tparam SymbolTable The symbol table instantiation.
/// @param  form    The datum to lower; must have a root.
/// @param  symbols The table @p form's names were interned into.
/// @return The core tree, or the leftmost failure.
template <int MaxNodes = default_max_nodes,
          int MaxChildren = default_max_children, int DatumNodes, int DatumList,
          class SymbolTable>
[[nodiscard]] constexpr auto
elaborate(reader::datum_tree<DatumNodes, DatumList> const &form,
          SymbolTable &symbols)
    -> foundation::result<core::core_tree<MaxNodes, MaxChildren>> {
    using out_tree = core::core_tree<MaxNodes, MaxChildren>;
    out_tree out;
    return foundation::and_then(
        elaborate_into(form, symbols, out),
        [&out](int root) -> foundation::result<out_tree> {
            out.set_root(root);
            return out;
        });
}
```

That's the same anchor phase 27 transcluded, and it's grown a function above the one that post was about. `elaborate` makes a tree, fills it, and sets its root; `elaborate_into` emits into a tree that may already hold earlier forms and hands back the root instead of setting it. The reason is the closure: it names its body by node index, so a function defined by the first form and called by the second needs both forms' nodes in one arena that outlives both. Elaborate each form into its own tree and the index in the closure means nothing.

Reading a **sequence** of top-level forms still isn't in the library. `read_datum` returns the cursor after the datum it read, so the unfold is available; nobody has written it. Driving a program today is four calls chained through `and_then` (install the builtins, read, elaborate, run), and `install_builtins` has to go first, because it interns `NIL` and `T`, which the elaborator asks the table about when deciding whether a bare `nil` is a constant or a variable. That chain exists twice in `machine.test.cpp` and wants to be a component before it exists a third time. R6 is a conformance corpus, so R6 is where it gets written.


# Under all of it, an object layer

None of the above is about anything without values to be about.

```cpp
/// A handle to a cons cell in a @ref heap. The default value is the
/// no-cell handle, which @ref valid rejects.
struct cons_ref {
    int index = -1; ///< Cell index, or -1 for no cell.

    /// Returns true if this handle names a cell.
    [[nodiscard]] constexpr auto valid() const -> bool { return index >= 0; }

    // HIDDEN FRIEND
    friend constexpr auto operator==(cons_ref, cons_ref) -> bool = default;
};

/// A handle to a run of characters in a @ref heap's string pool.
struct string_ref {
    int offset = 0; ///< First character's index in the pool.
    int length = 0; ///< Number of characters.

    // HIDDEN FRIEND
    friend constexpr auto operator==(string_ref, string_ref) -> bool = default;
};

/// A handle to a closure in a @ref heap.
struct closure_ref {
    int index = -1; ///< Closure index, or -1 for no closure.

    /// Returns true if this handle names a closure.
    [[nodiscard]] constexpr auto valid() const -> bool { return index >= 0; }

    // HIDDEN FRIEND
    friend constexpr auto operator==(closure_ref, closure_ref)
        -> bool = default;
};

/// A fixnum, the one numeric-tower member that is executable today.
struct value_fixnum {
    int value = 0; ///< The integer.

    // HIDDEN FRIEND
    friend constexpr auto operator==(value_fixnum, value_fixnum)
        -> bool = default;
};

/// A character object.
struct value_character {
    char value = 0; ///< The character.

    // HIDDEN FRIEND
    friend constexpr auto operator==(value_character, value_character)
        -> bool = default;
};

/// A symbol object, by interned id (decision D12).
///
/// `NIL` and `T` are ordinary symbols here, as ANSI says they are, not
/// alternatives of their own: `nil` is the symbol `NIL`, which is why it is
/// simultaneously the false value, the empty list, and something
/// `symbolp` answers true for. Which id that is belongs to the symbol
/// table, so a value alone cannot tell you it is `NIL` — the machine
/// resolves the name once and compares ids after.
struct value_symbol {
    symbol::symbol_id id{}; ///< The symbol.

    // HIDDEN FRIEND
    friend constexpr auto operator==(value_symbol, value_symbol)
        -> bool = default;
};

/// A string object, by handle into the heap's character pool.
struct value_string {
    string_ref ref{}; ///< The characters.

    // HIDDEN FRIEND
    friend constexpr auto operator==(value_string, value_string)
        -> bool = default;
};

/// A cons cell, by handle into the heap.
struct value_cons {
    cons_ref ref{}; ///< The cell.

    // HIDDEN FRIEND
    friend constexpr auto operator==(value_cons, value_cons) -> bool = default;
};

/// One Common Lisp object.
///
/// There is deliberately no function alternative yet. Nothing in the R5
/// object language produces one: `defun` names a function rather than
/// returning it (ANSI says it returns the name), a call resolves its callee
/// in the function namespace, and `#'f`, `lambda` as an expression and
/// `funcall` are all still diagnosed as not yet executable. Adding the
/// alternative before there is a form that yields one would be a channel
/// with no producer.
using value = std::variant<value_fixnum, value_character, value_symbol,
                           value_string, value_cons>;
```

A value is an immediate &#x2014; a fixnum, a character, a symbol id &#x2014; or a capacity-free handle into a heap of cons cells, string characters and closures. No runtime value type is parameterized on a capacity, which is D14, and the capacities all sit on the heap where storage belongs. An environment is not a data structure of its own: it's a chain of `(symbol . value)` pairs in that same heap, so it's an ordinary Lisp value, `lookup` is `find_if` over a view of the chain, and `extend` is two conses. The evaluator never walks a chain by hand; `heap::list_view` is a forward range and the loop is the iterator's increment.

Eighteen builtins go into function slots before anything runs: `car`, `cdr`, `cons`, `list`, `null`, `not`, `eq`, `eql`, `atom`, `consp`, `symbolp`, `length`, and the six arithmetic and comparison operators. `apply_builtin` returns an `outcome` rather than a value, because a builtin can fail, and its third channel is always empty, because nothing in that list is a control-transfer operator. A channel with no producer is not a channel that function has to think about.

`eq` and `eql` still coincide, and the reason has changed under them. DIV-0002 had them coincide because both were string comparison on a name. Here identity is real (an id, a cell index, a pool offset), and they agree only because every number that exists is an immediate, so there's nothing yet for which `eql` is the weaker test. Boxed numbers are what separates them, and boxed numbers are D19's later lanes.


# The third copy of intern\_checked

```cpp
/// Interns @p name in @p symbols, surfacing a full table or a full name pool
/// as a @ref foundation::parse_error at @p where rather than tripping
/// @ref symbol_table's capacity asserts. Interning a name the table already
/// holds needs no capacity and always succeeds.
///
/// Promoted here in step R5. The reader and the elaborator had each grown a
/// private copy and the evaluator wanted a third, which is the same signal
/// that promoted `foundation/` in R1. It belongs beside the table because
/// only the table knows what "full" means, and it is a free function rather
/// than a member because the checked form is a policy — diagnose, do not
/// assert — and the unchecked member is still the right call once a caller
/// has established there is room.
///
/// @tparam SymbolTable A table offering @c find, @c intern and the capacity
///                     observers of @ref symbol_table.
template <class SymbolTable>
[[nodiscard]] constexpr auto intern_checked(SymbolTable &symbols,
                                            std::string_view name,
                                            foundation::source_pos where = {})
    -> foundation::result<symbol_id> {
    if (auto const existing = symbols.find(name)) {
        return *existing;
    }
    if (symbols.size() >= symbols.capacity()) {
        return foundation::parse_error{where, "symbol table full"};
    }
    if (symbols.name_chars_used() + static_cast<int>(name.size()) >
        symbols.name_chars_capacity()) {
        return foundation::parse_error{where, "symbol name storage full"};
    }
    return symbols.intern(name);
}
```

Phase 27 left `intern_checked` written twice, in `reader::detail` and `elaborator::detail`, and said R5 would decide because R5 would want a third copy. It did want one, for `defun`'s function slot, so all three collapsed into a free function beside the table. Beside, because only the table knows what "full" means, and free instead of a member because the checked form is a policy (diagnose, don't assert) while the unchecked member stays right for a caller that already established there's room.

The copies could not have survived the promotion anyway, which I didn't see until the new definition was there. A symbol table's own type puts `smd::cl::symbol` among the associated namespaces of every call that passes one, so the moment `intern_checked` lived beside the table, ADL found it from inside `reader::detail` and `elaborator::detail` as well, and every unqualified call there was ambiguous with the local copy. R4 hit the identical collision promoting `and_then`. Twice is a coincidence; I'd like to notice it before the third one.


# What it doesn't do

`lambda` is not an expression. Nothing in this object language produces a function object at all: `defun` names one, a call resolves one through a slot, and `#'f`, `funcall`, `apply` and `lambda` in expression position are all diagnosed. There's no function alternative in `value` either, and there shouldn't be until something can make one. The current diagnostic for `(lambda (x) x)` is `undefined function`, because `LAMBDA` falls through to an ordinary call and the elaborator never gets a chance to say something better. That's a poor message, and fixing it means the elaborator recognizing `lambda` well enough to reject it properly, which nothing this step needed.

`(block nil ...)` is rejected. The atom pass turns `NIL` and `T` into constants before the role pass runs, so neither is available as a block name. And ANSI uses `(block nil ...)` for `loop`'s implicit block, so this needs an answer before any iteration macro lands. It needs classifying first, because D16 is clear that a test may pin a scope decision and must never pin a defect, and I don't yet know which one this is. There's nothing behind `&optional` or `&rest` either: a lambda list keyword is diagnosed where it stands.

Diagnostics still carry no source position, and it's worse than it was in phase 27. The datum tree has no positions on its nodes, so elaborator errors got a default one; now every **evaluator** error inherits the same hole, and `unbound variable` and `wrong number of arguments` arrive pointing at nothing. A conformance corpus is where that starts to hurt first, and R6 is a conformance corpus. Fixing it is reader and foundation work, not evaluator work, which is why it keeps not happening.

And there's still no SBCL in this environment, so R3's reader syntax, R4's elaboration and now R5's evaluation semantics are all read off the standard and pinned by tests I wrote myself. That remains the weakest evidence in the project. Everything in this post is a claim about a machine I built agreeing with a specification I read.


# The merge criterion

`(defun len (l) (if (null l) 0 (+ 1 (len (cdr l)))))`, then `(len '(a b c))`, constant-evaluating to `3`. That was written into the brief before the step started, and it's the only line in it that couldn't be satisfied by a partial job: an evaluator that can do everything except call a function by its own name is an evaluator that hasn't closed the divergence the rebuild was argued for.

What I can't tell yet is how much of the rest is right. Twenty-six `static_assert` cases are twenty-six programs I thought of, and D16's whole point is that a conformance corpus is a different kind of evidence from a test suite the implementer wrote. Next step builds the corpus, and every entry in it has to state its expected outcome **by channel** (value, error, or unwind), since collapsing them there would hand back what this phase spent itself separating.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 27 - The Elaborator Never Recurses](phase-27-elaboration.md)

</nav>


# References

Danvy, Olivier and Nielsen, Lasse R. (2001). *Defunctionalization at Work*.

Meijer, Erik and Fokkinga, Maarten and Paterson, Ross (1991). *Functional Programming with Bananas, Lenses, Envelopes and Barbed Wire*.

Reynolds, John C. (1972). *Definitional interpreters for higher-order programming languages*.

Steele, Guy L. (1990). *Common Lisp the Language*, Digital Press.
