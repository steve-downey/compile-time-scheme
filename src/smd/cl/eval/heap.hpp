// src/smd/cl/eval/heap.hpp                                       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_EVAL_HEAP_HPP
#define SRC_SMD_CL_EVAL_HEAP_HPP

#include <smd/cl/eval/value.hpp>
#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/foundation/static_vector.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <string_view>
#include <utility>
#include <variant>

namespace smd::cl::eval {

/// One cons cell.
struct cons_cell {
    value car{}; ///< The first half.
    value cdr{}; ///< The second half.

    // HIDDEN FRIEND
    friend constexpr auto operator==(cons_cell const &, cons_cell const &)
        -> bool = default;
};

/// A function object: a `core_lambda` node plus the environment it closed
/// over.
///
/// The parameter list is not copied here — it lives in the core tree's
/// @c core_lambda tag, which the machine already holds. A closure is
/// therefore two ints' worth of state plus its environment, and the
/// question "what are this function's parameters?" has exactly one answer,
/// in exactly one place.
struct closure {
    int lambda_node = -1; ///< The `core_lambda` node index.
    value env{};          ///< The captured environment.

    // HIDDEN FRIEND
    friend constexpr auto operator==(closure const &, closure const &)
        -> bool = default;
};

/// Storage for every object a running program builds: cons cells, string
/// characters, and closures.
///
/// Allocation is diagnosed, never asserted: a program that is merely too
/// big for the arena gets a @ref foundation::parse_error, the discipline
/// the elaborator's @c add_leaf_checked established. There is no reclaim —
/// this is an arena, and a compile-time evaluation is short.
///
/// The capacities are template parameters because the heap is storage;
/// nothing that flows through a program carries one (decision D14).
///
/// @tparam MaxCells       Maximum cons cells.
/// @tparam MaxStringChars Maximum characters across all string objects.
/// @tparam MaxClosures    Maximum closures.
// 6a2fbb0b-6b7b-4b0e-9cb9-1a25f9f6a3d2
template <int MaxCells, int MaxStringChars, int MaxClosures>
class heap {
  public:
    constexpr heap() = default;

    /// Allocates a cell holding @p car and @p cdr, or diagnoses a full heap.
    [[nodiscard]] constexpr auto cons(value car, value cdr)
        -> foundation::result<value>;

    /// Returns the cell @p ref names.
    /// @pre ref.valid() and ref names a cell of this heap
    [[nodiscard]] constexpr auto cell(cons_ref ref) const -> cons_cell const &;

    /// Copies @p text into the string pool, or diagnoses a full pool.
    [[nodiscard]] constexpr auto make_string(std::string_view text)
        -> foundation::result<value>;

    /// Returns the characters @p ref names, as a view into this heap.
    [[nodiscard]] constexpr auto string(string_ref ref) const
        -> std::string_view;

    /// Allocates a closure over @p lambda_node capturing @p env, or
    /// diagnoses a full heap.
    [[nodiscard]] constexpr auto make_closure(int lambda_node, value env)
        -> foundation::result<closure_ref>;

    /// Returns the closure @p ref names.
    /// @pre ref.valid() and ref names a closure of this heap
    [[nodiscard]] constexpr auto closure_at(closure_ref ref) const
        -> closure const &;

    /// A forward range over the elements of a proper list.
    ///
    /// Iteration stops at the first non-cons tail, so a dotted list yields
    /// its elements and drops its tail, and anything that is not a cons is
    /// an empty range — including `nil`, which is how "the empty list has
    /// no elements" falls out rather than being special-cased. This is the
    /// evaluator's only way to walk a chain: a chain walk written by hand
    /// would be a raw loop, and here the loop is the iterator's increment.
    class list_view;

    /// Returns the elements of the list @p head.
    [[nodiscard]] constexpr auto list(value const &head) const -> list_view;

