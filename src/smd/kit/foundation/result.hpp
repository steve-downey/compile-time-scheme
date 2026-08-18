// src/smd/kit/foundation/result.hpp                                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Extracted in step R8 from smd::cl::foundation::result, itself the reviewed
// union of two independently-drifted copies:
// src/smd/smdscheme/foundation/result.hpp (compile-time-scheme) and
// src/smd/forth/foundation/result.hpp (compile-time-forth). The cl rebuild
// added the value_type alias, equality, and and_then below; all three are
// generic over any result<T> rather than Lisp-specific, so they travel with
// the extraction. and_then's own doc comment still narrates the elaborator
// decision (D15, docs/cl-rebuild-plan.md) that motivated writing it, because
// that history is worth keeping, not because the function needs a Lisp.
#ifndef SRC_SMD_KIT_FOUNDATION_RESULT_HPP
#define SRC_SMD_KIT_FOUNDATION_RESULT_HPP

#include <smd/kit/foundation/parse_error.hpp>

#include <utility>
#include <variant>

namespace smd::kit::foundation {

// 6dc68d44-18f5-450d-98f2-e7833f38f0aa
/// A discriminated union of a success value @p T or a @ref parse_error.
///
/// Used throughout the parsing and elaboration pipelines as the
/// error-propagation type. Callers must check @ref has_value before accessing
/// @ref value or @ref error.
///
/// @tparam T The success type.
template <class T>
class result {
  public:
    /// The carried success type, named for effect-generic code such as
    /// @c traverse instances.
    using value_type = T;

    /// Constructs a successful result holding @p value.
    constexpr result(T value);

    /// Constructs a failed result holding @p error.
    constexpr result(parse_error error);

    /// Returns true if this result holds a value rather than an error.
    [[nodiscard]] constexpr auto has_value() const -> bool;

    /// Returns the contained success value.
    /// @pre has_value() == true
    [[nodiscard]] constexpr auto value() const -> T const &;

    /// Returns the contained error.
    /// @pre has_value() == false
    [[nodiscard]] constexpr auto error() const
        -> foundation::parse_error const &;

    // HIDDEN FRIEND
    friend constexpr auto operator==(result const &lhs, result const &rhs)
        -> bool = default;

  private:
    std::variant<T, foundation::parse_error> data_;
};

template <class T>
constexpr result<T>::result(T value) : data_{std::move(value)} {}

template <class T>
constexpr result<T>::result(parse_error error) : data_{error} {}

template <class T>
constexpr auto result<T>::has_value() const -> bool {
    return std::holds_alternative<T>(data_);
}

template <class T>
constexpr auto result<T>::value() const -> T const & {
    return std::get<T>(data_);
}

template <class T>
constexpr auto result<T>::error() const -> foundation::parse_error const & {
    return std::get<foundation::parse_error>(data_);
}

// Monadic sequencing for result — spelled and_then here since R3, and still
// spelled that way — is now the Monad instance's bind, and and_then a
// forwarder to it, both in <smd/kit/foundation/result_instances.hpp>. It
// began as the reader's private detail::and_then and was promoted to the
// substrate when the elaborator needed the same shape; what moved this time
// is only which file holds it, because a datatype does not carry its own
// typeclass adaptation (docs/CODING_RULES.md, Typeclass Design).
//
// Callers spelling and_then need that header rather than this one. Nothing
// else changed for them: and_then is still a function template, so argument
// -dependent lookup still finds it from a result argument.
// 6dc68d44-18f5-450d-98f2-e7833f38f0aa end

} // namespace smd::kit::foundation

#endif
