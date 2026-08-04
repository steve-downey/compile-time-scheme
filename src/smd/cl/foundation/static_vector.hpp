// src/smd/cl/foundation/static_vector.hpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Reviewed union of the two prior copies of this component:
// src/smd/smdscheme/foundation/static_vector.hpp (compile-time-scheme) and
// src/smd/forth/foundation/static_vector.hpp (compile-time-forth, which
// contributes capacity()). Equality is std::equal rather than the raw index
// loop both copies carried.
#ifndef SRC_SMD_CL_FOUNDATION_STATIC_VECTOR_HPP
#define SRC_SMD_CL_FOUNDATION_STATIC_VECTOR_HPP

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <ranges>
#include <utility>

namespace smd::cl::foundation {

/// A fixed-capacity vector with inline storage, usable in constexpr contexts.
///
/// Unlike @c std::vector, @c static_vector never allocates heap memory.
/// The capacity is a compile-time constant, so the type is trivially
/// constexpr-friendly. Pushing beyond capacity is a precondition violation
/// (asserted in debug mode).
///
/// @tparam T        Element type.
/// @tparam Capacity Maximum number of elements.
template <class T, int Capacity>
class static_vector {
  public:
    /// The element type, named for element-generic code.
    using value_type = T;

    constexpr static_vector() = default;

    /// Returns @p count copies of @p value — the whole of "a vector of
    /// @p count somethings", so a caller never spells that as a loop or as
    /// a @c for_each over an index range it then ignores.
    /// @pre 0 <= count <= Capacity
    [[nodiscard]] static constexpr auto filled(int count, T value)
        -> static_vector;

    /// Appends @p value to the end.
    /// @pre size() < Capacity
    constexpr auto push_back(T value) -> void;

    /// Appends the elements of @p range, in order.
    /// @pre size() + the range's length <= Capacity
    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_reference_t<R>, T>
    constexpr auto append_range(R &&range) -> void;

    /// Removes the last element.
    ///
    /// Added in step R5 for the evaluator's continuation stack, which is the
    /// first client that pops: every earlier client only grew.
    /// @pre !empty()
    constexpr auto pop_back() -> void;

    /// Returns the current number of elements.
    [[nodiscard]] constexpr auto size() const -> int;

    /// Returns the fixed capacity (@p Capacity) — a caller-visible way to
    /// check "is this vector full" (`size() >= capacity()`) before a
    /// `push_back` that must not exceed it, without needing @p Capacity
    /// named separately at the call site.
    [[nodiscard]] constexpr auto capacity() const -> int;

    /// Returns true if the vector contains no elements.
    [[nodiscard]] constexpr auto empty() const -> bool;

    /// Returns a reference to the element at @p index.
    /// @pre 0 <= index < size()
    [[nodiscard]] constexpr auto operator[](int index) -> T &;

    /// Returns a const reference to the element at @p index.
    /// @pre 0 <= index < size()
    [[nodiscard]] constexpr auto operator[](int index) const -> T const &;

    constexpr auto begin() -> T * { return storage_.data(); }
    constexpr auto begin() const -> const T * { return storage_.data(); }
    constexpr auto end() -> T * { return storage_.data() + size_; }
    constexpr auto end() const -> const T * { return storage_.data() + size_; }

    // HIDDEN FRIEND
    friend constexpr auto operator==(static_vector const &lhs,
                                     static_vector const &rhs) -> bool {
        return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
    }

  private:
    std::array<T, Capacity> storage_{};
    int size_{};
};

template <class T, int Capacity>
constexpr auto static_vector<T, Capacity>::filled(int count, T value)
    -> static_vector {
    assert(count >= 0 && count <= Capacity);
    static_vector result;
    result.size_ = count;
    std::ranges::fill(result, std::move(value));
    return result;
}

template <class T, int Capacity>
constexpr auto static_vector<T, Capacity>::push_back(T value) -> void {
    assert(size_ < Capacity);
    storage_[size_] = std::move(value);
    ++size_;
}

template <class T, int Capacity>
template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_reference_t<R>, T>
constexpr auto static_vector<T, Capacity>::append_range(R &&range) -> void {
    for (auto &&element : range) { // substrate generic algorithm
        push_back(std::forward<decltype(element)>(element));
    }
}

template <class T, int Capacity>
constexpr auto static_vector<T, Capacity>::pop_back() -> void {
    assert(size_ > 0);
    --size_;
}

template <class T, int Capacity>
constexpr auto static_vector<T, Capacity>::size() const -> int {
    return size_;
}

template <class T, int Capacity>
constexpr auto static_vector<T, Capacity>::capacity() const -> int {
    return Capacity;
}

template <class T, int Capacity>
constexpr auto static_vector<T, Capacity>::empty() const -> bool {
    return size_ == 0;
}

template <class T, int Capacity>
constexpr auto static_vector<T, Capacity>::operator[](int index) -> T & {
    assert(index >= 0 && index < size_);
    return storage_[index];
}

template <class T, int Capacity>
constexpr auto static_vector<T, Capacity>::operator[](int index) const
    -> T const & {
    assert(index >= 0 && index < size_);
    return storage_[index];
}

} // namespace smd::cl::foundation

#endif
