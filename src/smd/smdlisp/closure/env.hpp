// src/smd/smdlisp/closure/env.hpp                                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_CLOSURE_ENV_HPP
#define SRC_SMD_SMDLISP_CLOSURE_ENV_HPP

#include <smd/smdlisp/closure/value.hpp>
#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/foundation/static_vector.hpp>

#include <utility>

namespace smd::smdlisp::closure {

/// Identifies one dynamic activation of a `block` form (decision D5, step
/// L14). A fresh id (and a fresh @ref exit_record) is allocated every time
/// a `block` form is *evaluated*, not once per static `(block name ...)`
/// occurrence -- a recursive function re-entering its own `block` allocates
/// a distinct record, and therefore a distinct id, on each call.
using exit_id = int;

/// The "no exit in flight" / "not yet allocated" sentinel for @ref exit_id.
inline constexpr exit_id invalid_exit = -1;

/// The shared, static, identity-compared marker used for a `return-from`
/// unwind travelling through @ref smd::smdscheme::foundation::result's
/// error channel (`eval_direct.hpp`/`cps_code.hpp`).
///
/// `result<value<Core>>` is a frozen two-alternative type (a value or a
/// @ref smd::smdscheme::foundation::parse_error -- `smd::smdscheme` is
/// frozen for semantic changes, decision D1), so a `return-from` unwind
/// cannot add a third alternative to the return channel or carry its
/// payload there directly. It travels instead as an ordinary
/// @c parse_error whose @c message is this single, static, program-wide
/// pointer -- every call site that already checks `!has_value()` before
/// invoking a continuation (`eval_direct`'s propagation pattern,
/// `cps_dispatch`/`cps_apply_function_value`'s continuation-guard pattern)
/// therefore already "skips intervening frames" for a `return-from`
/// exactly as it does for a genuine error, with no changes needed at any
/// site except @c core_block's and @c core_return_from's own handling.
/// Only pointer identity is compared (never string content), so this must
/// stay the single shared object every unwind uses -- see
/// @ref exit_record for where the actual target/payload information
/// travels instead (a stable, arena-owned pointer reachable through the
/// environment's block namespace, not through the error value itself).
///
/// The marker is backed by a *named* `inline constexpr` array
/// (@ref block_unwind_marker_storage), not a bare string literal, precisely
/// so that pointer identity is well-defined at run time as well as during
/// constant evaluation. A `constexpr char const *` initialized directly from
/// an anonymous string literal is constant-folded to the literal's address
/// at each use site; with string-literal merging disabled (as under the Asan
/// build) those sites get *distinct* addresses for equal content, so the
/// `message == block_unwind_marker` identity check would silently fail at run
/// time (it held only in constexpr) and a `return-from` unwind would escape
/// its own `block`. A single named object has exactly one address, so every
/// use resolves to the same pointer regardless of merging.
inline constexpr char block_unwind_marker_storage[] =
    "smdlisp: block/return-from unwind";
inline constexpr char const *block_unwind_marker = block_unwind_marker_storage;

// 1232b7f6-c9b9-4611-a958-bd1bb14b0ff5
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
// 1232b7f6-c9b9-4611-a958-bd1bb14b0ff5 end

/// One-shot lexical exit bookkeeping for one dynamic activation of a
/// `block` form (decision D5, step L14).
///
/// `block` allocates a fresh, live @ref exit_record every time its form is
/// evaluated (see @ref exit_id's docs) and installs a pointer to it under
/// its name in the *block* namespace of the environment used to evaluate
/// its body (@ref env::define_block) -- the same environment a nested
/// `lambda` captures by value, so a closure created inside the block's
/// dynamic extent carries this same stable pointer forward, even after the
/// block's own evaluation has returned. `return-from` resolves its target
/// block by name through @ref env::lookup_block, then:
///  - if @ref live is already false, the block's dynamic extent has
///    already ended (the closure-smuggling case) -- a diagnosed runtime
///    error, never UB;
///  - otherwise, it stores the return value in @ref payload, clears
///    @ref live, and signals the unwind via @ref block_unwind_marker.
///
/// `block`'s own evaluation, on regaining control (whether by falling off
/// the end normally or by observing a propagating unwind), checks whether
/// @ref live has gone false on *its own* record: if so, the unwind was
/// aimed at it specifically, and it resolves to @ref payload; otherwise it
/// marks its own record dead (its extent is ending too, non-locally) and
/// propagates whatever it received unchanged. See `eval_direct.hpp`'s and
/// `cps_code.hpp`'s `core_block`/`core_return_from` handling for the exact
/// sequencing.
///
/// Instances live in an @ref env_arena (@ref env_arena::alloc_exit), the
/// same stable, non-relocating storage @ref env instances themselves use,
/// for the identical reason: a captured closure's pointer to a record must
/// stay valid for as long as that closure might still be called, which can
/// outlast the C++ call stack frame that allocated the record.
///
/// @tparam Core The core AST type (used to type @ref payload).
template <typename Core>
struct exit_record {
    symbol name{};             ///< Block-namespace tag.
    exit_id id = invalid_exit; ///< Unique per dynamic activation.
    bool live = false;         ///< False once return-from has fired
                               ///< against this record, or the block's
                               ///< own evaluation has completed.
    value<Core> payload{};     ///< The value being returned; only
                               ///< meaningful while an unwind targeting
                               ///< this record is in flight.
};

// a5b4dac9-bc87-4fe7-9160-c3f1283df8a3
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
// a5b4dac9-bc87-4fe7-9160-c3f1283df8a3 end

/// A Lisp-2 lexical environment: two independent, linear,
/// most-recent-first binding lists, one per namespace (decision D4,
/// docs/cl-pivot-plan.md), plus a third, equally independent list for
/// `block`/`return-from` tags (decision D5, step L14).
///
/// Adapted by copy from `smd::smdscheme::closure::env`'s architecture
/// (`foundation::static_vector`-backed, linear search from the newest
/// binding toward the oldest, so a later `define_value`/`define_function`
/// shadows an earlier one for the same name).  Deltas from the Scheme
/// original:
///
///  - There are *two* binding lists instead of one.  `(f x)` resolves `f`
///    in the function namespace and `x` in the variable namespace, so the
///    same name can name a variable and a function at once without either
///    shadowing the other — that is the whole point of a Lisp-2.
///  - Only the *variable* namespace ever needs mutation (`setq`, step
///    L12); the function namespace never grows a `store` -- redefining a
///    function (`defun` again with the same name) is already handled by
///    ordinary most-recent-first shadowing via @ref define_function, with
///    no indirection required.
///  - Like `smd::smdscheme::closure::env`, an `env` operates in one of two
///    modes for its variable namespace: *functional* (no backing
///    @ref store; each binding holds its value inline, `setq` is a
///    diagnosed error) or *mutable* (constructed with a @ref store; each
///    binding names a store location, so `setq` on a captured variable is
///    visible through every closure that shares the binding, not just a
///    private copy).  See @ref set_value.
///  - **The block namespace (step L14).** `block` installs a name ->
///    @ref exit_record pointer binding here (@ref define_block) directly
///    into the *ambient*, mutably-referenced environment threaded through
///    a body sequence -- the same object a sibling `progn`/lambda-body
///    expression's `setq`/`defun`/`defvar` mutates in place -- rather than
///    into a fresh copy, so that mutation-across-a-sequence works
///    identically whether or not a `block` sits in the middle of the
///    sequence. A `lambda` created inside a `block`'s dynamic extent
///    captures this same environment (and therefore the same
///    @ref exit_record pointer) by value when it copies its captured
///    environment, which is exactly how a `return-from` inside a later-
///    invoked closure can still find (and diagnose staleness against) the
///    record that same-named `block` allocated -- see @ref exit_record's
///    docs for the full mechanism.
///
/// Like the Scheme original, `env` has no parent-environment link: a
/// nested lexical scope is a *copy* of the enclosing `env` with additional
/// bindings appended (via `define_value`/`define_function`), not a chain
/// of environments searched outward.  That copy-and-extend approach is
/// exactly what makes an eventual owning capture (see `value.hpp`'s
/// `closure` documentation) reproduce lexical scoping correctly: copying
/// an `env` copies its three `static_vector`s by value.
///
/// @tparam Core        The core AST type (see @ref value).
/// @tparam MaxBindings Capacity of each of the three binding lists.
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

