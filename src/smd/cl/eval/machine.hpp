// src/smd/cl/eval/machine.hpp                                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_EVAL_MACHINE_HPP
#define SRC_SMD_CL_EVAL_MACHINE_HPP

#include <smd/cl/core/ast.hpp>
#include <smd/cl/eval/builtin.hpp>
#include <smd/cl/eval/environment.hpp>
#include <smd/cl/eval/heap.hpp>
#include <smd/cl/eval/outcome.hpp>
#include <smd/cl/eval/value.hpp>
#include <smd/cl/foundation/fold_left_short.hpp>
#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/foundation/static_vector.hpp>
#include <smd/cl/foundation/trampoline.hpp>
#include <smd/cl/symbol/symbol_id.hpp>

#include <ranges>
#include <utility>
#include <variant>

namespace smd::cl::eval {

/// Everything one machine is allowed to consume.
///
/// A single structural aggregate rather than six template parameters, so
/// that an instantiation reads as a configuration and not as an argument
/// list. Every field is storage or budget; none of them reaches a value
/// (decision D14).
struct limits {
    int cells = 1024;       ///< Cons cells, environments included.
    int string_chars = 512; ///< Characters across all string objects.
    int closures = 32;      ///< Closures.
    int frames = 256;       ///< Continuation frames: the recursion depth.
    int call_args = 16;     ///< Arguments to any one call.
    int steps = 200000;     ///< Machine steps before the budget is spent.

    // HIDDEN FRIEND
    friend constexpr auto operator==(limits, limits) -> bool = default;
};

namespace detail {

/// Waiting for a test, to choose an arm of `if`.
struct k_if {
    int node = -1; ///< The `core_if` node.
    value env{};   ///< The environment its arms are evaluated in.
};

/// Waiting for one body form, to evaluate the next; the value of the last
/// one is the value of the sequence.
struct k_sequence {
    int node = -1; ///< The node whose children are the body.
    value env{};   ///< The environment the body is evaluated in.
    int next = 0;  ///< Index of the child to evaluate next.
};

/// Waiting for one subform, to evaluate the next and then combine them —
/// the arguments of a call, or the two halves of a hermetic pair.
///
/// @tparam MaxArgs Maximum subforms this frame can collect.
template <int MaxArgs>
struct k_arguments {
    int node = -1; ///< The node being combined.
    value env{};   ///< The environment its subforms are evaluated in.
    int next = 0;  ///< Index of the subform to evaluate next.
    foundation::static_vector<value, MaxArgs>
        collected{}; ///< Subform values so far, left to right.
};

/// Waiting for a block's body, ready to catch an unwind aimed at this name.
struct k_block {
    symbol::symbol_id name{}; ///< The name this exit point answers to.
};

/// Waiting for a `return-from` result, to put it in flight.
struct k_return_from {
    symbol::symbol_id name{}; ///< The block being returned from.
};

// 2e6f8b3a-51d7-4a0c-9f6e-7c2b1d0a4e58
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
// 2e6f8b3a-51d7-4a0c-9f6e-7c2b1d0a4e58 end

} // namespace detail

// 0b7a5e42-9d3c-4b19-83f4-6e5c2a1b7d04
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
// 0b7a5e42-9d3c-4b19-83f4-6e5c2a1b7d04 end

/// Dispatches the alternatives of one leaf. A visitor over a single node's
/// alternatives is the use of @c std::visit the rules permit: it does not
/// drive the recursion, which is the frame stack's job.
template <limits Limits, class CoreTree, class SymbolTable>
struct machine<Limits, CoreTree, SymbolTable>::leaf_visitor {
    machine *self; ///< The machine whose environment and heap to use.

    constexpr auto operator()(core::core_fixnum leaf) const -> outcome {
        return outcome{value{value_fixnum{leaf.value}}};
    }

    constexpr auto operator()(core::core_character leaf) const -> outcome {
        return outcome{value{value_character{leaf.value}}};
    }

    constexpr auto operator()(core::core_keyword leaf) const -> outcome {
        return outcome{symbol_value(leaf.id)};
    }

    constexpr auto operator()(core::core_quoted_symbol leaf) const -> outcome {
        return outcome{symbol_value(leaf.id)};
    }

    constexpr auto operator()(core::core_nil) const -> outcome {
        return outcome{nil_value(self->known_)};
    }

    constexpr auto operator()(core::core_t) const -> outcome {
        return outcome{symbol_value(self->known_.t)};
    }

