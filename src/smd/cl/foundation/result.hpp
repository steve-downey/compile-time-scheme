// src/smd/cl/foundation/result.hpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Reviewed union of the two prior copies of this component:
// src/smd/smdscheme/foundation/result.hpp (compile-time-scheme) and
// src/smd/forth/foundation/result.hpp (compile-time-forth).
// New here: the value_type alias and equality, which the typeclass instances
// in <smd/cl/foundation/result_instances.hpp> and their law tests need.
#ifndef SRC_SMD_CL_FOUNDATION_RESULT_HPP
#define SRC_SMD_CL_FOUNDATION_RESULT_HPP

#include <smd/cl/foundation/parse_error.hpp>

#include <variant>

namespace smd::cl::foundation {

/// A discriminated union of a success value @p T or a @ref parse_error.
///
/// Used throughout the parsing and elaboration pipelines as the
/// error-propagation type. Callers must check @ref has_value before accessing
/// @ref value or @ref error.
///
/// Design note (decision D13): the evaluator built in step R5 needs a third
/// alternative — an unwind in flight — as a distinct channel beside value and
/// error. Keep client code on the has_value/value/error accessors rather than
/// on the variant itself, so the alternative set can widen without breaking
/// callers.
///
/// @tparam T The success type.
// 065a04a3-92d8-4537-bc38-f5c19ba9250b
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
// 065a04a3-92d8-4537-bc38-f5c19ba9250b end

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

} // namespace smd::cl::foundation

#endif