    /// Constructs a mutable environment backed by variable @ref store @p s
    /// (step L12: `setq` needs somewhere to mutate).  No pair heap.
    constexpr explicit env(store<Core, default_max_store> *s) : store_(s) {}

    /// Constructs an environment sharing pair heap @p p and backed by
    /// variable @ref store @p s.  Both must outlive this environment and
    /// every environment copied from it (including closure captures).
    constexpr env(pair_heap<Core, default_max_pairs> *p,
                  store<Core, default_max_store> *s)
        : pairs_(p), store_(s) {}

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

    /// Assigns @p val to the nearest existing VARIABLE-namespace binding
    /// for @p name (the `setq` primitive, step L12), returning the
    /// *assigned value* per ANSI CL (unlike Scheme's `set!`, which returns
    /// the unspecified value -- see this function's out-of-line docs for
    /// the full return-type rationale).
    ///
    /// Errors if this environment has no backing @ref store (functional
    /// mode -- mirrors `smd::smdscheme::closure::env::assign`'s identical
    /// check) or if @p name has no VARIABLE-namespace binding.
    /// Special-variable dynamic rebinding is deferred to step L16; this
    /// function only ever mutates the nearest *lexical* binding, per the
    /// plan's explicit scope cut for this step.
    [[nodiscard]] constexpr auto set_value(symbol name, value<Core> val) const
        -> smd::smdscheme::foundation::result<value<Core>>;