  private:
    foundation::static_vector<cons_cell, MaxCells> cells_{};
    foundation::static_vector<char, MaxStringChars> chars_{};
    foundation::static_vector<closure, MaxClosures> closures_{};
};
// 6a2fbb0b-6b7b-4b0e-9cb9-1a25f9f6a3d2 end

/// Returns the cell handle @p v names, or the invalid handle if @p v is not
/// a cons.
[[nodiscard]] constexpr auto as_cons(value const &v) -> cons_ref {
    auto const *pair = std::get_if<value_cons>(&v);
    return pair != nullptr ? pair->ref : cons_ref{};
}

// b5e6c9e7-7f4e-4a52-8a3a-2f1d0c7a4b90
template <int MaxCells, int MaxStringChars, int MaxClosures>
class heap<MaxCells, MaxStringChars, MaxClosures>::list_view
    : public std::ranges::view_interface<list_view> {
  public:
    /// Walks the chain one cell at a time.
    class iterator {
      public:
        using iterator_concept = std::forward_iterator_tag;
        using iterator_category = std::forward_iterator_tag;
        using value_type = value;
        using difference_type = std::ptrdiff_t;

        constexpr iterator() = default;

        /// Positions this iterator at the cell @p at of @p owner.
        constexpr iterator(heap const *owner, cons_ref at)
            : heap_{owner}, at_{at} {}

        [[nodiscard]] constexpr auto operator*() const -> value const & {
            return heap_->cell(at_).car;
        }

        constexpr auto operator++() -> iterator & {
            at_ = as_cons(heap_->cell(at_).cdr);
            return *this;
        }

        constexpr auto operator++(int) -> iterator {
            iterator const before = *this;
            ++*this;
            return before;
        }

        // HIDDEN FRIEND
        friend constexpr auto operator==(iterator const &, iterator const &)
            -> bool = default;

        // HIDDEN FRIEND
        friend constexpr auto operator==(iterator const &it,
                                         std::default_sentinel_t) -> bool {
            return !it.at_.valid();
        }

      private:
        heap const *heap_ = nullptr;
        cons_ref at_{};
    };

    constexpr list_view() = default;

    /// Views the list starting at @p at in @p owner.
    constexpr list_view(heap const *owner, cons_ref at) : first_{owner, at} {}

    [[nodiscard]] constexpr auto begin() const -> iterator { return first_; }

    [[nodiscard]] constexpr auto end() const -> std::default_sentinel_t {
        return std::default_sentinel;
    }

  private:
    iterator first_{};
};
// b5e6c9e7-7f4e-4a52-8a3a-2f1d0c7a4b90 end

template <int MaxCells, int MaxStringChars, int MaxClosures>
constexpr auto heap<MaxCells, MaxStringChars, MaxClosures>::cons(value car,
                                                                 value cdr)
    -> foundation::result<value> {
    if (cells_.size() >= cells_.capacity()) {
        return foundation::parse_error{{}, "heap is full"};
    }
    cells_.push_back(cons_cell{std::move(car), std::move(cdr)});
    return value{value_cons{cons_ref{cells_.size() - 1}}};
}

template <int MaxCells, int MaxStringChars, int MaxClosures>
constexpr auto
heap<MaxCells, MaxStringChars, MaxClosures>::cell(cons_ref ref) const
    -> cons_cell const & {
    assert(ref.valid() && ref.index < cells_.size());
    return cells_[ref.index];
}

template <int MaxCells, int MaxStringChars, int MaxClosures>
constexpr auto
heap<MaxCells, MaxStringChars, MaxClosures>::make_string(std::string_view text)
    -> foundation::result<value> {
    if (chars_.size() + static_cast<int>(text.size()) > chars_.capacity()) {
        return foundation::parse_error{{}, "string storage is full"};
    }
    string_ref const ref{chars_.size(), static_cast<int>(text.size())};
    std::ranges::for_each(text, [this](char c) { chars_.push_back(c); });
    return value{value_string{ref}};
}

template <int MaxCells, int MaxStringChars, int MaxClosures>
constexpr auto
heap<MaxCells, MaxStringChars, MaxClosures>::string(string_ref ref) const
    -> std::string_view {
    return std::string_view{chars_.begin() + ref.offset,
                            static_cast<std::size_t>(ref.length)};
}

template <int MaxCells, int MaxStringChars, int MaxClosures>
constexpr auto heap<MaxCells, MaxStringChars, MaxClosures>::make_closure(
    int lambda_node, value env) -> foundation::result<closure_ref> {
    if (closures_.size() >= closures_.capacity()) {
        return foundation::parse_error{{}, "too many closures"};
    }
    closures_.push_back(closure{lambda_node, std::move(env)});
    return closure_ref{closures_.size() - 1};
}

template <int MaxCells, int MaxStringChars, int MaxClosures>
constexpr auto
heap<MaxCells, MaxStringChars, MaxClosures>::closure_at(closure_ref ref) const
    -> closure const & {
    assert(ref.valid() && ref.index < closures_.size());
    return closures_[ref.index];
}

template <int MaxCells, int MaxStringChars, int MaxClosures>
constexpr auto
heap<MaxCells, MaxStringChars, MaxClosures>::list(value const &head) const
    -> list_view {
    return list_view{this, as_cons(head)};
}

} // namespace smd::cl::eval

#endif
