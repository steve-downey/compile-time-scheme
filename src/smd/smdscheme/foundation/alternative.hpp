// src/smd/smdscheme/foundation/alternative.hpp                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_FOUNDATION_ALTERNATIVE_HPP
#define SRC_SMD_SMDSCHEME_FOUNDATION_ALTERNATIVE_HPP

#include <type_traits>
#include <utility>

namespace smd::smdscheme::foundation {

template <class Impl>
struct alternative : protected Impl {
    using Impl::alt;
    using Impl::empty;

    template <class A, class B>
    constexpr auto combine(this auto &&self, A &&a, B &&b) {
        return self.alt(std::forward<A>(a), std::forward<B>(b));
    }
};

template <class T>
inline constexpr auto alternative_typeclass = std::false_type{};

struct alt_fn {
    template <class A, class B,
              const auto &TC =
                  alternative_typeclass<std::remove_cvref_t<A>>>
    constexpr auto operator()(A &&a, B &&b) const {
        using tc_type = std::remove_cvref_t<decltype(TC)>;
        return tc_type{}.alt(std::forward<A>(a), std::forward<B>(b));
    }
};

inline constexpr alt_fn alt{};

struct empty_fn {
    template <class T,
              const auto &TC = alternative_typeclass<std::remove_cvref_t<T>>>
    constexpr auto operator()() const {
        using tc_type = std::remove_cvref_t<decltype(TC)>;
        return tc_type{}.empty();
    }
};

inline constexpr empty_fn empty{};

} // namespace smd::smdscheme::foundation

#endif