    /// Marks @p name as a special variable (`defvar`/`defparameter`, step
    /// L12).  Recorded now; dynamic-binding *behavior* for special
    /// variables arrives in step L16 -- this step only stores the mark.
    /// Idempotent: marking an already-special name does not duplicate the
    /// entry.
    constexpr auto mark_special(symbol name) -> void;

    /// Returns whether @p name has been marked special by
    /// @ref mark_special.
    [[nodiscard]] constexpr auto is_special(symbol name) const -> bool;

    /// Adds a new block-namespace binding for @p name -> @p record (the
    /// `block` primitive, step L14).  A later binding for the same
    /// @p name (a sibling or re-entrant `block` with the same tag) shadows
    /// this one, matching @ref define_value/@ref define_function's
    /// most-recent-first shadowing.  Entirely independent of the other two
    /// namespaces (decision D4): a `block` named the same as an in-scope
    /// variable or function neither shadows nor is shadowed by it.
    constexpr auto define_block(symbol name, exit_record<Core> *record) -> void;

    /// Looks up @p name in the block namespace, most-recent binding first
    /// (the `return-from` primitive, step L14).
    /// @return The bound @ref exit_record pointer, or an "undefined block"
    ///         error if @p name has no block binding.  Reaching this error
    ///         at runtime should not happen for a `return-from` the
    ///         elaborator accepted (decision D5: block-name resolution is
    ///         lexical, done at elaboration time) -- see
    ///         @ref exit_record::live for the runtime check that *does*
    ///         apply to an elaborator-accepted `return-from`.
    [[nodiscard]] constexpr auto lookup_block(symbol name) const
        -> smd::smdscheme::foundation::result<exit_record<Core> *>;

  private:
    struct binding {
        symbol name{};
        value<Core> val{}; ///< Inline value in functional mode; unused
                           ///< (see @ref loc) in mutable mode.
        int loc = -1;      ///< VARIABLE-namespace @ref store location in
                           ///< mutable mode; -1 otherwise.  Never set for
                           ///< @ref functions_ bindings (the function namespace
                           ///< has no mutable mode; see the class docs).
    };

    struct block_binding {
        symbol name{};
        exit_record<Core> *record = nullptr; ///< Non-owning; stable for the
                                             ///< lifetime of the owning
                                             ///< @ref env_arena (see
                                             ///< @ref exit_record's docs).
    };

