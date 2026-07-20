// src/smd/smdlisp/closure/env.hpp                                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_CLOSURE_ENV_HPP
#define SRC_SMD_SMDLISP_CLOSURE_ENV_HPP

#include <smd/smdlisp/closure/value.hpp>
#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/foundation/static_vector.hpp>

#include <utility>

namespace smd::smdlisp::closure {

/// A Lisp-2 lexical environment: two independent, linear,
/// most-recent-first binding lists, one per namespace (decision D4,
/// docs/cl-pivot-plan.md).
///
/// Adapted by copy from `smd::smdscheme::closure::env`'s architecture
/// (`foundation::static_vector`-backed, linear search from the newest
/// binding toward the oldest, so a later `define_value`/`define_function`
/// shadows an earlier one for the same name).  Two deltas from the Scheme
/// original, both intentional for this step:
///
///  - There are *two* binding lists instead of one.  `(f x)` resolves `f`
///    in the function namespace and `x` in the variable namespace, so the
///    same name can name a variable and a function at once without either
///    shadowing the other — that is the whole point of a Lisp-2.
///  - Every binding is a plain inline value (the Scheme original's
///    "functional" mode, with no backing `store`).  `setq` (step L12) is
///    what turns a variable binding mutable; until then there is nothing
///    to mutate, so no `store` indirection is needed yet.
///
/// Like the Scheme original, `env` has no parent-environment link: a
/// nested lexical scope is a *copy* of the enclosing `env` with additional
/// bindings appended (via `define_value`/`define_function`), not a chain
/// of environments searched outward.  That copy-and-extend approach is
/// exactly what makes an eventual owning capture (see `value.hpp`'s
/// `closure` documentation) reproduce lexical scoping correctly: copying
/// an `env` copies its two `static_vector`s by value.
///
/// @tparam Core        The core AST type (see @ref value).
/// @tparam MaxBindings Capacity of each of the two binding lists.
template <typename Core, int MaxBindings>
class env {
  public:
    constexpr env() = default;

    /// Constructs an environment sharing the pair heap @p p (step L11: the
    /// evaluator needs `cons`/`car`/`cdr`/`list` builtins and the hermetic
    /// `core_cons` construction node to reach a common heap).  Mirrors
    /// `smd::smdscheme::closure::env`'s `pairs_` member and @ref pairs
    /// accessor; @p p must outlive this environment and every environment
    /// copied from it (including closure captures).
    constexpr explicit env(pair_heap<Core, default_max_pairs> *p) : pairs_(p) {}

    /// Returns the shared pair heap (non-owning; null if this environment
    /// has none).  List primitives and @c core_cons construction allocate
    /// and dereference cells through it.
    [[nodiscard]] constexpr auto pairs() const
        -> pair_heap<Core, default_max_pairs> * {
        return pairs_;
    }

    /// Adds a new variable binding for @p name -> @p val.
    /// A later binding for the same @p name shadows this one.
    constexpr auto define_value(symbol name, value<Core> val) -> void;

    /// Adds a new function binding for @p name -> @p fn.
    /// A later binding for the same @p name shadows this one.  Entirely
    /// independent of @ref define_value: the same @p name may be bound in
    /// both namespaces simultaneously.
    constexpr auto define_function(symbol name, value<Core> fn) -> void;

    /// Looks up @p name in the variable namespace, most-recent binding
    /// first.
    /// @return The bound value, or an "unbound variable" error if @p name
    ///         has no variable binding.
    [[nodiscard]] constexpr auto lookup_value(symbol name) const
        -> smd::smdscheme::foundation::result<value<Core>>;

    /// Looks up @p name in the function namespace, most-recent binding
    /// first.
    /// @return The bound value, or an "undefined function" error if
    ///         @p name has no function binding.  Note: failing to find
    ///         @p name here says nothing about @ref lookup_value, and vice
    ///         versa; the two namespaces are looked up (and fail)
    ///         independently.
    [[nodiscard]] constexpr auto lookup_function(symbol name) const
        -> smd::smdscheme::foundation::result<value<Core>>;

    // set_value (the `setq` primitive) arrives in step L12; this
    // environment is intentionally immutable-once-defined until then.

  private:
    struct binding {
        symbol name{};
        value<Core> val{};
    };

