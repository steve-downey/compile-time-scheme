// src/smd/smdlisp/closure/env.hpp                                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_CLOSURE_ENV_HPP
#define SRC_SMD_SMDLISP_CLOSURE_ENV_HPP

#include <smd/smdlisp/closure/value.hpp>
#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/foundation/static_vector.hpp>

#include <utility>

namespace smd::smdlisp::closure {

/// Fixed capacity of the mutable @ref store shared across every @ref env
/// copy, including closure captures.
inline constexpr int default_max_store = 256;

/// A flat, constexpr, allocation-free store of mutable value cells,
/// addressed by stable integer locations.
///
/// Adapted by copy from `smd::smdscheme::closure::store` (the `set!`
/// machinery step L12's `setq` is built on). @ref
/// smd::smdscheme::foundation::static_vector is array-backed and never
/// reallocates, so a location handed out by @ref alloc stays valid for the
/// store's whole lifetime. @ref env shares one `store*` (non-owning) across
/// every copy of itself -- including every captured closure environment --
/// so mutating a cell through @ref set is visible from every copy that
/// shares this store: exactly what `setq` needs, and also what `defun`'s
/// self-recursion patch needs (see @ref env::patch_function). The store is
/// shared across BOTH the variable and function namespaces (see @ref
/// env::define_value / @ref env::define_function): a store cell does not
/// care which namespace's binding list points at it.
///
/// @tparam Core     The core AST type.
/// @tparam MaxStore Maximum number of cells.
template <typename Core, int MaxStore>
class store {
  public:
    /// Appends @p v and returns its stable location.
    constexpr auto alloc(value<Core> v) -> int {
        int loc = cells_.size();
        cells_.push_back(std::move(v));
        return loc;
    }

    /// Returns the value at @p loc.
    [[nodiscard]] constexpr auto get(int loc) const -> value<Core> const & {
        return cells_[loc];
    }

    /// Overwrites the cell at @p loc with @p v.
    constexpr auto set(int loc, value<Core> v) -> void {
        cells_[loc] = std::move(v);
    }

  private:
    smd::smdscheme::foundation::static_vector<value<Core>, MaxStore> cells_{};
};

/// A Lisp-2 lexical environment: two independent, linear,
/// most-recent-first binding lists, one per namespace (decision D4,
/// docs/cl-pivot-plan.md).
///
/// Adapted by copy from `smd::smdscheme::closure::env`'s architecture
/// (`foundation::static_vector`-backed, linear search from the newest
/// binding toward the oldest, so a later `define_value`/`define_function`
/// shadows an earlier one for the same name). Two deltas from the Scheme
/// original, both intentional:
///
///  - There are *two* binding lists instead of one. `(f x)` resolves `f`
///    in the function namespace and `x` in the variable namespace, so the
///    same name can name a variable and a function at once without either
///    shadowing the other -- that is the whole point of a Lisp-2.
///  - **Step L12: a dual mode, adapted from `smd::smdscheme::closure::env`'s
///    functional-vs-mutable split.** With no backing @ref store (the L9-L11
///    default), every binding holds its value inline ("functional" mode; no
///    mutation support). Constructed with a @ref store, every binding
///    instead names a location in the shared store ("mutable" mode): this
///    is what lets @ref set_value (`setq`) mutate a binding such that the
///    mutation is visible through every @ref env copy that shares the same
///    store, including a closure's captured copy. Unlike the Scheme
///    original (a single namespace), this store is shared by BOTH
///    @ref define_value and @ref define_function, because `defun`'s
///    self-recursion trick (@ref patch_function) needs the exact same
///    "mutate in place, visible through every copy" property for the
///    FUNCTION namespace: a self-recursive function's own captured
///    environment must observe its own (patched-in) closure value.
///
/// Like the Scheme original, `env` has no parent-environment link: a
/// nested lexical scope is a *copy* of the enclosing `env` with additional
/// bindings appended (via `define_value`/`define_function`), not a chain
/// of environments searched outward. That copy-and-extend approach is
/// exactly what makes an eventual owning capture (see `value.hpp`'s
/// `closure` documentation) reproduce lexical scoping correctly: copying
/// an `env` copies its two `static_vector`s by value (and, in mutable
/// mode, the shared, non-owning @ref store pointer -- a shallow copy, by
/// design, so mutations after the copy stay visible to both).
///
/// @tparam Core        The core AST type (see @ref value).
/// @tparam MaxBindings Capacity of each of the two binding lists.
template <typename Core, int MaxBindings>
class env {
  public:
    constexpr env() = default;

