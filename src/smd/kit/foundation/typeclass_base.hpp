// src/smd/kit/foundation/typeclass_base.hpp                         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Picked up from beman.transpose's typeclass-instance-grounding work
// (include/beman/transpose/detail/typeclass_base.hpp). The two trees run
// the same typeclass-object pattern -- a CRTP base deriving operations
// from an Impl's primitives, a lookup variable per typeclass, operation
// objects over the lookup -- so the machinery that pattern needs is shared
// by design rather than by accident. Nothing here is specific to one
// typeclass; the six bases in this directory all name it.
#ifndef SRC_SMD_KIT_FOUNDATION_TYPECLASS_BASE_HPP
#define SRC_SMD_KIT_FOUNDATION_TYPECLASS_BASE_HPP

#include <type_traits>

namespace smd::kit::foundation {

/// Always false, but dependent on a template parameter pack.
///
/// A @c static_assert written directly with @c false fires as soon as the
/// enclosing template is parsed, even if the member it guards is never
/// called. Making the condition depend on the pack defers the check to
/// instantiation, so it fires only when someone actually calls the guarded
/// operation — which is what lets a member give a custom "this operation
/// does not exist, here is why" diagnostic instead of a bare "no member"
/// error.
template <class...>
inline constexpr bool always_false_v = false;

/// True when a typeclass lookup variable has been specialized away from its
/// @c std::false_type default — that is, when the type has an instance of
/// that typeclass.
///
/// Pass @c decltype(functor<T>) and friends; the reference and
/// cv-qualification a @c const @c auto @c & NTTP picks up are stripped here.
///
/// The operation objects use this to fail with a diagnostic that names the
/// missing specialization. Without it an unregistered type reaches the
/// operation as @c std::false_type{} and the error is a bare "no member
/// named 'traverse' in 'std::false_type'", pointing at library internals
/// rather than at the specialization the caller has to write.
template <class LookupResult>
inline constexpr bool has_instance_v =
    !std::is_same_v<std::remove_cvref_t<LookupResult>, std::false_type>;

/// The @c Impl sub-object reference a typeclass base's member should
/// address, given the deduced type of its @c self parameter.
///
/// Every derived operation on a typeclass base takes the same shape: probe
/// @c Impl for a native version of the operation, forward to it if it is
/// there, derive otherwise. Both halves address @c Impl rather than @c self,
/// which is what keeps two mutually-derivable operations from recursing into
/// each other. This spells the const propagation that addressing needs,
/// once.
///
/// The cast itself cannot live here. Each base inherits its @c Impl
/// protectedly, so only a member of that base may perform the conversion;
/// each base carries a three-line private @c impl_of that does, and they all
/// name this alias.
///
/// @tparam Impl The primitive-providing implementation type.
/// @tparam Self The deduced type of the calling member's @c self parameter.
template <class Impl, class Self>
using impl_ref_t =
    std::conditional_t<std::is_const_v<std::remove_reference_t<Self>>,
                       const Impl, Impl> &;

/// Trait that extracts the element type from a context — the @c T in a
/// @c result<T,E>, a @c static_vector<T,N>, an @c identity<T>. The primary
/// template uses the nested @c value_type alias, which every context in this
/// kit provides; specialize it for one that cannot.
///
/// Named for what it extracts rather than for one typeclass: the concepts
/// below use it for Functor, Foldable and Traversable as much as for
/// Applicative. (beman.transpose spells the same trait
/// @c applicative_value_t, which is wording-visible there and so cannot be
/// renamed.)
template <class T, class = void>
struct element_type;

template <class T>
struct element_type<T,
                    std::void_t<typename std::remove_cvref_t<T>::value_type>> {
    using type = typename std::remove_cvref_t<T>::value_type;
};

/// Convenience alias for @c element_type<T>::type.
template <class T>
using element_type_t = typename element_type<std::remove_cvref_t<T>>::type;

namespace detail {

/// Representative witness callable for probing a one-argument derived
/// operation that is templated over an arbitrary callable — @c fmap,
/// @c fold_map, @c traverse and similar. A concept can check membership for
/// only one concrete instantiation, never for every possible callable; this
/// is that one instantiation, parameterized by what the probed operation
/// needs the callable to produce. Its presence in a concept is a witness
/// that the operation exists, not a proof that it exists for every callable.
template <class Result>
struct probe_witness {
    template <class Argument>
    constexpr auto operator()(Argument const &) const -> Result {
        return Result{};
    }
};

/// The two-argument counterpart of @ref probe_witness, for probing derived
/// operations templated over an arbitrary binary callable — @c fold_left and
/// @c fold_right's state-combining function, and @c lift_a2. A single
/// callable type cannot serve both roles: the @c invoke derivation curries
/// its callable one argument at a time, and a callable invocable with either
/// one argument or two would be accepted after the first and never reach the
/// second, silently truncating the arity the probe means to exercise.
template <class Result>
struct probe_witness2 {
    template <class First, class Second>
    constexpr auto operator()(First const &, Second const &) const -> Result {
        return Result{};
    }
};

} // namespace detail

} // namespace smd::kit::foundation

#endif