    constexpr auto operator()(core::core_string const &leaf) const -> outcome {
        return to_outcome(self->store_.make_string(leaf.view()));
    }

    constexpr auto operator()(core::core_variable leaf) const -> outcome {
        auto const bound =
            environment::lookup(self->store_, self->env_, leaf.id);
        if (!bound) {
            return foundation::parse_error{{}, "unbound variable"};
        }
        return outcome{*bound};
    }
};

/// Dispatches the alternatives of one branch tag: what a compound form
/// does when it is reached in control position.
template <limits Limits, class CoreTree, class SymbolTable>
struct machine<Limits, CoreTree, SymbolTable>::branch_visitor {
    machine *self;             ///< The machine to advance.
    int node;                  ///< The node being evaluated.
    branch_type const &branch; ///< Its tag and children.

    constexpr auto operator()(core::core_call) const -> void {
        self->evaluate_subforms(node, branch);
    }

    constexpr auto operator()(core::core_cons) const -> void {
        self->evaluate_subforms(node, branch);
    }

    constexpr auto operator()(core::core_if) const -> void {
        if (branch.children.size() < 2) {
            self->malformed();
            return;
        }
        if (self->push(frame{detail::k_if{node, self->env_}})) {
            self->go_to(branch.children[0], self->env_);
        }
    }

    constexpr auto operator()(core::core_progn) const -> void {
        self->evaluate_body(node, branch, self->env_);
    }

    constexpr auto operator()(core::core_block tag) const -> void {
        if (self->push(frame{detail::k_block{tag.name}})) {
            self->evaluate_body(node, branch, self->env_);
        }
    }

    constexpr auto operator()(core::core_return_from tag) const -> void {
        if (branch.children.empty()) {
            self->complete(outcome{unwind{tag.name, nil_value(self->known_)}});
            return;
        }
        if (self->push(frame{detail::k_return_from{tag.name}})) {
            self->go_to(branch.children[0], self->env_);
        }
    }

    constexpr auto operator()(core::core_unwind_protect) const -> void {
        if (branch.children.empty()) {
            self->malformed();
            return;
        }
        if (self->push(frame{
                detail::k_protect{node, self->env_, 0, outcome{value{}}}})) {
            self->go_to(branch.children[0], self->env_);
        }
    }

    constexpr auto operator()(core::core_defun tag) const -> void {
        if (branch.children.empty()) {
            self->malformed();
            return;
        }
        auto const made =
            self->store_.make_closure(branch.children[0], self->env_);
        if (!made.has_value()) {
            self->complete(outcome{made.error()});
            return;
        }
        self->symbols_.set_function(tag.name, function_binding{made.value()});
        self->complete(outcome{symbol_value(tag.name)});
    }

    constexpr auto operator()(core::core_lambda const &) const -> void {
        self->complete(outcome{foundation::parse_error{
            {}, "a function definition is not yet an expression"}});
    }
};

/// Dispatches the alternatives of one branch tag again, this time for a
/// node whose subforms have all been evaluated.
template <limits Limits, class CoreTree, class SymbolTable>
struct machine<Limits, CoreTree, SymbolTable>::combine_visitor {
    machine *self;        ///< The machine to advance.
    arg_list const &args; ///< The subform values, left to right.

    constexpr auto operator()(core::core_call tag) const -> void {
        self->call_function(tag.callee, args);
    }

    constexpr auto operator()(core::core_cons) const -> void {
        if (args.size() != 2) {
            self->malformed();
            return;
        }
        self->complete(to_outcome(self->store_.cons(args[0], args[1])));
    }

    constexpr auto operator()(core::core_if) const -> void { unreachable(); }
    constexpr auto operator()(core::core_progn) const -> void { unreachable(); }
    constexpr auto operator()(core::core_block) const -> void { unreachable(); }
    constexpr auto operator()(core::core_lambda const &) const -> void {
        unreachable();
    }
    constexpr auto operator()(core::core_defun) const -> void { unreachable(); }
    constexpr auto operator()(core::core_return_from) const -> void {
        unreachable();
    }
    constexpr auto operator()(core::core_unwind_protect) const -> void {
        unreachable();
    }

