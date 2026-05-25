// src/smd/schemepoc/fix.hpp                                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SCHEMEPOC_FIX_HPP
#define SRC_SMD_SCHEMEPOC_FIX_HPP

#include <utility>

namespace smd::schemepoc {

template <template <class> class F>
class fix {
  public:
    using layer_type = F<fix<F>>;

    constexpr explicit fix(layer_type layer)
        : layer_(new layer_type(std::move(layer))) {}

    constexpr fix(fix const& other) : layer_(new layer_type(*other.layer_)) {}

    constexpr fix(fix&& other) noexcept : layer_(other.layer_) {
        other.layer_ = nullptr;
    }

    constexpr auto operator=(fix other) noexcept -> fix& {
        std::swap(layer_, other.layer_);
        return *this;
    }

    constexpr ~fix() { delete layer_; }

    [[nodiscard]] constexpr auto layer() const -> layer_type const& {
        return *layer_;
    }

  private:
    layer_type* layer_ = nullptr;
};

// fold_fix recursively folds a fix<F> tree using a carrier-algebra.
// R is the carrier type (return type of algebra).
// fmap must be findable via ADL for the specific F<...> layer type.
template <class R, template <class> class F, class Algebra>
constexpr auto fold_fix(fix<F> const& tree, Algebra algebra) -> R {
    auto mapped = fmap(tree.layer(), [&](auto const& child) -> R {
        return fold_fix<R>(child, algebra);
    });
    return algebra(mapped);
}

} // namespace smd::schemepoc

#endif