    /// Constructs an environment sharing the pair heap @p p (step L11: the
    /// evaluator needs `cons`/`car`/`cdr`/`list` builtins and the hermetic
    /// `core_cons` construction node to reach a common heap). Mirrors
    /// `smd::smdscheme::closure::env`'s `pairs_` member and @ref pairs
    /// accessor; @p p must outlive this environment and every environment
    /// copied from it (including closure captures).
    constexpr explicit env(pair_heap<Core, default_max_pairs> *p) : pairs_(p) {}

    /// Constructs a mutable environment backed by store @p s (step L12):
    /// every @ref define_value / @ref define_function call allocates its
    /// binding's value in @p s rather than holding it inline, which is what
    /// makes @ref set_value (`setq`) and @ref patch_function (`defun`
    /// self-recursion) able to mutate a binding visibly across every copy
    /// of this environment. @p s must outlive this environment and every
    /// environment copied from it (including closure captures).
    constexpr explicit env(store<Core, default_max_store> *s) : store_(s) {}

    /// Constructs a mutable environment backed by store @p s and sharing
    /// pair heap @p p. Both must outlive this environment and every
    /// environment copied from it (including closure captures).
    constexpr env(store<Core, default_max_store> *s,
                  pair_heap<Core, default_max_pairs> *p)
        : store_(s), pairs_(p) {}

    /// Returns the shared pair heap (non-owning; null if this environment
    /// has none). List primitives and @c core_cons construction allocate
    /// and dereference cells through it.
    [[nodiscard]] constexpr auto pairs() const
        -> pair_heap<Core, default_max_pairs> * {
        return pairs_;
    }

    /// Adds a new variable binding for @p name -> @p val.
    /// A later binding for the same @p name shadows this one.
    /// In mutable mode (see the class docs), @p val is allocated in the
    /// shared @ref store; in functional mode it is held inline.
    constexpr auto define_value(symbol name, value<Core> val) -> void;

    /// Adds a new function binding for @p name -> @p fn, returning the
    /// store location the binding was allocated at (mutable mode) or -1
    /// (functional mode, no store). A later binding for the same @p name
    /// shadows this one. Entirely independent of @ref define_value: the
    /// same @p name may be bound in both namespaces simultaneously.
    ///
    /// The returned location exists so `defun`'s evaluator case (step L12)
    /// can install a placeholder function value, capture the environment
    /// (so a self-recursive call inside the function's own body can find
    /// its own name), and only then patch the placeholder to the real
    /// closure value via @ref patch_function -- see that function's docs.
    constexpr auto define_function(symbol name, value<Core> fn) -> int;

    /// Overwrites the function binding at store location @p loc (as
    /// returned by @ref define_function) with @p val, if this environment
    /// is mutable-mode (@p loc >= 0 and store-backed); otherwise falls
    /// back to an ordinary @ref define_function(@p name, @p val) call,
    /// which shadows the placeholder binding with a fresh, correct one.
    ///
    /// **Why `defun` needs this two-step reserve-then-patch dance (step
    /// L12).** A self-recursive `defun` (e.g. `(defun fact (n) (if (eq n 0)
    /// 1 (* n (fact (- n 1))))))`) must let `fact`'s own body find `fact`
    /// in the FUNCTION namespace of its own captured environment. But the
    /// captured environment is a *copy* taken at the moment the closure is
    /// built (@ref smd::smdlisp::closure::env_arena::alloc), and the real
    /// closure value cannot exist yet at that moment -- it needs the
    /// captured-environment pointer as one of its own fields. The fix:
    /// install a placeholder binding for the name FIRST (via
    /// @ref define_function, in mutable mode allocating a store cell),
    /// capture the environment (the copy shares the *same* store pointer,
    /// a shallow copy), build the real closure value, then patch the
    /// store cell in place. Every environment copy that shares the store
    /// -- including the just-captured one -- observes the patched value on
    /// its next lookup, because @ref env::lookup_function fetches through
    /// the shared store rather than an inline per-copy value. In
    /// functional mode (no store), there is nothing to patch in place, so
    /// this falls back to shadowing with an ordinary, correct binding for
    /// the *current* environment object -- external callers of the
    /// function still get the right value, but a self-recursive call
    /// inside the function's own body will not find itself (an
    /// undefined-function error), matching the same "no mutation support
    /// without a store" limitation @ref set_value already has.
    constexpr auto patch_function(symbol name, int loc, value<Core> val)
        -> void;