    pair_heap<Core, default_max_pairs> *pairs_ =
        nullptr; ///< Non-owning; may be null (see @ref pairs).
    smd::smdscheme::foundation::static_vector<binding, MaxBindings> values_{};
    smd::smdscheme::foundation::static_vector<binding, MaxBindings>
        functions_{};
};

template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::define_value(symbol name,
                                                    value<Core> val) -> void {
    values_.push_back(binding{name, std::move(val)});
}

template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::define_function(symbol name,
                                                       value<Core> fn) -> void {
    functions_.push_back(binding{name, std::move(fn)});
}

template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::lookup_value(symbol name) const
    -> smd::smdscheme::foundation::result<value<Core>> {
    for (int i = values_.size() - 1; i >= 0; --i)
        if (values_[i].name == name)
            return values_[i].val;
    return smd::smdscheme::foundation::parse_error{{}, "unbound variable"};
}

template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::lookup_function(symbol name) const
    -> smd::smdscheme::foundation::result<value<Core>> {
    for (int i = functions_.size() - 1; i >= 0; --i)
        if (functions_[i].name == name)
            return functions_[i].val;
    return smd::smdscheme::foundation::parse_error{{}, "undefined function"};
}

/// Returns an environment pre-populated with the default builtins,
/// installed in the *function* namespace per step L9: `+`, `*`, `cons`,
/// `car`, `cdr`, `list`, `null`, `eq`, `eql`, `atom`, `funcall`, `apply`.
///
/// Symbol names are spelled uppercase to match what the reader (steps
/// L4-L6) produces after case folding (decision D2); `+` and `*` are
/// spelled as-is since case folding only affects letters.
///
/// @tparam Core        The core AST type.
/// @tparam MaxBindings Environment capacity.
template <typename Core, int MaxBindings>
[[nodiscard]] constexpr auto default_env() -> env<Core, MaxBindings> {
    env<Core, MaxBindings> e{};
    e.define_function(symbol{"+"}, value<Core>{builtin{builtin_op::add}});
    e.define_function(symbol{"*"}, value<Core>{builtin{builtin_op::multiply}});
    e.define_function(symbol{"CONS"}, value<Core>{builtin{builtin_op::cons}});
    e.define_function(symbol{"CAR"}, value<Core>{builtin{builtin_op::car}});
    e.define_function(symbol{"CDR"}, value<Core>{builtin{builtin_op::cdr}});
    e.define_function(symbol{"LIST"}, value<Core>{builtin{builtin_op::list}});
    e.define_function(symbol{"NULL"}, value<Core>{builtin{builtin_op::null}});
    e.define_function(symbol{"EQ"}, value<Core>{builtin{builtin_op::eq}});
    e.define_function(symbol{"EQL"}, value<Core>{builtin{builtin_op::eql}});
    e.define_function(symbol{"ATOM"}, value<Core>{builtin{builtin_op::atom}});
    e.define_function(symbol{"FUNCALL"},
                      value<Core>{builtin{builtin_op::funcall}});
    e.define_function(symbol{"APPLY"}, value<Core>{builtin{builtin_op::apply}});
    return e;
}

/// Returns an environment sharing pair heap @p p, pre-populated with the
/// same default builtins as the no-heap overload.
///
/// @p p must outlive the returned environment and every environment copied
/// from it (including closure captures).  The evaluator (step L11) needs
/// this overload: `cons`/`car`/`cdr`/`list` and the elaborator's hermetic
/// `core_cons` construction node both allocate into a shared heap, and
/// there is nowhere else to plug one in once `env` is built.
///
/// @tparam Core        The core AST type.
/// @tparam MaxBindings Environment capacity.
template <typename Core, int MaxBindings>
[[nodiscard]] constexpr auto default_env(pair_heap<Core, default_max_pairs> &p)
    -> env<Core, MaxBindings> {
    env<Core, MaxBindings> e{&p};
    e.define_function(symbol{"+"}, value<Core>{builtin{builtin_op::add}});
    e.define_function(symbol{"*"}, value<Core>{builtin{builtin_op::multiply}});
    e.define_function(symbol{"CONS"}, value<Core>{builtin{builtin_op::cons}});
    e.define_function(symbol{"CAR"}, value<Core>{builtin{builtin_op::car}});
    e.define_function(symbol{"CDR"}, value<Core>{builtin{builtin_op::cdr}});
    e.define_function(symbol{"LIST"}, value<Core>{builtin{builtin_op::list}});
    e.define_function(symbol{"NULL"}, value<Core>{builtin{builtin_op::null}});
    e.define_function(symbol{"EQ"}, value<Core>{builtin{builtin_op::eq}});
    e.define_function(symbol{"EQL"}, value<Core>{builtin{builtin_op::eql}});
    e.define_function(symbol{"ATOM"}, value<Core>{builtin{builtin_op::atom}});
    e.define_function(symbol{"FUNCALL"},
                      value<Core>{builtin{builtin_op::funcall}});
    e.define_function(symbol{"APPLY"}, value<Core>{builtin{builtin_op::apply}});
    return e;
}

/// Fixed default capacity of an @ref env_arena.
inline constexpr int default_max_envs = 128;

/// An arena of @ref env instances, addressed by stable pointer, that owns
/// every environment a runtime @ref closure captures (step L11).
///
/// **The closure-capture ownership decision, resolved.** Step L9 left
/// `closure<Core, MaxBindings>::captured` a non-owning raw pointer because
/// `value.hpp` cannot include `env.hpp` back (a real header-cycle
/// constraint, not a style choice — see `value.hpp`'s `closure` docs) and
/// so cannot embed a complete, owned `env` by value.  `smd::smdscheme`
/// sidesteps this with `constexpr_box<env<Core,16>>`, a `new`/`delete`
/// -backed owning box whose destructor runs when the owning `closure`
/// value is destroyed — but that RAII-on-`new` technique only works
/// because `smdscheme::closure::closure` and `smdscheme::closure::env` are
/// defined together in one header, where `env` is complete at every point
/// `constexpr_box<env<Core,16>>` needs it.  `smdlisp`'s split into
/// `value.hpp` (declares `closure`) and `env.hpp` (defines `env`) rules
/// that construction out for the `closure` struct itself.
///
/// The fix is to stop trying to own the environment *inside* the value
/// that references it, and instead give every captured environment a
/// stable home the evaluator itself owns for the whole evaluation — the
/// same trick @ref pair_heap already uses for cons cells.
/// `smd::smdscheme::foundation::static_vector` is array-backed with fixed
/// capacity, so once an element is pushed, its address never changes for
/// the lifetime of the arena (no reallocation, ever): pushing element N+1
/// cannot invalidate a pointer to element N.  A closure captures a raw
/// pointer into this arena instead of a pointer onto a C++ call-stack
/// local, and the arena — like @ref pair_heap and the core `tree_arena` —
/// is caller-owned and threaded through the whole evaluation by mutable
/// reference, so it necessarily outlives every closure built from it.  No
/// `new`/`delete` is needed anywhere in this path, which sidesteps the
/// separate question of whether a transient `new` inside one evaluation
/// gets `delete`d before that same constant evaluation finishes (the
/// concern `constexpr_box` exists to solve for the `smdscheme` design).
///
/// @tparam Core        The core AST type.
/// @tparam MaxBindings Capacity of each captured @ref env (must match the
///                      @ref env this arena's caller is evaluating with).
/// @tparam MaxEnvs      Maximum number of environments captured over one
///                      evaluation (one per closure materialized, i.e. one
///                      per evaluated `lambda`/`function` form, including
///                      repeated evaluations of the same source `lambda`).
template <typename Core, int MaxBindings, int MaxEnvs = default_max_envs>
class env_arena {
  public:
    constexpr env_arena() = default;

    // a73f8d39-7455-4ddc-a541-7424ab1d3a35
    /// Copies @p e into the arena and returns a stable, arena-owned
    /// pointer to the copy.  The returned pointer remains valid for the
    /// lifetime of this arena (never merely for the lifetime of the
    /// call that produced @p e).
    constexpr auto alloc(env<Core, MaxBindings> e)
        -> env<Core, MaxBindings> const * {
        envs_.push_back(std::move(e));
        return &envs_[envs_.size() - 1];
    }
    // a73f8d39-7455-4ddc-a541-7424ab1d3a35 end

  private:
    smd::smdscheme::foundation::static_vector<env<Core, MaxBindings>, MaxEnvs>
        envs_{};
};

} // namespace smd::smdlisp::closure

#endif
