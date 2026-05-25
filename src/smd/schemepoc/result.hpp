// src/smd/schemepoc/result.hpp                                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SCHEMEPOC_RESULT_HPP
#define SRC_SMD_SCHEMEPOC_RESULT_HPP

#include <smd/schemepoc/source.hpp>

#include <variant>

namespace smd::schemepoc {

template <class T>
class result {
  public:
    constexpr result(T value);
    constexpr result(parse_error error);

    [[nodiscard]] constexpr auto has_value() const -> bool;
    [[nodiscard]] constexpr auto value() const -> T const &;
    [[nodiscard]] constexpr auto error() const -> parse_error const &;

  private:
    std::variant<T, parse_error> data_;
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
constexpr auto result<T>::error() const -> parse_error const & {
    return std::get<parse_error>(data_);
}

} // namespace smd::schemepoc

#endif
