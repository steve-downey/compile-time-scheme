// src/smd/smdscheme/foundation/static_vector.hpp               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_FOUNDATION_STATIC_VECTOR_HPP
#define SRC_SMD_SMDSCHEME_FOUNDATION_STATIC_VECTOR_HPP

#include <array>
#include <cassert>

namespace smd::smdscheme::foundation {

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
    constexpr static_vector() = default;

    /// Appends @p value to the end.
    /// @pre size() < Capacity
    constexpr auto push_back(T value) -> void;

    /// Returns the current number of elements.
    [[nodiscard]] constexpr auto size() const -> int;

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

    friend constexpr auto operator==(static_vector const &lhs,
                                     static_vector const &rhs) -> bool {
        if (lhs.size_ != rhs.size_)
            return false;
        for (int i = 0; i < lhs.size_; ++i)
            if (lhs.storage_[i] != rhs.storage_[i])
                return false;
        return true;
    }

  private:
    std::array<T, Capacity> storage_{};
    int size_{};
};

template <class T, int Capacity>
constexpr auto static_vector<T, Capacity>::push_back(T value) -> void {
    assert(size_ < Capacity);
    storage_[size_] = std::move(value);
    ++size_;
}

template <class T, int Capacity>
constexpr auto static_vector<T, Capacity>::size() const -> int {
    return size_;
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

} // namespace smd::smdscheme::foundation

#endif
