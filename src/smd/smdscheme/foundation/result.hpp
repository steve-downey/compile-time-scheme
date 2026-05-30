// src/smd/smdscheme/foundation/result.hpp                      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_FOUNDATION_RESULT_HPP
#define SRC_SMD_SMDSCHEME_FOUNDATION_RESULT_HPP

#include <smd/smdscheme/foundation/parse_error.hpp>

#include <variant>

namespace smd::smdscheme::foundation {

/// A discriminated union of a success value @p T or a @ref parse_error.
///
/// Used throughout the parsing and elaboration pipelines as the
/// error-propagation type. Callers must check @ref has_value before accessing
/// @ref value or
/// @ref error.
///
/// @tparam T The success type.
template <class T>
class result {
  public:
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

} // namespace smd::smdscheme::foundation

#endif