  private:
    /// Only @c core_call and @c core_cons evaluate all their subforms first;
    /// every other tag is driven by a frame of its own and never reaches
    /// here. Diagnosed rather than asserted, so a future tag that forgets to
    /// say which it is fails loudly and survivably.
    constexpr auto unreachable() const -> void {
        self->complete(outcome{foundation::parse_error{
            {}, "this form does not combine evaluated subforms"}});
    }
};

/// Hands the pending outcome to the innermost frame, which decides what to
/// do with each of the three channels.
template <limits Limits, class CoreTree, class SymbolTable>
struct machine<Limits, CoreTree, SymbolTable>::resume_visitor {
    machine *self; ///< The machine to advance.

    constexpr auto operator()(detail::k_if const &f) const -> void {
        auto const &children = self->program_.branch(f.node).children;
        value const env = f.env;
        bool const took_else =
            self->pending_.is_value() &&
            is_false(self->pending_.as_value(), self->known_);
        bool const has_value = self->pending_.is_value();
        int const arm = !has_value            ? -1
                        : !took_else          ? children[1]
                        : children.size() > 2 ? children[2]
                                              : -1;
        self->konts_.pop_back();
        if (!has_value) {
            return; // an error or an unwind passes through untouched
        }
        if (arm < 0) {
            self->complete(outcome{nil_value(self->known_)});
            return;
        }
        self->go_to(arm, env);
    }

    constexpr auto operator()(detail::k_sequence &f) const -> void {
        auto const &children = self->program_.branch(f.node).children;
        if (!self->pending_.is_value() || f.next >= children.size()) {
            self->konts_.pop_back();
            return;
        }
        int const next = f.next;
        ++f.next;
        self->go_to(children[next], f.env);
    }

    constexpr auto operator()(k_args &f) const -> void {
        if (!self->pending_.is_value()) {
            self->konts_.pop_back();
            return;
        }
        if (f.collected.size() >= f.collected.capacity()) {
            self->konts_.pop_back();
            self->complete(outcome{
                foundation::parse_error{{}, "too many arguments in one call"}});
            return;
        }
        f.collected.push_back(self->pending_.as_value());
        auto const &children = self->program_.branch(f.node).children;
        if (f.next < children.size()) {
            int const next = f.next;
            ++f.next;
            self->go_to(children[next], f.env);
            return;
        }
        int const node = f.node;
        arg_list const args = f.collected;
        self->konts_.pop_back();
        self->combine(node, args);
    }

    constexpr auto operator()(detail::k_block const &f) const -> void {
        symbol::symbol_id const name = f.name;
        self->konts_.pop_back();
        if (self->pending_.is_unwind() &&
            self->pending_.as_unwind().target == name) {
            // Aimed at this block: the unwind lands and becomes a value.
            self->complete(outcome{self->pending_.as_unwind().payload});
        }
    }

    constexpr auto operator()(detail::k_return_from const &f) const -> void {
        symbol::symbol_id const name = f.name;
        self->konts_.pop_back();
        if (!self->pending_.is_value()) {
            return;
        }
        self->complete(outcome{unwind{name, self->pending_.as_value()}});
    }

