<div class="abstract">
<p>
Every structure in this compiler lives in compile-time memory. I establish the
vocabulary types that make this possible: fixed-capacity vectors, a result monad,
an arena-and-handle system for recursive trees, and an owning pointer that
replaces <code>std::indirect</code>.
</p>
</div>

# The Foundation

Before I parse a single S-expression or evaluate a lambda, I need memory
management that works inside the `constexpr` evaluator. The C++ compile-time
environment forbids persistent heap allocation: anything allocated with `new`
must be freed within the same constant evaluation. Every data structure in
SchemePoC must either fit in inline stack storage, or use transient heap
allocations that are fully reclaimed before evaluation completes.

Four vocabulary types carry the entire foundation: `result<T>` for error
propagation, `static_vector` for fixed-capacity inline storage, the
`tree_arena` and `arena_box` pair for arena-based recursive structures, and
`Box<A>` for the owning pointer pattern needed by the fixpoint tree.

## result\<T\>

The parsing pipeline can fail — mismatched parentheses, unexpected end of
input, unrecognised token forms. I need a type that represents either a
successful value or a parse error, and that I can return from `constexpr`
functions without exceptions.

```cpp
// src/smd/smdscheme/foundation/result.hpp
template <class T>
class result {
  public:
    constexpr result(T value);
    constexpr result(parse_error error);

    [[nodiscard]] constexpr auto has_value() const -> bool;
    [[nodiscard]] constexpr auto value() const -> T const &;
    [[nodiscard]] constexpr auto error() const -> foundation::parse_error const &;

  private:
    std::variant<T, foundation::parse_error> data_;
};
```

`result<T>` is a thin wrapper over `std::variant`. A `parse_error` carries
a `source_pos` — line and column — and a static string literal naming what
was expected. Because `message` is a pointer to a string literal, the struct
holds no owned memory and is trivially copyable across evaluation boundaries.

I deliberately chose this narrow design over `std::expected`
[cite:@cppref_expected]. `std::expected`'s richer monadic interface is
appealing, but `result<T>` only needs the two constructors and three
accessors that the parsing pipeline actually calls. The smaller surface keeps
cognitive load low.

## static\_vector\<T, Capacity\>

Every collection in the compiler has a known maximum size at compile time. A
`std::vector` would require heap allocation that persists beyond the
evaluation — illegal in `constexpr`. Instead I use a fixed-capacity inline
vector:

```cpp
// src/smd/smdscheme/foundation/static_vector.hpp
template <class T, int Capacity>
class static_vector {
    std::array<T, Capacity> storage_{};
    int size_{};
  public:
    constexpr auto push_back(T value) -> void;
    [[nodiscard]] constexpr auto size() const -> int;
    [[nodiscard]] constexpr auto operator[](int index) -> T &;
    // ...
};
```

The entire vector lives in the `std::array` member — no heap, no dynamic
allocation. Because all storage is inline, instances can be returned by value
from `constexpr` functions and verified with `static_assert`:

```cpp
// src/smd/smdscheme/foundation/static_vector.test.cpp
constexpr auto make_vec() {
    static_vector<int, 4> xs;
    xs.push_back(1);
    xs.push_back(2);
    return xs;
}

static_assert(make_vec().size() == 2);
static_assert(make_vec()[0] == 1);
static_assert(make_vec()[1] == 2);
```

The capacity is a compile-time constant and pushing beyond it is a
precondition violation. This is intentional: the compiler knows the maximum
arity of any Scheme form, the maximum nesting depth, and the maximum
identifier length. Hard limits make the types simple.

## tree\_arena and arena\_box

The reader produces a recursive tree — lists that contain atoms and other
lists. Representing recursion in `constexpr` without persistent heap
allocation requires indirection that does not use heap pointers.

The solution is an arena-and-handle pattern:

