// src/smd/schemepoc/result.hpp                                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_FOUNDATION_RESULT_HPP
#define SRC_SMD_SMDSCHEME_FOUNDATION_RESULT_HPP

#include <smd/smdscheme/foundation/source.hpp>

#include <variant>

namespace smd::smdscheme::foundation {

template <class T>
class result {
  public:
    constexpr result(T value);
    constexpr result(parse_error error);

    [[nodiscard]] constexpr auto has_value() const -> bool;
    [[nodiscard]] constexpr auto value() const -> T const &;
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
