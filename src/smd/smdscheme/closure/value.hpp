// src/smd/smdscheme/closure/value.hpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_CLOSURE_VALUE_HPP
#define SRC_SMD_SMDSCHEME_CLOSURE_VALUE_HPP

#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/foundation/static_vector.hpp>
#include <smd/smdscheme/reader/datum_type.hpp>

#include <span>
#include <string_view>
#include <variant>

namespace smd::smdscheme::closure {

/// Built-in arithmetic operations supported at the value level.
enum class builtin_op { add, multiply };

/// A built-in operator value (holds the operation kind).
struct builtin {
    builtin_op op;
    friend constexpr auto operator==(builtin, builtin) -> bool = default;
};

/// The Scheme "unspecified" value.
///
/// Returned by side-effecting forms such as @c set! whose result is not meant
/// to be used.  All @c unspecified values compare equal.
struct unspecified {
    friend constexpr auto operator==(unspecified, unspecified)
        -> bool = default;
};

/// The empty list @c '().
///
/// A distinct value type — not @ref unspecified — because the empty list is
/// ordinary data (@c null? tests for it) whereas @ref unspecified is the result
/// of a side effect.  All @c null_t values compare equal.
struct null_t {
    friend constexpr auto operator==(null_t, null_t) -> bool = default;
};

/// A Scheme pair: a non-owning handle (stable location) into a @ref pair_heap.
///
/// A pair is the first value that is recursive, shareable, and mutable, so it
/// cannot be a deep-copy value the way a scalar is: two names must be able to
/// refer to the *same* cell for @c eq? identity and @c set-car! mutation to
/// work.  Following the @ref store model used by @c set!, a pair is therefore
/// an integer index into a shared heap; the @c int also breaks the @c pair ↔
/// @c value type cycle without indirection.  Equality is identity: two
/// @c pair_ref compare equal iff they name the same cell.
struct pair_ref {
    int loc = -1; ///< Stable heap location; -1 means "no pair".
    friend constexpr auto operator==(pair_ref lhs, pair_ref rhs) -> bool {
        return lhs.loc == rhs.loc;
    }
};

// Forward declaration of env to resolve circular dependency
template <typename Core, int MaxBindings>
class env;

/// A copyable, constexpr-compatible owning pointer.
///
/// @c std::unique_ptr cannot be used in constexpr because it has a deleted
/// copy constructor; @c constexpr_box provides deep-copy semantics while
/// still destroying via @c delete.  It is used to break the circular
/// dependency between @ref closure and @ref env.
///
/// @tparam T Pointee type.
template <class T>
struct constexpr_box {
    T *ptr = nullptr; ///< Raw owning pointer; null means "no value".

    constexpr constexpr_box() = default;

    /// Takes ownership of @p p.
    constexpr explicit constexpr_box(T *p) : ptr(p) {}

    /// Deep-copies @p other's pointee.
    constexpr constexpr_box(constexpr_box const &other) {
        if (other.ptr)
            ptr = new T(*other.ptr);
    }

    constexpr constexpr_box(constexpr_box &&other) noexcept {
        ptr = other.ptr;
        other.ptr = nullptr;
    }

    constexpr auto operator=(constexpr_box const &other) -> constexpr_box & {
        if (this == &other)
            return *this;
        delete ptr;
        if (other.ptr)
            ptr = new T(*other.ptr);
        else
            ptr = nullptr;
        return *this;
    }

    constexpr auto operator=(constexpr_box &&other) noexcept
        -> constexpr_box & {
        delete ptr;
        ptr = other.ptr;
        other.ptr = nullptr;
        return *this;
    }

    constexpr ~constexpr_box() { delete ptr; }

    /// Dereferences the pointer.
    constexpr auto operator*() const -> T & { return *ptr; }

    /// Pointer access.
    constexpr auto operator->() const -> T * { return ptr; }

    /// Returns true when the pointer is non-null.
    constexpr explicit operator bool() const { return ptr != nullptr; }

