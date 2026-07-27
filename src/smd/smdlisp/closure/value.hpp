// src/smd/smdlisp/closure/value.hpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_CLOSURE_VALUE_HPP
#define SRC_SMD_SMDLISP_CLOSURE_VALUE_HPP

#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/foundation/static_vector.hpp>

#include <span>
#include <string_view>
#include <utility>
#include <variant>

namespace smd::smdlisp::closure {

/// Built-in operations supported at the value level.
///
/// Step L9 (the Lisp-2 environment) needs one tag type it can install into
/// the *function* namespace for every default builtin, so this enum is the
/// superset covering arithmetic (`add`, `multiply`) and the step L8 list
/// primitives — `cons`, `car`, `cdr`, `list`, `null`, `eq`, `eql`, `atom`,
/// named identically to `pairs.hpp`'s `list_op` — plus `funcall` and
/// `apply`.  `pairs.hpp::apply_prim` still consumes its own `list_op`; the
/// two enums are deliberately kept as separate types rather than merged,
/// because `pairs.hpp` includes `value.hpp` (for `value<Core>`), so
/// `value.hpp` naming `list_op` back would be a header cycle.  A later
/// evaluator step (L10/L11) bridges the two over their shared names.
/// `funcall` and `apply` have no `apply_prim` case: invoking a closure
/// value needs the evaluator itself, not just value-level plumbing.
/// `append` joins the set in step L18: the backquote expander lowers
/// `,@x` (unquote-splicing) to a call against this builtin, again named
/// identically to `pairs.hpp::list_op::append`.
/// `values` joins in step L20 and, like `funcall`/`apply`, has no
/// `apply_prim` case: it is the one builtin whose result is *n-ary*, so it
/// can only be implemented where the evaluator's n-ary return channel is in
/// scope (see @ref value_list).  Making it an ordinary builtin rather than a
/// special operator is what ANSI CL actually specifies -- `values` is a
/// function -- and is what makes `#'values`, `(funcall #'values 1 2)` and
/// `(apply #'values '(1 2))` work with no extra machinery.
enum class builtin_op {
    add,
    multiply,
    cons,
    car,
    cdr,
    list,
    null,
    eq,
    eql,
    atom,
    funcall,
    apply,
    append,
    values
};

/// A built-in operator value (holds the operation kind).
struct builtin {
    builtin_op op;
    friend constexpr auto operator==(builtin, builtin) -> bool = default;
};

/// The distinguished `nil` value.
///
/// Per decision D3 (docs/cl-pivot-plan.md), `nil` is a single value kind that
/// simultaneously serves as the sole false value, the empty list, and the
/// symbol `NIL`.  There is no separate boolean kind: truthiness is decided
/// exclusively by @ref is_true, never by a per-site encoding.
struct nil_t {
    friend constexpr auto operator==(nil_t, nil_t) -> bool = default;
};

/// A cons cell handle: a non-owning, stable-location reference into a
/// @ref pair_heap.
///
/// Adapted by copy from `smd::smdscheme::closure::pair_ref` (PR #26, step
/// L8).  A pair is the first value that is recursive, shareable, and
/// mutable, so — as with the Scheme original — it cannot be a deep-copy
/// value the way a scalar is: two names must be able to refer to the *same*
/// cell.  Unlike Scheme, there is no separate `null_t` empty-list kind: per
/// decision D3, `nil` (@ref nil_t) already serves as the empty list, so a
/// proper list's final `cdr` is a @ref nil_t value rather than a distinct
/// end-marker type.  Equality is identity: two @ref pair_ref compare equal
/// iff they name the same cell (CL's `eq`).
struct pair_ref {
    int loc = -1; ///< Stable heap location; -1 means "no pair".
    friend constexpr auto operator==(pair_ref lhs, pair_ref rhs) -> bool {
        return lhs.loc == rhs.loc;
    }
};

/// Forward declaration of the Lisp-2 environment, defined in `env.hpp`
/// (step L9).
template <typename Core, int MaxBindings>
class env;

/// A first-class closure: a lambda node paired with a captured environment.
///
/// `node` points into the core arena; it is non-owning.  `MaxBindings`
/// parameterizes the capacity of the captured @ref env — the "hard-wired
/// 16" wart the plan calls out — and defaults to 16 so `closure<Core>`
/// alone still names a usable type.
///
/// **Capture ownership, decided in step L9.** `captured` stays a
/// non-owning raw pointer rather than becoming an owning
/// `constexpr_box<env<Core, MaxBindings>>` deep copy the way
/// `smd::smdscheme::closure::closure` does it.  That is not a style
/// choice: `env.hpp` includes `value.hpp` (`env` needs `value<Core>`,
/// `symbol`, `builtin_op`, ...), so `value.hpp` cannot include `env.hpp`
/// back without a header cycle, and therefore cannot embed a complete
/// `env` by value.  `smdscheme` sidesteps the cycle by defining `closure`,
/// `constexpr_box`, and `env` together in one file; the plan for step L9
/// asks for `env` in its own header instead, so that escape hatch is not
/// available here.  Ownership of the concrete `env` instances a closure
/// captures is therefore deferred to whichever step first constructs real
/// closures (L10/L11's evaluator).  The natural fit is an "env arena" in
/// the same stable-index style already used by @ref pair_heap (and by the
/// Scheme `store`): a closure would capture a stable, arena-owned pointer,
/// never a raw pointer onto a C++ call-stack local that could outlive its
/// evaluator frame.
///
/// @tparam Core        The core AST type.
/// @tparam MaxBindings Capacity of the captured environment (default 16).
template <typename Core, int MaxBindings = 16>
struct closure {
    Core const *node = nullptr; ///< Non-owning pointer to the lambda node.
    env<Core, MaxBindings> const *captured =
        nullptr; ///< Non-owning; see the ownership note above.