    constexpr auto operator()(detail::k_protect &f) const -> void {
        auto const &children = self->program_.branch(f.node).children;
        if (f.stage == 0) {
            f.saved = self->pending_;
        } else if (!self->pending_.is_value()) {
            // A transfer out of a cleanup form replaces what it was
            // cleaning up after; nothing is left to re-emit.
            self->konts_.pop_back();
            return;
        }
        int const next = f.stage + 1;
        if (next < children.size()) {
            f.stage = next;
            self->go_to(children[next], f.env);
            return;
        }
        outcome const saved = f.saved;
        self->konts_.pop_back();
        self->complete(saved);
    }
};

template <limits Limits, class CoreTree, class SymbolTable>
constexpr machine<Limits, CoreTree, SymbolTable>::machine(
    CoreTree const &program, SymbolTable &symbols, standard_symbols known)
    : program_{program}, symbols_{symbols}, known_{known},
      env_{nil_value(known)}, pending_{nil_value(known)} {}

template <limits Limits, class CoreTree, class SymbolTable>
constexpr auto machine<Limits, CoreTree, SymbolTable>::store() const
    -> heap_type const & {
    return store_;
}

template <limits Limits, class CoreTree, class SymbolTable>
constexpr auto machine<Limits, CoreTree, SymbolTable>::complete(outcome o)
    -> void {
    control_ = -1;
    pending_ = std::move(o);
}

template <limits Limits, class CoreTree, class SymbolTable>
constexpr auto machine<Limits, CoreTree, SymbolTable>::go_to(int node,
                                                             value env)
    -> void {
    control_ = node;
    env_ = std::move(env);
}

template <limits Limits, class CoreTree, class SymbolTable>
constexpr auto machine<Limits, CoreTree, SymbolTable>::push(frame f) -> bool {
    if (konts_.size() >= konts_.capacity()) {
        complete(
            outcome{foundation::parse_error{{}, "control stack overflow"}});
        return false;
    }
    konts_.push_back(std::move(f));
    return true;
}

template <limits Limits, class CoreTree, class SymbolTable>
constexpr auto machine<Limits, CoreTree, SymbolTable>::step()
    -> foundation::step_status {
    if (control_ >= 0) {
        evaluate();
        return foundation::step_status::running;
    }
    if (konts_.empty()) {
        return foundation::step_status::done;
    }
    resume();
    return foundation::step_status::running;
}

template <limits Limits, class CoreTree, class SymbolTable>
constexpr auto machine<Limits, CoreTree, SymbolTable>::evaluate() -> void {
    int const node = control_;
    control_ = -1;
    if (program_.is_leaf(node)) {
        pending_ = std::visit(leaf_visitor{this}, program_.leaf(node));
        return;
    }
    auto const &branch = program_.branch(node);
    std::visit(branch_visitor{this, node, branch}, branch.tag);
}

template <limits Limits, class CoreTree, class SymbolTable>
constexpr auto machine<Limits, CoreTree, SymbolTable>::resume() -> void {
    std::visit(resume_visitor{this}, konts_[konts_.size() - 1]);
}

template <limits Limits, class CoreTree, class SymbolTable>
constexpr auto machine<Limits, CoreTree, SymbolTable>::evaluate_subforms(
    int node, branch_type const &branch) -> void {
    if (branch.children.empty()) {
        combine(node, arg_list{});
        return;
    }
    if (push(frame{k_args{node, env_, 1, {}}})) {
        go_to(branch.children[0], env_);
    }
}

template <limits Limits, class CoreTree, class SymbolTable>
constexpr auto machine<Limits, CoreTree, SymbolTable>::evaluate_body(
    int node, branch_type const &branch, value env) -> void {
    if (branch.children.empty()) {
        complete(outcome{nil_value(known_)});
        return;
    }
    if (branch.children.size() > 1 &&
        !push(frame{detail::k_sequence{node, env, 1}})) {
        return;
    }
    go_to(branch.children[0], std::move(env));
}

template <limits Limits, class CoreTree, class SymbolTable>
constexpr auto machine<Limits, CoreTree, SymbolTable>::malformed() -> void {
    complete(outcome{foundation::parse_error{{}, "malformed core tree"}});
}

template <limits Limits, class CoreTree, class SymbolTable>
constexpr auto
machine<Limits, CoreTree, SymbolTable>::combine(int node, arg_list const &args)
    -> void {
    std::visit(combine_visitor{this, args}, program_.branch(node).tag);
}

// 7fb628ca-8a1d-4af4-ac88-3a05726e237f
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
// 7fb628ca-8a1d-4af4-ac88-3a05726e237f end

template <limits Limits, class CoreTree, class SymbolTable>
constexpr auto machine<Limits, CoreTree, SymbolTable>::bind_parameters(
    foundation::static_vector<symbol::symbol_id, core::max_lambda_params> const
        &params,
    arg_list const &args, value env) -> foundation::result<value> {
    return foundation::fold_left_short(
        std::views::zip(params, args), std::move(env),
        [this](value scope, auto const &binding) {
            auto const &[name, bound] = binding;
            return environment::extend(store_, scope, name, bound);
        });
}

template <limits Limits, class CoreTree, class SymbolTable>
constexpr auto machine<Limits, CoreTree, SymbolTable>::run(int root)
    -> outcome {
    control_ = root;
    env_ = nil_value(known_);
    pending_ = outcome{nil_value(known_)};
    auto const steps = foundation::trampoline(
        *this, Limits.steps, [](machine &m) { return m.step(); });
    // The trampoline's success value is a step count, which says only that
    // the machine halted; what the program produced is in pending_.
    // Substituting one for the other is a map over the effect, and widening
    // the surviving error channel into outcome's is to_outcome's job —
    // spelling that widening out here made a second copy of it.
    return to_outcome(
        foundation::fmap([this](int) { return pending_; }, steps));
}

template <limits Limits, class CoreTree, class SymbolTable>
constexpr auto machine<Limits, CoreTree, SymbolTable>::run() -> outcome {
    return run(program_.root());
}

} // namespace smd::cl::eval

#endif
