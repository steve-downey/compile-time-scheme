// src/smd/smdscheme/foundation/arena_box.hpp                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_FOUNDATION_ARENA_BOX_HPP
#define SRC_SMD_SMDSCHEME_FOUNDATION_ARENA_BOX_HPP

#include <smd/smdscheme/foundation/static_vector.hpp>
#include <utility>

namespace smd::smdscheme::foundation {

template <typename T, int MaxNodes = 1024>
struct arena_box {
    int id_{-1};

    constexpr arena_box() = default;
    constexpr explicit arena_box(int id) : id_(id) {}

    constexpr explicit operator bool() const { return id_ != -1; }
};

template <typename T, int MaxNodes>
struct tree_arena {
    static_vector<T, MaxNodes> data{};

    constexpr tree_arena() = default;

    constexpr auto allocate(T value) -> int {
        int id = data.size();
        data.push_back(std::move(value));
        return id;
    }

    constexpr auto get(int id) -> T & { return data[id]; }
    constexpr auto get(int id) const -> const T & { return data[id]; }
    constexpr auto get(arena_box<T, MaxNodes> b) -> T & { return data[b.id_]; }
    constexpr auto get(arena_box<T, MaxNodes> b) const -> const T & {
        return data[b.id_];
    }
};

template <typename T, int MaxNodes, typename... Args>
constexpr auto make_arena_box(tree_arena<T, MaxNodes> &arena, Args &&...args)
    -> arena_box<T, MaxNodes> {
    return arena_box<T, MaxNodes>(
        arena.allocate(T(std::forward<Args>(args)...)));
}

} // namespace smd::smdscheme::foundation

#endif
