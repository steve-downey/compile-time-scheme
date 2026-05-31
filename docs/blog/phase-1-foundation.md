<div class="abstract" id="org452dd11">
<p>
Welcome to the first post in our series on building SchemePoC, a compile-time Scheme compiler built entirely in C++26.
Before we can parse a single S-expression or evaluate a lambda, we must lay a solid foundation.
Here is how we survive the <code>constexpr</code> straightjacket using zero-allocation data structures and monadic error handling.
</p>

</div>


# The `constexpr` Challenge

In C++26, the `constexpr` environment is incredibly powerful, but it comes with a fundamental restriction: traditional heap allocation—such as standard `std::vector` or `std::string` instances that carry data into runtime—is prohibited from persisting beyond the compile-time boundary. Any memory allocated during constant evaluation must be deallocated before the evaluation completes if that data structure is to be leaked into the runtime world.

Because our Scheme compiler parses and generates code at compile-time, we cannot rely on the standard library's heap-allocating containers to hold our Abstract Syntax Tree (AST) or our generated program representation. If you try, the compiler will unceremoniously inform you that your allocation is not a constant expression.


# Zero-Allocation Utilities

To survive and thrive within these boundaries, we establish zero-allocation, fixed-capacity tools early on.


## The `static_vector` Pattern

The cornerstone of this foundation is the `static_vector`. It's essentially an array with a dynamically tracked size but a statically determined maximum capacity.

```c++
template <typename T, std::size_t Capacity>
class static_vector {
    std::array<T, Capacity> data_;
    std::size_t size_ = 0;
    // ... push_back, pop_back, end(), begin() operations ...
};
```

By using `static_vector`, we eliminate dynamic memory allocation entirely. The storage is completely inline, satisfying the `constexpr` constraints. All structures representing our parsed Scheme program will be backed by such fixed-allocation memory arenas. It turns out that pre-allocating a chunk of memory and carefully managing indices is not just for 1980s game developers; it's required for 2020s compile-time computing.


## Monadic Error Handling

Compiler engineering inherently means dealing with invalid input. Exceptions are not available in `constexpr` contexts in the way standard runtime C++ handles them, and C-style error codes quickly become an unwieldy mess of `if (err != 0) return err;`.

Instead, we employ Monadic error handling, drawing inspiration from Haskell's `Either` type or Rust's `Result`. We utilize a `result<T, E>` type (similar to `std::expected` in C++23).

```c++
template <typename T, typename E>
class result {
    // ...
};
```

This pattern allows us to cleanly thread success and failure states through our parser pipeline without relying on exceptions or out-parameters. It remains purely functional and, most importantly, `constexpr` friendly.


# Conclusion

By establishing a robust toolset based on zero-allocation data structures and monadic error handling, we create a safe sandbox. This foundation ensures that the complex parsing and semantic analysis layers to follow can focus purely on logic, unburdened by the quirks of the C++26 `constexpr` memory model.


# References

-   Smith, R. (2022). "Constant evaluation in C++". C++ Standards Committee Papers.
-   "std::expected", cppreference.com, <https://en.cppreference.com/w/cpp/utility/expected>
