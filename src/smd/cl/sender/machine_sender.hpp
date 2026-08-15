// src/smd/cl/sender/machine_sender.hpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_SENDER_MACHINE_SENDER_HPP
#define SRC_SMD_CL_SENDER_MACHINE_SENDER_HPP

#include <smd/cl/eval/machine.hpp>
#include <smd/cl/eval/outcome.hpp>
#include <smd/cl/eval/value.hpp>
#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/sender/sender_v.hpp>

#include <utility>

/// The sender backend of decision D17: `eval::machine` exposed as a Beman
/// Execution sender, alongside R5's evaluator rather than in place of it.
///
/// **The shape, and what it is not.** Two shapes were on the table (step
/// brief R7). One forks `machine::evaluate_subforms`'s frame so a call's
/// arguments run as a `when_all` over sibling senders — the pivot's
/// `when_all` trick, minus the pointer tree. The other drives the machine
/// whole: connect a sender, and `start` runs the machine's own trampoline to
/// completion before completing the sender. This file is the second one.
///
/// The forking shape was rejected on a concrete cost, not a preference.
/// `eval::machine` is single-owner state: one frame stack (`konts_`), one
/// heap (`store_`), one control register (`control_`/`env_`/`pending_`),
/// mutated in place by `step`. Forking an argument list means either
/// splitting that state across concurrently-started senders — which nothing
/// in `machine`'s design supports, since every frame assumes it is the only
/// thing advancing the shared stack — or recursing through a *second*
/// evaluator built from scratch for the purpose, at which point the "no
/// evaluation recursion is C++ recursion" property `machine.hpp` was built to
/// establish (`limits::frames`, `limits::steps` as diagnosed capacities
/// rather than the C++ stack) would have been rebuilt on the sender side and
/// then immediately spent: `Limits.steps` defaults to 200000, and chaining
/// that many sender continuations recursively is 200000 C++ activation
/// records, the exact failure mode `machine.hpp`'s own docs argue against.
/// Driving the whole machine keeps its trampoline as the one loop that
/// empties the step budget, and asks the sender to do only what a sender is
/// for here: make "evaluate this program" a first-class, composable
/// asynchronous value.
///
/// See `docs/divergences/DIV-0016-mendler-para-rederived-for-foundation-fix.md`
/// (2026-08-15 note) for the fuller argument, including why this is not
/// underdetermined between the two shapes.
///
/// **What this buys.** Two whole-program evaluations are genuinely
/// independent senders — separate machines, separate heaps, no shared
/// mutable state — so `when_all` over *them* is honest in the sense R7's
/// brief asks for: a claim about visible graph structure between programs,
/// never a claim about evaluation order inside one. `machine_sender.test.cpp`
/// demonstrates exactly that and nothing stronger.
///
/// **What this does not buy.** DIV-0015 is accepted-permanent: Beman
/// Execution's `connect`/`start`/`sync_wait` are not constant-evaluable, so
/// nothing here is `constexpr`, unlike `eval::machine` itself. That absence
/// is stated in the API, per that divergence's own recommendation, rather
/// than discovered only in the tests.
namespace smd::cl::sender {

// 0c10d654-ec97-4580-9c14-aed654701bbf
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
// 0c10d654-ec97-4580-9c14-aed654701bbf end

// 4087241f-a4fb-4e3f-bc87-59f3973fc026
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
// 4087241f-a4fb-4e3f-bc87-59f3973fc026 end

} // namespace smd::cl::sender

#endif
