<div class="abstract" id="org6b886b9">
<p>
Scheme is homoiconic.
Code is Data, and Data is Code.
In Phase 3, we build the "Reader", whose sole job is to convert raw strings into the native structural format of Lisp: the nested List, represented as the <code>Datum</code> tree, entirely within C++26 <code>constexpr</code> boundaries.
</p>

</div>


# The Separation of Syntax and Semantics

A common pitfall in compiler design is attempting to parse textual tokens directly into semantic AST nodes. For instance, turning the string `"if"` immediately into an `IfNode` during the first pass. We explicitly resist this temptation.

When the Reader encounters:

```scheme
(if #t 1 2)
```

It does not parse a conditional branch. It simply parses a list containing four items: the symbol `'if`, the boolean true, the integer `1`, and the integer `2`.

This strict isolation simplifies our parsing logic immensely. The parser combinators from Phase 2 are used solely to build structural `Datum` types: `Integer`, `Boolean`, `Symbol`, and `List` (cons cells). The Reader doesn't ask semantic questions; it merely shape-matches text into an S-expression.


# Surviving `constexpr`: The Flat Arena

In a standard C++ implementation, a recursive tree like a List fundamentally relies on heap allocations. Recursive types require pointers—such as `std::unique_ptr<Datum>` or raw ``Datum*~—because a C++ `struct` cannot contain itself by value. As we discussed in Phase 1, dynamic allocations that outlive a single ~constexpr`` evaluation context are forbidden by the C++ standard.

To overcome this, we represent our recursive tree via a **Flat Arena**. An Arena is a statically sized array—specifically a ~static\_vector~—of nodes. Instead of raw memory pointers, nodes refer to their children using integer indexes, often called a *handle* or a *box* identifier.

```cpp
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
```

By abstracting the index into an `arena_box`, our nodes behave semantically like pointers, complete with a null-state equivalent (`id_ == -1`). The backing memory layout is simple and predictable:

```cpp
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
```


## Benefits of the Flat Arena

1.  ****`constexpr` Compliance****: The arena uses fixed internal arrays, making it perfectly valid for returning from `constexpr` contexts intact.
2.  ****Cache Locality****: Trees allocated node-by-node on the heap suffer from memory fragmentation. An arena stores sibling data sequentially in memory, meaning bulk operations across the tree enjoy massive CPU cache-hit benefits. Memory pre-fetchers love arenatized data patterns.
3.  ****Immutability and Trivial Copying****: Copying the entire AST simply means performing a bulk trivial copy of a single array buffer. We completely bypass the cost of tracing deep pointer trees to execute deep copies.


# Open Recursion and `Datum` Formats

In our design, list elements point into the arena via the `arena_box` handle, side-stepping the incomplete type problem in C++.

```cpp
/// A Scheme list datum, e.g. @c (a b c).
///
/// Elements are stored as @ref foundation::arena_box handles into the reader
/// arena, avoiding recursion during the parse phase.
///
/// @tparam R        The recursive element type (the datum fix-point).
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum number of list elements.
template <typename R, int MaxNodes, int MaxList>
struct datum_list {
    foundation::static_vector<foundation::arena_box<R, MaxNodes>, MaxList>
        elements{};
};
```

However, expressing a recursive variant manually requires forward declarations and complex workarounds. A cleaner approach utilized heavily in modern compiler engineering is *Open Recursion* paired with a *Fixed-Point combinator*. By defining a generic factory that takes a recursive self-reference `R` as a type template parameter, our `datum_type` variant only declares exactly one flat layer.

```cpp
/// Factory template that produces the open-recursive functor used by
/// @ref datum_type.
///
/// The @c type member template is the @c std::variant layer for one level of
/// the datum tree; @ref foundation::fix ties the recursion.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
struct datum_f_factory {
    /// The one-layer variant type; @p R is the recursive self-reference.
    template <typename R>
    using type = std::variant<datum_integer, datum_symbol, datum_boolean,
                              datum_list<R, MaxNodes, MaxList>,
                              datum_quote<R, MaxNodes>>;
};

/// The concrete recursive Scheme datum type, formed as the fixed point of
/// @ref datum_f_factory.
///
/// @tparam MaxNodes Maximum tree nodes (arena size).
/// @tparam MaxList  Maximum list elements per node.
template <int MaxNodes, int MaxList>
using datum_type =
    foundation::fix<datum_f_factory<MaxNodes, MaxList>::template type>;
```

The `foundation::fix` helper applies the recursive structural knot for us, tying `datum_type` directly into a closed AST structure without needing explicit cyclic data definitions!


# Conclusion

The Reader cleanly translates human text into the mechanical `Datum` arena. At this stage, our compiler possesses a perfectly parsed, memory-safe, and cache-friendly structural representation of the input string. It does not yet know what the code computes—it only knows precisely how it is shaped.


# References

-   McCarthy, J. (1960). ["Recursive Functions of Symbolic Expressions and Their Computation by Machine, Part I"](https://dl.acm.org/doi/10.1145/367177.367199). /Communications of the ACM, 3/(4), 184–195. The foundational paper outlining the S-expression layout and eval mechanics.
-   Bendersky, M. (2014). ["Using the Y-combinator in C++"](https://eli.thegreenplace.net/2014/06/04/using-the-c-zipper-to-compute-factorial). Eli Bendersky's Website. Contextual reading into fixed-point combinators in modern C++.
-   "Region-based memory management" (Arena Allocation). [Wikipedia](https://en.wikipedia.org/wiki/Region-based_memory_management).