    /// Returns the raw pointer without transferring ownership.
    constexpr auto get() const -> T * { return ptr; }
};

/// A first-class Scheme closure: a lambda node paired with a captured
/// environment.
///
/// @c node points into the core arena; it is non-owning.  @c captured is an
/// owned copy of the lexical environment at the time of closure creation.
///
/// @tparam Core The core AST type.
// 8335a467-f7e9-4d18-8d5b-54931b1d5c18
template <typename Core>
struct closure {
    Core const
        *node; ///< Non-owning pointer to the lambda node in the core arena.
    constexpr_box<env<Core, 16>> captured; ///< Captured lexical environment.

    friend constexpr auto operator==(closure<Core> const &lhs,
                                     closure<Core> const &rhs) -> bool {
        // Simple structural equality for test purposes.
        return lhs.node == rhs.node;
    }
};
// 8335a467-f7e9-4d18-8d5b-54931b1d5c18 end

/// An interned Scheme symbol at runtime (not a compile-time identifier).
struct symbol {
    std::string_view name;
    friend constexpr auto operator==(symbol const &lhs, symbol const &rhs)
        -> bool {
        return lhs.name == rhs.name;
    }
};

/// A foreign (C++) function callable from Scheme.
///
/// @c fn is a function pointer with signature @c result<value>(span<value
/// const>). This lets test harnesses and extension points install native
/// callbacks without defining new core forms.
///
/// @tparam Core The core AST type.
template <typename Core>
struct foreign_function {
    using val_t = std::variant<int, bool, builtin, closure<Core>, symbol,
                               foreign_function, unspecified, pair_ref, null_t>;
    using sig_t = foundation::result<val_t> (*)(std::span<val_t const>);

    sig_t fn; ///< Pointer to the native implementation.

    friend constexpr auto operator==(foreign_function const &lhs,
                                     foreign_function const &rhs) -> bool {
        return lhs.fn == rhs.fn;
    }
};

/// The runtime Scheme value type: integer, boolean, builtin, closure,
/// symbol, foreign function, the unspecified value, a pair handle, or the
/// empty list.
///
/// @tparam Core The core AST type (used to type @ref closure and
///              @ref foreign_function).
// 7251dd2e-066e-4c1e-8360-c33b15301355
template <typename Core>
using value =
    std::variant<int, bool, builtin, closure<Core>, symbol,
                 foreign_function<Core>, unspecified, pair_ref, null_t>;
// 7251dd2e-066e-4c1e-8360-c33b15301355 end

/// Fixed capacity of the mutable value @ref store shared by an @ref env.
inline constexpr int default_max_store = 256;

/// A flat, constexpr, allocation-free value store: an array of mutable cells
/// addressed by stable integer locations.
///
/// Locations never move: @ref foundation::static_vector is array-backed, so it
/// never reallocates and @ref alloc only ever appends.  A @ref store is shared
/// (by non-owning pointer) across every @ref env copy — including closure
/// captures — so that @c set! on a captured variable is visible to every
/// closure that shares the binding.  The store must outlive all envs that point
/// at it (it lives for one program evaluation).
///
/// @tparam Core     The core AST type.
/// @tparam MaxStore Maximum number of cells.
template <typename Core, int MaxStore>
class store {
  public:
    /// Appends @p v and returns its stable location.
    constexpr auto alloc(value<Core> v) -> int {
        int loc = cells_.size();
        cells_.push_back(std::move(v));
        return loc;
    }

    /// Returns the value at @p loc.
    [[nodiscard]] constexpr auto get(int loc) const -> value<Core> const & {
        return cells_[loc];
    }

    /// Overwrites the cell at @p loc with @p v.
    constexpr auto set(int loc, value<Core> v) -> void {
        cells_[loc] = std::move(v);
    }

