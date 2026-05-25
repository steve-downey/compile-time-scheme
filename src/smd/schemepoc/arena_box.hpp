// src/smd/schemepoc/arena_box.hpp                                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SCHEMEPOC_ARENA_BOX_HPP
#define SRC_SMD_SCHEMEPOC_ARENA_BOX_HPP

#include <smd/schemepoc/static_vector.hpp>
#include <utility>

namespace smd::schemepoc {

template <typename T, int MaxNodes>
struct tree_arena {
    static_vector<T, MaxNodes> data{};

    constexpr tree_arena() = default;

    constexpr auto allocate(T value) -> int {
        int id = data.size();
        data.push_back(std::move(value));
        return id;
    }

    constexpr auto get(int id) -> T& { return data[id]; }
    constexpr auto get(int id) const -> const T& { return data[id]; }
};

template <typename T, int MaxNodes = 1024>
struct arena_box {
    tree_arena<T, MaxNodes>* arena_{nullptr};
    int id_{-1};

    constexpr arena_box() = default;
    constexpr arena_box(tree_arena<T, MaxNodes>& arena, T value)
        : arena_(&arena), id_(arena.allocate(std::move(value))) {}

    constexpr auto operator*() -> T& { return arena_->get(id_); }
    constexpr auto operator*() const -> const T& { return arena_->get(id_); }
    constexpr auto operator->() -> T* { return &arena_->get(id_); }
    constexpr auto operator->() const -> const T* { return &arena_->get(id_); }
    
    constexpr explicit operator bool() const { return arena_ != nullptr && id_ != -1; }
};

template <typename T, int MaxNodes, typename... Args>
constexpr auto make_arena_box(tree_arena<T, MaxNodes>& arena, Args&&... args) -> arena_box<T, MaxNodes> {
    return arena_box<T, MaxNodes>(arena, T(std::forward<Args>(args)...));
}

} // namespace smd::schemepoc

#endif