```cpp
// src/smd/smdscheme/foundation/arena_box.hpp
template <typename T, int MaxNodes = 1024>
struct arena_box {
    int id_{-1};
    constexpr explicit operator bool() const { return id_ != -1; }
};

template <typename T, int MaxNodes>
struct tree_arena {
    static_vector<T, MaxNodes> data{};

    constexpr auto allocate(T value) -> int {
        int id = data.size();
        data.push_back(std::move(value));
        return id;
    }

    constexpr auto get(int id) -> T & { return data[id]; }
    constexpr auto get(arena_box<T, MaxNodes> b) -> T & { return data[b.id_]; }
};
```

A `tree_arena<Datum, 1024>` holds up to 1024 datum nodes in a contiguous
`static_vector`. An `arena_box<Datum>` is just an integer — the index of a
node in the arena. Recursive structures embed `arena_box` handles instead of
pointers. The entire tree lives in one flat array, and a datum node's children
are adjacent integer offsets rather than scattered heap addresses.

This pattern is sometimes called region-based memory management
[cite:@wiki2024arena]. The compile-time variant works because the arena itself
is a `static_vector` — inline storage that the constant evaluator can reason
about without tracing heap pointers.

The null handle has `id_ == -1` and converts to `false`, matching the
conventional nullable-pointer idiom.

## Box\<A\>

The arena-and-handle pattern works for the reader's datum tree because all
nodes share the same type. The computation tree is different: `Fix<CompF>`
is a recursive algebraic type whose node variants hold *owning* sub-trees of
the same type. That requires a genuine owning pointer.

I need something with value semantics: deep copy, destructor that frees, and
a nullable default that does not allocate. The standard type for this role is
`std::indirect` [cite:@p3019indirect], but `std::indirect`'s explicit default
constructor blocks its use in aggregate-initialized containers like
`static_vector`. Until that is resolved, I use `Box<A>`:

```cpp
// src/smd/fixpoint/box.hpp
template <typename A>
struct Box {
    A *ptr = nullptr;

    constexpr Box() = default;
    constexpr explicit Box(A *p) : ptr(p) {}

    constexpr Box(Box const &other)
        : ptr(other.ptr ? new A(*other.ptr) : nullptr) {}

    constexpr Box(Box &&other) noexcept
        : ptr(std::exchange(other.ptr, nullptr)) {}

    constexpr ~Box() { delete ptr; }

    constexpr auto operator*() const -> A & { return *ptr; }
    constexpr auto operator->() const -> A * { return ptr; }
};

template <typename A, typename... Args>
constexpr auto make_box(Args &&...args) -> Box<A> {
    return Box<A>(new A(std::forward<Args>(args)...));
}
```

`Box<A>` owns its pointee, copies it deeply, and frees it on destruction. The
raw `new` and `delete` are `constexpr` in C++20 and later, provided the
allocation is *transient*: every object allocated during a constant evaluation
must be freed before that evaluation ends.

## The Constexpr Allocation Model

The key invariant that makes `Box<A>` legal in `constexpr` contexts is
*transient allocation*. C++20 extended constant evaluation to permit dynamic
allocation so long as every `new` within a constant evaluation is matched by a
corresponding `delete` before the evaluation returns. Allocation that escapes —
a heap pointer surviving in the result — remains illegal.

In SchemePoC the intermediate `Fix<CompF>` tree is built from `Box`-bearing
nodes and fully consumed by the evaluator. The evaluator's result type,
`result<value>`, contains only scalar fields — numbers, booleans, and
environment handles — with no heap pointers. When the evaluation completes,
the `Fix<CompF>` tree falls out of scope, its `Box` destructors run, every
`new` is matched by a `delete`, and the result escapes cleanly.

This is why `Box<A>` works: the allocation is real but temporary. The compiler
tracks it, verifies the cleanup, and accepts the result as a compile-time
constant.

# References

P3019R13 (2024). *std::indirect and std::polymorphic*, ISO C++ Committee.

cppreference.com. *std::expected*. <https://en.cppreference.com/w/cpp/utility/expected>

Wikipedia. *Region-based memory management*. <https://en.wikipedia.org/wiki/Region-based_memory_management>