  private:
    foundation::static_vector<value<Core>, MaxStore> cells_{};
};

/// Fixed capacity of the @ref pair_heap shared by an @ref env.
inline constexpr int default_max_pairs = 256;

/// One cons cell: a @c car and a @c cdr, each an arbitrary @ref value.
///
/// @tparam Core The core AST type.
template <typename Core>
struct pair_cell {
    value<Core> car{};
    value<Core> cdr{};
};

/// A flat, constexpr, allocation-free heap of cons cells, addressed by stable
/// integer locations (@ref pair_ref).
///
/// The same design as @ref store: @ref foundation::static_vector is
/// array-backed so locations never move, and the heap is shared (by non-owning
/// pointer) across every @ref env copy — including closure captures — so that a
/// @c set-car! on a pair is visible through every handle that names it.  The
/// heap must outlive all envs that point at it (it lives for one program
/// evaluation).  In a constant evaluation it is a transient local; at runtime
/// it is caller-owned.  A pair handle therefore cannot escape the evaluation
/// that built the heap.
///
/// @tparam Core     The core AST type.
/// @tparam MaxPairs Maximum number of cons cells.
template <typename Core, int MaxPairs>
class pair_heap {
  public:
    /// Appends @p c and returns its stable location.
    constexpr auto alloc(pair_cell<Core> c) -> int {
        int loc = cells_.size();
        cells_.push_back(std::move(c));
        return loc;
    }

    /// Returns the cell at @p loc (read-only).
    [[nodiscard]] constexpr auto get(int loc) const -> pair_cell<Core> const & {
        return cells_[loc];
    }

    /// Returns the cell at @p loc for mutation (@c set-car! / @c set-cdr!).
    [[nodiscard]] constexpr auto at(int loc) -> pair_cell<Core> & {
        return cells_[loc];
    }

  private:
    foundation::static_vector<pair_cell<Core>, MaxPairs> cells_{};
};

/// Structural (deep) equality — the @c equal? predicate.
///
/// Pairs are compared by walking the shared @p heap: two @ref pair_ref are
/// @c equal? iff their cars and cdrs are recursively @c equal?.  Every other
/// value type compares by its own @c operator== (which, for @ref pair_ref
/// alone, is identity — hence the special case here).
///
/// @tparam Core     The core AST type.
/// @tparam MaxPairs Heap capacity.
template <typename Core, int MaxPairs>
[[nodiscard]] constexpr auto values_equal(value<Core> const &a,
                                          value<Core> const &b,
                                          pair_heap<Core, MaxPairs> const &heap)
    -> bool {
    if (a.index() != b.index())
        return false;
    if (std::holds_alternative<pair_ref>(a)) {
        auto const &pa = heap.get(std::get<pair_ref>(a).loc);
        auto const &pb = heap.get(std::get<pair_ref>(b).loc);
        return values_equal(pa.car, pb.car, heap) &&
               values_equal(pa.cdr, pb.cdr, heap);
    }
    return a == b;
}

/// A variable binding environment with linear search and most-recent-first
/// shadowing.
///
/// Uses a @ref foundation::static_vector to remain constexpr and
/// allocation-free.
/// @c lookup searches from the most recently defined binding toward the oldest,
/// so later @c define calls shadow earlier ones.
///
/// An @c env operates in one of two modes:
///  - *Functional* (no @ref store): each binding holds its value inline.  This
///    is the historical, purely-functional behaviour; @c set! is not supported.
///  - *Mutable* (constructed with a @ref store): each binding names a location
///    in the shared store.  Because copying an @c env copies only the (shared,
///    non-owning) store pointer plus the name→location map, @c set! on a
///    captured variable is visible across every closure that shares it.
///
/// @tparam Core        The core AST type.
/// @tparam MaxBindings Maximum number of bindings.
template <typename Core, int MaxBindings>
class env {
  public:
    constexpr env() = default;

    /// Constructs a mutable environment backed by @p s.
    constexpr explicit env(store<Core, default_max_store> *s) : store_(s) {}

    /// Constructs an environment backed by store @p s and pair heap @p p.
    constexpr env(store<Core, default_max_store> *s,
                  pair_heap<Core, default_max_pairs> *p)
        : store_(s), pairs_(p) {}

    /// Returns the shared pair heap (non-owning; null if this environment has
    /// none).  Pair primitives allocate and mutate cells through it.
    [[nodiscard]] constexpr auto pairs() const
        -> pair_heap<Core, default_max_pairs> * {
        return pairs_;
    }

    /// Adds a new binding for @p name → @p val.
    /// Later bindings shadow earlier ones for the same name.
    constexpr auto define(std::string_view name, value<Core> val) -> void;

    /// Looks up @p name, returning its value or an "unbound variable" error.
    [[nodiscard]] constexpr auto lookup(std::string_view name) const
        -> foundation::result<value<Core>>;