    friend constexpr auto operator==(closure<Core, MaxBindings> const &lhs,
                                     closure<Core, MaxBindings> const &rhs)
        -> bool {
        // Simple structural equality for test purposes.
        return lhs.node == rhs.node;
    }
};

/// An interned symbol at runtime (not a compile-time identifier).
///
/// Per decision D2, unescaped symbol names are folded to uppercase at read
/// time; this type just holds whatever spelling it is given.
struct symbol {
    std::string_view name;
    friend constexpr auto operator==(symbol const &lhs, symbol const &rhs)
        -> bool {
        return lhs.name == rhs.name;
    }
};

/// A self-evaluating keyword (`:foo`), folded to uppercase per D2.
///
/// A keyword is a distinct value kind from @ref symbol so that keyword-ness
/// is a static, checkable property rather than a naming convention.
struct keyword {
    std::string_view name;
    friend constexpr auto operator==(keyword const &lhs, keyword const &rhs)
        -> bool {
        return lhs.name == rhs.name;
    }
};

/// A foreign (C++) function callable from the compiled Lisp program.
///
/// `fn` is a function pointer with signature `result<value>(span<value
/// const>)`.  This lets test harnesses and extension points install native
/// callbacks without defining new core forms.
///
/// @tparam Core The core AST type.
template <typename Core>
struct foreign_function {
    using val_t = std::variant<nil_t, int, symbol, keyword, builtin,
                               closure<Core>, foreign_function, pair_ref>;
    using sig_t =
        smd::smdscheme::foundation::result<val_t> (*)(std::span<val_t const>);

    sig_t fn = nullptr; ///< Pointer to the native implementation.

    friend constexpr auto operator==(foreign_function const &lhs,
                                     foreign_function const &rhs) -> bool {
        return lhs.fn == rhs.fn;
    }
};

/// The runtime value type: `nil`, integer, symbol, keyword, builtin,
/// closure, foreign function, or cons cell.
///
/// Cons cells (@ref pair_ref) arrive in step L8 (decision D6, proper lists
/// first); there is no separate empty-list kind because `nil` already fills
/// that role (decision D3).
///
/// @tparam Core The core AST type (used to type @ref closure and
///              @ref foreign_function).
template <typename Core>
using value = std::variant<nil_t, int, symbol, keyword, builtin, closure<Core>,
                           foreign_function<Core>, pair_ref>;

// a4a90af9-fd64-4f75-ad47-609c7c1e50fd
/// Fixed default bound on how many values one form may return (step L20).
///
/// This bounds the *width* of a single multiple-value return, not any kind
/// of nesting or activation count, so it is small on purpose: ANSI CL only
/// requires an implementation to accept 20, and no program in this project
/// returns more than a handful.  Every continuation payload in both closure
/// backends is a @ref value_list of exactly this capacity, so raising it
/// costs stack in every recursive evaluator frame.
inline constexpr int default_max_values = 8;

/// The n-ary payload a form evaluates to (step L20).
///
/// Common Lisp's evaluation relation does not produce *a* value; it produces
/// a possibly-empty sequence of values, of which single-value contexts take
/// the first.  This is that sequence, and it is the type both closure
/// backends' continuations take: `eval_direct_values` returns one, and a CPS
/// continuation is invoked with one.  The @ref value-returning
/// `eval_direct`/`apply_function_value` overloads are thin adapters over the
/// n-ary primitives that call @ref primary_value on the result.
///
/// The alternative -- a side "values register" hung off the environment
/// arena, which is how many real implementations do it -- was rejected for
/// this project: a register makes "who last wrote the register" a global
/// invariant that every arm of every backend has to maintain by hand, and
/// the three backends here have three different control-flow shapes.  An
/// n-ary return channel makes the same information travel exactly where the
/// value already travels, so no arm can forget it.
///
/// @tparam Core      The core AST type.
/// @tparam MaxValues Maximum number of values (see @ref default_max_values).
template <typename Core, int MaxValues = default_max_values>
using value_list =
    smd::smdscheme::foundation::static_vector<value<Core>, MaxValues>;

/// Returns the **primary value** of @p vs: its first element, or `nil` when
/// @p vs is empty.
///
/// This is ANSI CL's rule for reading a multiple-value result in a
/// single-value context, in one place: `(values)` returns zero values, and a
/// single-value context that receives zero values sees `nil`.  Every place
/// either backend narrows an n-ary result to one value routes through here,
/// the same way every truthiness test routes through @ref is_true.
template <typename Core, int MaxValues>
[[nodiscard]] constexpr auto primary_value(
    smd::smdscheme::foundation::static_vector<value<Core>, MaxValues> const &vs)
    -> value<Core> {
    return vs.empty() ? value<Core>{nil_t{}} : vs[0];
}

/// Returns a one-element @ref value_list holding @p v.
///
/// The overwhelmingly common case: every form that is not `values` (or a
/// call to something that ends in one) returns exactly one value.
///
/// @tparam MaxValues Capacity of the returned list; defaults to
///                    @ref default_max_values.
template <int MaxValues = default_max_values, typename Core>
[[nodiscard]] constexpr auto single_value(value<Core> v)
    -> value_list<Core, MaxValues> {
    value_list<Core, MaxValues> vs{};
    vs.push_back(std::move(v));
    return vs;
}
// a4a90af9-fd64-4f75-ad47-609c7c1e50fd end

/// Fixed capacity of the mutable variable @ref store shared by an @ref env.
inline constexpr int default_max_store = 256;

/// A flat, constexpr, allocation-free store of mutable value cells,
/// addressed by stable integer locations.
///
/// Adapted by copy from `smd::smdscheme::closure::store` (PR #23, the
/// landed `set!` step) as the basis for `setq` (step L12, decision:
/// `docs/cl-pivot-plan.md` step L12 explicitly names this class as the
/// machinery to adapt). @ref smd::smdscheme::foundation::static_vector is
/// array-backed, so a location never moves once allocated: an @ref env
/// binding can hold a bare stable index instead of a pointer. A @ref store
/// is shared (by non-owning pointer) across every @ref env copy --
/// including closure captures made through @ref env_arena -- so `setq` on
/// a variable is visible to every closure sharing that binding, not just
/// whichever copy performed the mutation. The store must outlive every
/// environment that names a location in it (it lives for one program
/// evaluation, exactly like @ref pair_heap).
///
/// @tparam Core     The core AST type.
/// @tparam MaxStore Maximum number of mutable cells.
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