    pair_heap<Core, default_max_pairs> *pairs_ =
        nullptr; ///< Non-owning; may be null (see @ref pairs).
    store<Core, default_max_store> *store_ =
        nullptr; ///< Non-owning; may be null (functional mode; see the
                 ///< class docs and @ref set_value).
    smd::smdscheme::foundation::static_vector<binding, MaxBindings> values_{};
    smd::smdscheme::foundation::static_vector<binding, MaxBindings>
        functions_{};
    smd::smdscheme::foundation::static_vector<symbol, MaxBindings> special_{};
    smd::smdscheme::foundation::static_vector<block_binding, MaxBindings>
        blocks_{};
};

template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::define_value(symbol name,
                                                    value<Core> val) -> void {
    if (store_ != nullptr) {
        int loc = store_->alloc(std::move(val));
        values_.push_back(binding{name, {}, loc});
    } else {
        values_.push_back(binding{name, std::move(val), -1});
    }
}

template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::define_function(symbol name,
                                                       value<Core> fn) -> void {
    functions_.push_back(binding{name, std::move(fn), -1});
}

template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::lookup_value(symbol name) const
    -> smd::smdscheme::foundation::result<value<Core>> {
    for (int i = values_.size() - 1; i >= 0; --i) {
        if (values_[i].name == name) {
            if (store_ != nullptr)
                return store_->get(values_[i].loc);
            return values_[i].val;
        }
    }
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

// 2d0e5b9a-4f4d-4b1a-9a4e-2b6a2e6c9a41
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
// 2d0e5b9a-4f4d-4b1a-9a4e-2b6a2e6c9a41 end

template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::mark_special(symbol name) -> void {
    for (auto const &s : special_)
        if (s == name)
            return;
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
constexpr auto env<Core, MaxBindings>::define_block(symbol name,
                                                    exit_record<Core> *record)
    -> void {
    blocks_.push_back(block_binding{name, record});
}

template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::lookup_block(symbol name) const
    -> smd::smdscheme::foundation::result<exit_record<Core> *> {
    for (int i = blocks_.size() - 1; i >= 0; --i)
        if (blocks_[i].name == name)
            return blocks_[i].record;
    return smd::smdscheme::foundation::parse_error{{}, "undefined block"};
}

/// Returns an environment pre-populated with the default builtins,
/// installed in the *function* namespace per step L9: `+`, `*`, `cons`,
/// `car`, `cdr`, `list`, `null`, `eq`, `eql`, `atom`, `funcall`, `apply`,
/// `append` (the last added in step L18, for backquote's `,@x` lowering).
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
    e.define_function(symbol{"APPEND"},
                      value<Core>{builtin{builtin_op::append}});
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
    e.define_function(symbol{"APPEND"},
                      value<Core>{builtin{builtin_op::append}});
    return e;
}

/// Returns an environment sharing pair heap @p p and variable @ref store
/// @p s, pre-populated with the same default builtins as the other
/// overloads.  This is the *mutable* overload (step L12): `setq` on a
/// variable bound through the returned environment (or any environment
/// copied from it, including closure captures) succeeds, because
/// @ref env::define_value threads new bindings through @p s.
///
/// @p p and @p s must outlive the returned environment and every
/// environment copied from it.
///
/// @tparam Core        The core AST type.
/// @tparam MaxBindings Environment capacity.
template <typename Core, int MaxBindings>
[[nodiscard]] constexpr auto default_env(pair_heap<Core, default_max_pairs> &p,
                                         store<Core, default_max_store> &s)
    -> env<Core, MaxBindings> {
    env<Core, MaxBindings> e{&p, &s};
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
    e.define_function(symbol{"APPEND"},
                      value<Core>{builtin{builtin_op::append}});
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
/// Fixed default capacity of the @ref exit_record storage an @ref env_arena
/// also owns (step L14).
inline constexpr int default_max_exits = 128;

/// Fixed default capacity of the dynamic `catch` stack an @ref env_arena
/// also owns (step L15).  Unlike @ref default_max_exits this bounds the
/// maximum *nesting depth* of simultaneously-active `catch` frames, not the
/// total number of activations: see @ref env_arena::push_catch.
inline constexpr int default_max_catches = 128;

/// @tparam Core        The core AST type.
/// @tparam MaxBindings Capacity of each captured @ref env (must match the
///                      @ref env this arena's caller is evaluating with).
/// @tparam MaxEnvs      Maximum number of environments captured over one
///                      evaluation (one per closure materialized, i.e. one
///                      per evaluated `lambda`/`function` form, including
///                      repeated evaluations of the same source `lambda`).
/// @tparam MaxExits     Maximum number of `block` activations (one
///                      @ref exit_record each) live at once over one
///                      evaluation (step L14; see @ref alloc_exit).
/// @tparam MaxCatches   Maximum *nesting depth* of dynamically active
///                      `catch` frames (step L15; see @ref push_catch).
template <typename Core, int MaxBindings, int MaxEnvs = default_max_envs,
          int MaxExits = default_max_exits,
          int MaxCatches = default_max_catches>
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

    /// Allocates a fresh, live @ref exit_record for one dynamic activation
    /// of a `block` named @p name (step L14) and returns a stable,
    /// arena-owned pointer to it -- the same non-relocating-storage
    /// guarantee @ref alloc gives captured environments, and for the
    /// identical reason: a closure created inside the block's dynamic
    /// extent may capture this pointer (via @ref env::define_block) and
    /// outlive the block's own evaluation.  Each call gets a distinct
    /// @ref exit_id, even for repeated activations of the same static
    /// `(block name ...)` occurrence (e.g. one per recursive call).
    constexpr auto alloc_exit(symbol name) -> exit_record<Core> * {
        exit_id const id = next_exit_id_++;
        exits_.push_back(exit_record<Core>{name, id, true, value<Core>{}});
        return &exits_[exits_.size() - 1];
    }

    // 107e5185-5361-44a8-9fbd-016334660d25
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
    // 107e5185-5361-44a8-9fbd-016334660d25 end

    /// Pops the innermost `catch` frame pushed by @ref push_catch.
    ///
    /// Every exit path out of a `catch` body -- normal completion, an
    /// ordinary error, a `return-from` unwind passing through, a `throw`
    /// aimed at this frame, and a `throw` aimed at an outer one -- must call
    /// this exactly once, which is what keeps the stack's depth in step with
    /// the forms' real dynamic extents (see the evaluators' `core_catch`
    /// handling).
    constexpr auto pop_catch() -> void {
        if (depth_ > 0)
            --depth_;
    }

    /// Returns the innermost dynamically active, still-live `catch` frame
    /// whose tag is `eq` to @p tag, or null if there is none -- the search
    /// `throw` performs (step L15).
    ///
    /// Comparison is `value<Core>`'s @c operator==, which is precisely what
    /// `pairs.hpp`'s `eq` primitive applies (identity for @ref pair_ref,
    /// by-value for every other alternative), so a program can predict which
    /// frame a `throw` reaches using the same `eq` it can call directly.
    /// A null return is the *uncaught throw* case, which the evaluators
    /// report as a diagnosed error rather than unwinding anywhere.
    [[nodiscard]] constexpr auto find_catch(value<Core> const &tag)
        -> catch_record<Core> * {
        for (int i = depth_ - 1; i >= 0; --i)
            if (frames_[i].live && frames_[i].tag == tag)
                return &frames_[i];
        return nullptr;
    }

    /// Returns the current `catch`-frame nesting depth (test/diagnostic
    /// support: a balanced program returns to depth 0, which is how the
    /// tests witness that every exit path pops exactly once).
    [[nodiscard]] constexpr auto catch_depth() const -> int { return depth_; }

  private:
    smd::smdscheme::foundation::static_vector<env<Core, MaxBindings>, MaxEnvs>
        envs_{};
    smd::smdscheme::foundation::static_vector<exit_record<Core>, MaxExits>
        exits_{};
    smd::smdscheme::foundation::static_vector<catch_record<Core>, MaxCatches>
        frames_{};
    int depth_ = 0; ///< Live prefix of @ref frames_; see @ref push_catch.
    exit_id next_exit_id_ = 0;
};

} // namespace smd::smdlisp::closure

#endif
