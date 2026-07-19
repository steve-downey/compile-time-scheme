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

} // namespace smd::smdlisp::closure

#endif
