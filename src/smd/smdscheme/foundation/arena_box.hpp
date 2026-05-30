// src/smd/smdscheme/foundation/arena_box.hpp                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_FOUNDATION_ARENA_BOX_HPP
#define SRC_SMD_SMDSCHEME_FOUNDATION_ARENA_BOX_HPP

#include <smd/smdscheme/foundation/static_vector.hpp>
#include <utility>

namespace smd::smdscheme::foundation {

/// A typed integer handle into a @ref tree_arena.
///
/// Stores the index of a node rather than a pointer, keeping the tree
/// relocatable and constexpr-friendly. The sentinel value @c -1 means
/// "null" and converts to @c false.
///
/// @tparam T        The element type in the arena.
/// @tparam MaxNodes Capacity of the backing arena (disambiguates handle types).
template <typename T, int MaxNodes = 1024>
struct arena_box {
    int id_{-1}; ///< Index into the arena, or -1 for the null handle.

    constexpr arena_box() = default;

    /// Constructs a handle for the element at @p id.
    constexpr explicit arena_box(int id) : id_(id) {}

    /// Returns false when this is the null handle.
    constexpr explicit operator bool() const { return id_ != -1; }
};

/// A bump-allocator arena of @p T with fixed capacity, usable in constexpr.
///
/// Nodes are appended in order; allocation never moves existing nodes.
/// Access by raw integer index or by @ref arena_box handle.
///
/// @tparam T        Element type stored in the arena.
/// @tparam MaxNodes Maximum number of elements.
template <typename T, int MaxNodes>
struct tree_arena {
    static_vector<T, MaxNodes> data{};

    constexpr tree_arena() = default;

    /// Appends @p value and returns its integer index.
    constexpr auto allocate(T value) -> int {
        int id = data.size();
        data.push_back(std::move(value));
        return id;
    }

    /// Returns a reference to the element at @p id.
    constexpr auto get(int id) -> T & { return data[id]; }

    /// Returns a const reference to the element at @p id.
    constexpr auto get(int id) const -> const T & { return data[id]; }

    /// Returns a reference to the element referred to by @p b.
    constexpr auto get(arena_box<T, MaxNodes> b) -> T & { return data[b.id_]; }

    /// Returns a const reference to the element referred to by @p b.
    constexpr auto get(arena_box<T, MaxNodes> b) const -> const T & {
        return data[b.id_];
    }
};

/// Constructs a @p T in-place in @p arena and returns a handle to it.
///
/// @tparam T        Element type.
/// @tparam MaxNodes Arena capacity.
/// @tparam Args     Constructor argument types.
/// @param  arena    The arena to allocate into.
/// @param  args     Constructor arguments forwarded to @p T.
/// @return An @ref arena_box handle to the newly allocated element.
template <typename T, int MaxNodes, typename... Args>
constexpr auto make_arena_box(tree_arena<T, MaxNodes> &arena, Args &&...args)
    -> arena_box<T, MaxNodes> {
    return arena_box<T, MaxNodes>(
        arena.allocate(T(std::forward<Args>(args)...)));
}

} // namespace smd::smdscheme::foundation

#endif
