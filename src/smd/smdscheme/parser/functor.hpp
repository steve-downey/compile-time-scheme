// src/smd/schemepoc/functor.hpp                                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_PARSER_FUNCTOR_HPP
#define SRC_SMD_SMDSCHEME_PARSER_FUNCTOR_HPP

#include <type_traits>
#include <utility>

namespace smd::smdscheme::parser {

template <class Impl>
struct Functor : protected Impl {
    using Impl::fmap;

    template <class T, class U>
    constexpr auto replace(this auto &&self, T &&value, U &&replacement) {
        return self.fmap([replacement = std::forward<U>(replacement)](
                             const auto &) { return replacement; },
                         std::forward<T>(value));
    }
};

template <class T>
inline constexpr auto functor_typeclass = std::false_type{};

struct fmap_fn {
    template <class F, class T,
              const auto &TC = functor_typeclass<std::remove_cvref_t<T>>>
    constexpr auto operator()(F &&f, T &&value) const {
        using tc_type = std::remove_cvref_t<decltype(TC)>;
        return tc_type{}.fmap(std::forward<F>(f), std::forward<T>(value));
    }
};

inline constexpr fmap_fn fmap{};

} // namespace smd::smdscheme::parser

#endif
