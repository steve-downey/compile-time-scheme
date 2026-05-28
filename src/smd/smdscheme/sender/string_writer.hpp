// src/smd/smdscheme/sender/string_writer.hpp                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_STRING_WRITER_HPP
#define SRC_SMD_SMDSCHEME_SENDER_STRING_WRITER_HPP

#include <string>
#include <utility>
#include <variant>

namespace smd::smdscheme::sender {

template <typename T>
struct string_writer {
    std::string log;
    T           value;

    static auto pure(T val) -> string_writer {
        return string_writer{"", std::move(val)};
    }
};

template <typename F, typename A, typename B>
auto lift_a2(F func, string_writer<A> const& a, string_writer<B> const& b)
    -> string_writer<decltype(func(a.value, b.value))> {
    return {a.log + b.log, func(a.value, b.value)};
}

template <typename T>
auto combine(string_writer<T> const& a, string_writer<T> const& b)
    -> string_writer<T> {
    return {a.log + b.log, b.value};
}

template <typename F, typename A>
auto fmap(F func, string_writer<A> const& w)
    -> string_writer<decltype(func(w.value))> {
    return {w.log, func(w.value)};
}

inline auto tell(std::string msg) -> string_writer<std::monostate> {
    return {std::move(msg), std::monostate{}};
}

} // namespace smd::smdscheme::sender

#endif