    /// Overwrites the cell at @p loc with @p v (the `setq` primitive).
    constexpr auto set(int loc, value<Core> v) -> void {
        cells_[loc] = std::move(v);
    }

  private:
    smd::smdscheme::foundation::static_vector<value<Core>, MaxStore> cells_{};
};

/// Fixed capacity of the @ref pair_heap shared by an environment.
inline constexpr int default_max_pairs = 256;

/// One cons cell: a `car` and a `cdr`, each an arbitrary @ref value.
///
/// Adapted by copy from `smd::smdscheme::closure::pair_cell` (PR #26).
///
/// @tparam Core The core AST type.
template <typename Core>
struct pair_cell {
    value<Core> car{};
    value<Core> cdr{};
};

/// A flat, constexpr, allocation-free heap of cons cells, addressed by
/// stable integer locations (@ref pair_ref).
///
/// Adapted by copy from `smd::smdscheme::closure::pair_heap` (PR #26):
/// @ref smd::smdscheme::foundation::static_vector is array-backed, so
/// locations never move once allocated, which is what lets @ref pair_ref
/// hold a bare stable index instead of a pointer.  The heap is shared (by
/// non-owning pointer) across every environment that needs it; it must
/// outlive every @ref pair_ref it hands out.
///
/// @tparam Core     The core AST type.
/// @tparam MaxPairs Maximum number of cons cells.
template <typename Core, int MaxPairs>
class pair_heap {
  public:
    /// Appends @p c and returns its stable location.
    constexpr auto alloc(pair_cell<Core> c) -> int {
        int loc = cells_.size();
        cells_.push_back(std::move(c));
        return loc;
    }

    /// Returns the cell at @p loc (read-only).
    [[nodiscard]] constexpr auto get(int loc) const -> pair_cell<Core> const & {
        return cells_[loc];
    }

    /// Returns the cell at @p loc for mutation.
    [[nodiscard]] constexpr auto at(int loc) -> pair_cell<Core> & {
        return cells_[loc];
    }

  private:
    smd::smdscheme::foundation::static_vector<pair_cell<Core>, MaxPairs>
        cells_{};
};

/// The sole truthiness authority for the whole project (decision D3).
///
/// `nil` is the only false value; every other value, including integer `0`
/// and every keyword, is true.  Every later `if` site (elaborator,
/// evaluators, CPS/sender backends) must route through this function rather
/// than re-encoding truthiness locally.
///
/// @tparam Core The core AST type.
// 82728208-0712-4e1a-9252-036e99276919
template <typename Core>
[[nodiscard]] constexpr auto is_true(value<Core> const &v) -> bool {
    return !std::holds_alternative<nil_t>(v);
}
// 82728208-0712-4e1a-9252-036e99276919 end

} // namespace smd::smdlisp::closure

#endif