    /// Assigns @p val to the existing binding for @p name (the @c set!
    /// operation), returning the @ref unspecified value.  Errors if @p name is
    /// unbound, or if this environment has no backing @ref store.
    [[nodiscard]] constexpr auto assign(std::string_view name,
                                        value<Core> val) const
        -> foundation::result<value<Core>>;

  private:
    struct binding {
        std::string_view name;
        int loc = -1; ///< Store location in mutable mode; -1 otherwise.
        value<Core>
            val{}; ///< Inline value in functional mode; unused if loc>=0.
    };
    store<Core, default_max_store> *store_ =
        nullptr; ///< Non-owning; may be null.
    pair_heap<Core, default_max_pairs> *pairs_ =
        nullptr; ///< Non-owning; may be null.
    foundation::static_vector<binding, MaxBindings> bindings_{};
};

template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::define(std::string_view name,
                                              value<Core> val) -> void {
    if (store_ != nullptr) {
        int loc = store_->alloc(std::move(val));
        bindings_.push_back(binding{name, loc, {}});
    } else {
        bindings_.push_back(binding{name, -1, std::move(val)});
    }
}

template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::lookup(std::string_view name) const
    -> foundation::result<value<Core>> {
    for (int i = bindings_.size() - 1; i >= 0; --i) {
        if (bindings_[i].name == name) {
            if (store_ != nullptr)
                return store_->get(bindings_[i].loc);
            return bindings_[i].val;
        }
    }
    return foundation::parse_error{{}, "unbound variable"};
}

template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::assign(std::string_view name,
                                              value<Core> val) const
    -> foundation::result<value<Core>> {
    if (store_ == nullptr)
        return foundation::parse_error{
            {}, "set!: environment has no mutable store"};
    for (int i = bindings_.size() - 1; i >= 0; --i) {
        if (bindings_[i].name == name) {
            store_->set(bindings_[i].loc, std::move(val));
            return value<Core>{unspecified{}};
        }
    }
    return foundation::parse_error{{}, "set!: unbound variable"};
}

/// Returns a functional environment pre-populated with the built-in @c + and
/// @c * operators.  Does not support @c set! (see the @ref store overload).
///
/// @tparam Core        Core AST type.
/// @tparam MaxBindings Environment capacity.
template <typename Core, int MaxBindings>
[[nodiscard]] constexpr auto default_env() -> env<Core, MaxBindings> {
    env<Core, MaxBindings> e{};
    e.define("+", value<Core>{builtin{builtin_op::add}});
    e.define("*", value<Core>{builtin{builtin_op::multiply}});
    return e;
}

/// Returns a mutable environment backed by @p s and pre-populated with the
/// built-in @c + and @c * operators.  Supports @c set!.
///
/// @p s must outlive the returned environment and every environment copied from
/// it (including closure captures).
///
/// @tparam Core        Core AST type.
/// @tparam MaxBindings Environment capacity.
template <typename Core, int MaxBindings>
[[nodiscard]] constexpr auto default_env(store<Core, default_max_store> &s)
    -> env<Core, MaxBindings> {
    env<Core, MaxBindings> e{&s};
    e.define("+", value<Core>{builtin{builtin_op::add}});
    e.define("*", value<Core>{builtin{builtin_op::multiply}});
    return e;
}

/// Returns an environment backed by store @p s and pair heap @p p,
/// pre-populated with the built-in @c + and @c * operators.  Supports both @c
/// set! (via @p s) and the pair primitives (via @p p).
///
/// Both @p s and @p p must outlive the returned environment and every
/// environment copied from it (including closure captures).
///
/// @tparam Core        Core AST type.
/// @tparam MaxBindings Environment capacity.
template <typename Core, int MaxBindings>
[[nodiscard]] constexpr auto default_env(store<Core, default_max_store> &s,
                                         pair_heap<Core, default_max_pairs> &p)
    -> env<Core, MaxBindings> {
    env<Core, MaxBindings> e{&s, &p};
    e.define("+", value<Core>{builtin{builtin_op::add}});
    e.define("*", value<Core>{builtin{builtin_op::multiply}});
    return e;
}

} // namespace smd::smdscheme::closure

#endif
