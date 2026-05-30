// src/smd/smdscheme/sender/string_writer.hpp                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_STRING_WRITER_HPP
#define SRC_SMD_SMDSCHEME_SENDER_STRING_WRITER_HPP

#include <smd/smdscheme/foundation/alternative.hpp>
#include <smd/smdscheme/foundation/applicative.hpp>
#include <smd/smdscheme/foundation/functor.hpp>

#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace smd::smdscheme::sender {

template <typename T>
struct string_writer {
    std::string log;
    T value;

    static auto pure(T val) -> string_writer {
        return string_writer{"", std::move(val)};
    }
};

template <typename F, typename A, typename B>
auto lift_a2(F func, string_writer<A> const &a, string_writer<B> const &b)
    -> string_writer<decltype(func(a.value, b.value))> {
    return {a.log + b.log, func(a.value, b.value)};
}

template <typename T>
auto combine(string_writer<T> const &a, string_writer<T> const &b)
    -> string_writer<T> {
    return {a.log + b.log, b.value};
}

template <typename F, typename A>
auto fmap(F func, string_writer<A> const &w)
    -> string_writer<decltype(func(w.value))> {
    return {w.log, func(w.value)};
}

inline auto tell(std::string msg) -> string_writer<std::monostate> {
    return {std::move(msg), std::monostate{}};
}

// -- Typeclass instances for string_writer --

template <class T>
struct string_writer_functor_impl {
    template <class F>
    constexpr auto fmap(this auto &&, F &&func, string_writer<T> const &w)
        -> string_writer<std::invoke_result_t<F, T const &>> {
        return {w.log, std::invoke(std::forward<F>(func), w.value)};
    }
};

template <class T>
struct string_writer_functor_map
    : foundation::functor<string_writer_functor_impl<T>> {
    using string_writer_functor_impl<T>::fmap;
};

template <class T>
struct string_writer_applicative_impl {
    template <class V>
    constexpr auto pure(this auto &&, V &&val)
        -> string_writer<std::remove_cvref_t<V>> {
        return string_writer<std::remove_cvref_t<V>>{"", std::forward<V>(val)};
    }

    template <class FW, class AW>
    constexpr auto apply(this auto &&, FW &&func_writer, AW &&arg_writer) {
        using result_type = std::invoke_result_t<decltype(func_writer.value),
                                                 decltype(arg_writer.value)>;
        return string_writer<result_type>{
            func_writer.log + arg_writer.log,
            std::invoke(func_writer.value, arg_writer.value)};
    }
};

template <class T>
struct string_writer_applicative_map
    : foundation::applicative<string_writer_applicative_impl<T>> {
    using string_writer_applicative_impl<T>::pure;
    using string_writer_applicative_impl<T>::apply;
};

template <class T>
struct string_writer_alternative_impl {
    constexpr auto empty(this auto &&) -> string_writer<T> {
        return string_writer<T>{"", T{}};
    }

    constexpr auto alt(this auto &&, string_writer<T> const &a,
                       string_writer<T> const &b) -> string_writer<T> {
        return {a.log + b.log, b.value};
    }
};

template <class T>
struct string_writer_alternative_map
    : foundation::alternative<string_writer_alternative_impl<T>> {
    using string_writer_alternative_impl<T>::empty;
    using string_writer_alternative_impl<T>::alt;
};

} // namespace smd::smdscheme::sender

namespace smd::smdscheme::foundation {

template <class T>
inline constexpr auto functor_typeclass<sender::string_writer<T>> =
    sender::string_writer_functor_map<T>{};

template <class T>
inline constexpr auto applicative_typeclass<sender::string_writer<T>> =
    sender::string_writer_applicative_map<T>{};

template <class T>
inline constexpr auto alternative_typeclass<sender::string_writer<T>> =
    sender::string_writer_alternative_map<T>{};

} // namespace smd::smdscheme::foundation

#endif
