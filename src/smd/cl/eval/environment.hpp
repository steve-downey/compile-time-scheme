// src/smd/cl/eval/environment.hpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_EVAL_ENVIRONMENT_HPP
#define SRC_SMD_CL_EVAL_ENVIRONMENT_HPP

#include <smd/cl/eval/heap.hpp>
#include <smd/cl/eval/value.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/symbol/symbol_id.hpp>

#include <algorithm>
#include <optional>
#include <utility>

namespace smd::cl::eval {

/// The lexical environment of variable bindings.
///
/// An environment is an ordinary Lisp value: an association list of
/// `(symbol . value)` pairs, held in the same @ref heap as everything else.
/// It is not a separate arena with a separate capacity, and it carries no
/// capacity of its own, which is what decision D14 asks of anything a
/// closure captures — the pivot's `env<Core, MaxBindings>` is exactly the
/// shape that made a value's type depend on an environment's size.
///
/// The empty environment is any non-cons value; the machine uses `nil`.
/// Shadowing falls out of the representation: @ref lookup finds the
/// leftmost binding, and @ref extend adds to the left.
namespace environment {

// 8a26a939-6938-4e23-a016-5328e9288d5b
/// Returns the value bound to @p id in @p env, or empty if it is unbound.
///
/// @tparam Heap The @ref heap instantiation holding @p env.
template <class Heap>
[[nodiscard]] constexpr auto lookup(Heap const &store, value const &env,
                                    symbol::symbol_id id)
    -> std::optional<value> {
    auto const bindings = store.list(env);
    auto const binds_id = [&store, id](value const &binding) {
        cons_ref const pair = as_cons(binding);
        return pair.valid() && is_symbol(store.cell(pair).car, id);
    };
    auto const found = std::ranges::find_if(bindings, binds_id);
    if (found == bindings.end()) {
        return std::nullopt;
    }
    return store.cell(as_cons(*found)).cdr;
}

/// Returns @p env extended with @p id bound to @p bound, or diagnoses a
/// full heap.
///
/// @tparam Heap The @ref heap instantiation holding @p env.
template <class Heap>
[[nodiscard]] constexpr auto extend(Heap &store, value const &env,
                                    symbol::symbol_id id, value bound)
    -> foundation::result<value> {
    return foundation::and_then(store.cons(symbol_value(id), std::move(bound)),
                                [&store, &env](value const &binding) {
                                    return store.cons(binding, env);
                                });
}
// 8a26a939-6938-4e23-a016-5328e9288d5b end

} // namespace environment

} // namespace smd::cl::eval

#endif