    /// Adds a new variable binding for @p name -> @p val (as
    /// @ref define_value) and additionally proclaims @p name special (see
    /// @ref is_special) -- the `defvar`/`defparameter` primitive.
    constexpr auto define_special_value(symbol name, value<Core> val) -> void;

    /// Proclaims @p name special without touching either binding
    /// namespace. Step L12 scope: this only records the mark (consumed by
    /// step L16's dynamic binding); it has no binding-lookup effect yet.
    constexpr auto declare_special(symbol name) -> void;

    /// @return Whether @p name has been proclaimed special (via
    ///         @ref declare_special / @ref define_special_value) in this
    ///         environment.
    [[nodiscard]] constexpr auto is_special(symbol name) const -> bool;

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

    /// Mutates the nearest LEXICAL variable binding for @p name to @p val
    /// -- the `setq` primitive (step L12).
    ///
    /// **Decision, this step: the return type is `result<value<Core>>`,
    /// echoing @p val back on success, not `result<void>`.** The plan's own
    /// sketch (docs/cl-pivot-plan.md section 9, step L12) writes
    /// `set_value(symbol, value) -> result<void>`, but also explicitly
    /// notes "`setq` returns the assigned value per CL, not the Scheme
    /// `unspecified`" -- a `result<void>` return cannot carry that value
    /// back to the evaluator's `core_setq` case, so `result<value<Core>>`
    /// is the type that actually satisfies the stated ANSI CL behavior.
    ///
    /// Only searches the VARIABLE namespace (`setq` never touches function
    /// bindings) and only ever mutates a LEXICAL binding already present in
    /// this environment: special-variable/dynamic-binding interaction is
    /// explicitly out of scope until step L16 (docs/cl-pivot-plan.md step
    /// L12's "special-variable interaction is explicitly deferred").
    ///
    /// @return @p val on success; a "setq: environment has no mutable
    ///         store" error if this environment is functional-mode (no
    ///         @ref store); a "setq: unbound variable" error if @p name has
    ///         no variable binding.
    [[nodiscard]] constexpr auto set_value(symbol name, value<Core> val) const
        -> smd::smdscheme::foundation::result<value<Core>>;

  private:
    struct binding {
        symbol name{};
        int loc = -1;      ///< Store location in mutable mode; -1 otherwise.
        value<Core> val{}; ///< Inline value in functional mode; unused if
                           ///< loc >= 0.
    };

