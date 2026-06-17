// src/smd/fixpoint/box.hpp                                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INCLUDED_SMD_FIXPOINT_BOX
#define INCLUDED_SMD_FIXPOINT_BOX

#include <memory>

namespace smd::fixpoint {

/** Indirection type for recursive positions inside F<Fix<F>>.
 * Box<A> = std::indirect<A>: value semantics, deep copy on copy.
 * @tparam A the wrapped type (typically a recursive Fix instantiation)
 */
template <typename A>
using Box = std::indirect<A>;

/** Construct a Box<A>, forwarding @p args to A's constructor. */
template <typename A, typename... Args>
auto make_box(Args &&...args) -> Box<A> {
    return Box<A>(std::forward<Args>(args)...);
}

} // namespace smd::fixpoint

#endif
