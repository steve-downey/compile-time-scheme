<div class="abstract" id="org8331983">
<p>
Scheme is homoiconic.
Code is Data, and Data is Code.
In Phase 3, we build the "Reader", whose sole job is to convert raw strings into the native structural format of Lisp: the nested List, represented as the <code>Datum</code> tree, entirely within C++26 <code>constexpr</code> boundaries.
</p>

</div>


# The Separation of Syntax and Semantics

A common pitfall is attempting to parse textual tokens directly into semantic AST nodes (e.g., turning `"if"` immediately into an `IfNode`). We explicitly resist this.

When the Reader sees:

```scheme
(if #t 1 2)
```

It does not parse a conditional branch. It simply parses a list containing four items: the symbol `'if`, the boolean true, the integer `1`, and the integer `2`.

This isolation simplifies our parsing logic immensely. The combinators from Phase 2 are used solely to build structural `Datum` types: `Integer`, `Boolean`, `Symbol`, and `List` (Cons cells). The Reader doesn't ask questions; it just builds the list.


# Surviving `constexpr`: The Flat Arena

In a standard C++ implementation, a recursive tree like a List relies on heap allocations (pointers via `new` or `std::unique_ptr`).

```c++
struct Cons {
    Datum* car;
    Datum* cdr;
};
```

As discussed in Phase 1, dynamic allocations that outlive the `constexpr` evaluation context are forbidden. To overcome this, we represent our recursive tree via a **Flat Arena**.

An Arena is a statically sized array (\`static\_vector\`) of nodes. Instead of raw memory pointers, nodes refer to their children using integer indexes, often called a `node_id`.

```c++
using static_node_id = std::uint32_t;

struct ConsData {
    static_node_id car_id;
    static_node_id cdr_id;
};

// The Arena holds our tree memory:
static_vector<DatumVariant, 1024> datum_arena;
```


## Benefits of the Flat Arena

1.  ****`constexpr` Compliance****: The arena uses fixed internal arrays, making it perfectly valid for returning from `constexpr` functions. 2. ****Cache Locality****: Trees allocated node-by-node on the heap suffer from fragmentation.

An arena stores sibling data sequentially in memory, meaning operations bounding across the tree enjoy massive CPU cache-hit benefits. Memory pre-fetchers love arenas. 3. ****Immutability and Easy Copying****: Copying the entire AST means passing a single array value. We don't have to trace deep pointer trees to perform deep copies. It's just copying a block of memory.


# Conclusion

The Reader cleanly translates human text into the mechanical `Datum` arena. At this stage, our compiler possesses a perfectly parsed, memory-safe structural representation of the input string. It does not yet know what the code does, but it knows exactly how it is shaped.


# References

-   McCarthy, J. (1960). "Recursive Functions of Symbolic Expressions and Their Computation by Machine, Part I." Communications of the ACM.
-   "Arena Allocation", Wikipedia, <https://en.wikipedia.org/wiki/Region-based_memory_management>