    store<Core, default_max_store> *store_ =
        nullptr; ///< Non-owning; may be null (functional mode).
    pair_heap<Core, default_max_pairs> *pairs_ =
        nullptr; ///< Non-owning; may be null (see @ref pairs).
    smd::smdscheme::foundation::static_vector<binding, MaxBindings> values_{};
    smd::smdscheme::foundation::static_vector<binding, MaxBindings>
        functions_{};
    smd::smdscheme::foundation::static_vector<symbol, MaxBindings> special_{};
};

template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::define_value(symbol name,
                                                    value<Core> val) -> void {
    if (store_ != nullptr) {
        int loc = store_->alloc(std::move(val));
        values_.push_back(binding{name, loc, {}});
    } else {
        values_.push_back(binding{name, -1, std::move(val)});
    }
}

template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::define_function(symbol name,
                                                       value<Core> fn) -> int {
    if (store_ != nullptr) {
        int loc = store_->alloc(std::move(fn));
        functions_.push_back(binding{name, loc, {}});
        return loc;
    }
    functions_.push_back(binding{name, -1, std::move(fn)});
    return -1;
}

template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::patch_function(symbol name, int loc,
                                                      value<Core> val) -> void {
    if (store_ != nullptr && loc >= 0) {
        store_->set(loc, std::move(val));
        return;
    }
    define_function(name, std::move(val));
}

template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::define_special_value(symbol name,
                                                            value<Core> val)
    -> void {
    define_value(name, std::move(val));
    declare_special(name);
}

template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::declare_special(symbol name) -> void {
    special_.push_back(name);
}

template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::is_special(symbol name) const -> bool {
    for (auto const &s : special_)
        if (s == name)
            return true;
    return false;
}

template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::lookup_value(symbol name) const
    -> smd::smdscheme::foundation::result<value<Core>> {
    for (int i = values_.size() - 1; i >= 0; --i)
        if (values_[i].name == name)
            return values_[i].loc >= 0 ? store_->get(values_[i].loc)
                                       : values_[i].val;
    return smd::smdscheme::foundation::parse_error{{}, "unbound variable"};
}

template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::lookup_function(symbol name) const
    -> smd::smdscheme::foundation::result<value<Core>> {
    for (int i = functions_.size() - 1; i >= 0; --i)
        if (functions_[i].name == name)
            return functions_[i].loc >= 0 ? store_->get(functions_[i].loc)
                                          : functions_[i].val;
    return smd::smdscheme::foundation::parse_error{{}, "undefined function"};
}

template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::set_value(symbol name,
                                                 value<Core> val) const
    -> smd::smdscheme::foundation::result<value<Core>> {
    if (store_ == nullptr)
        return smd::smdscheme::foundation::parse_error{
            {}, "setq: environment has no mutable store"};
    for (int i = values_.size() - 1; i >= 0; --i) {
        if (values_[i].name == name) {
            store_->set(values_[i].loc, val);
            return val;
        }
    }
    return smd::smdscheme::foundation::parse_error{{},
                                                   "setq: unbound variable"};
}

namespace detail {

/// Installs the same set of default builtins into @p e, regardless of
/// which @ref default_env overload constructed it. Factored out so the
/// four @ref default_env overloads (no-store/store x no-pairs/pairs) do not
/// each repeat the same twelve `define_function` calls.
template <typename Core, int MaxBindings>
constexpr auto install_default_builtins(env<Core, MaxBindings> &e) -> void {
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
}

} // namespace detail

/// Returns an environment pre-populated with the default builtins,
/// installed in the *function* namespace per step L9: `+`, `*`, `cons`,
/// `car`, `cdr`, `list`, `null`, `eq`, `eql`, `atom`, `funcall`, `apply`.
/// Functional mode (no @ref store): does not support @ref set_value
/// (`setq`) or `defun` self-recursion (see @ref patch_function).
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
    detail::install_default_builtins(e);
    return e;
}

/// Returns an environment sharing pair heap @p p, pre-populated with the
/// same default builtins as the no-argument overload. Functional mode (no
/// @ref store): see that overload's docs for what this does not support.
///
/// @p p must outlive the returned environment and every environment copied
/// from it (including closure captures).
///
/// @tparam Core        The core AST type.
/// @tparam MaxBindings Environment capacity.
template <typename Core, int MaxBindings>
[[nodiscard]] constexpr auto default_env(pair_heap<Core, default_max_pairs> &p)
    -> env<Core, MaxBindings> {
    env<Core, MaxBindings> e{&p};
    detail::install_default_builtins(e);
    return e;
}

/// Returns a mutable environment (step L12) backed by store @p s,
/// pre-populated with the same default builtins. Supports @ref set_value
/// (`setq`) and `defun` self-recursion (@ref patch_function); has no pair
/// heap, so `cons`/`car`/`cdr`/`list` and quoted-list construction will
/// error.
///
/// @p s must outlive the returned environment and every environment copied
/// from it (including closure captures).
///
/// @tparam Core        The core AST type.
/// @tparam MaxBindings Environment capacity.
template <typename Core, int MaxBindings>
[[nodiscard]] constexpr auto default_env(store<Core, default_max_store> &s)
    -> env<Core, MaxBindings> {
    env<Core, MaxBindings> e{&s};
    detail::install_default_builtins(e);
    return e;
}

/// Returns a mutable environment (step L12) backed by store @p s and
/// sharing pair heap @p p, pre-populated with the default builtins. The
/// fully-featured default environment: supports @ref set_value (`setq`),
/// `defun` self-recursion (@ref patch_function), and the pair primitives.
///
/// Both @p s and @p p must outlive the returned environment and every
/// environment copied from it (including closure captures).
///
/// @tparam Core        The core AST type.
/// @tparam MaxBindings Environment capacity.
template <typename Core, int MaxBindings>
[[nodiscard]] constexpr auto default_env(store<Core, default_max_store> &s,
                                         pair_heap<Core, default_max_pairs> &p)
    -> env<Core, MaxBindings> {
    env<Core, MaxBindings> e{&s, &p};
    detail::install_default_builtins(e);
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
