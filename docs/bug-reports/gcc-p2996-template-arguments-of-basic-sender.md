# GCC Bug: `std::meta::template_arguments_of` throws on certain class template specialisations

- **Component:** C++ compiler (g++), P2996 reflection implementation
- **Affected version:** GCC 16 trunk (confirmed broken; earlier versions
  with `-freflection` may also be affected)
- **Standard:** C++26 with P2996 (`-std=c++26`)
- **Severity:** Incorrect behaviour — standard-conforming code silently
  throws at `consteval` time

---

## Summary

`std::meta::template_arguments_of(r)` throws when `r` reflects a class template
specialisation whose base class is parameterised on the *same* template arguments.
The standard requires `template_arguments_of` to return the template
arguments of any class template specialisation; it should only throw
when the operand is not a specialisation at all.

The problem was discovered while reflecting over `beman::execution::detail::basic_sender`,
whose template signature is:

```cpp
template <typename Tag, typename Data, typename... Child>
struct basic_sender : product_type<Tag, Data, Child...> { ... };
```

`template_arguments_of(^^basic_sender<just_t, product_type<int>>)` throws.
Accessing the same arguments through the inherited base — using `bases_of` to reach
the `product_type` base, then calling `template_arguments_of` on that — succeeds.

---

## Minimal Reproducible Example

The following is self-contained and does not require the beman::execution library.
It mirrors the relevant type structure at each level of complexity.

```cpp
// Compile with: g++ -std=c++26 -freflection mre.cpp
// (or paste into https://godbolt.org with GCC trunk and -std=c++26 -freflection)

#include <meta>
#include <cstddef>
#include <utility>

// ── Level 1: flat template ──────────────────────────────────────────────────
// Expected: works fine.

template <typename T, typename U>
struct flat_pair {};

consteval bool test_flat() {
    auto args = std::meta::template_arguments_of(^^flat_pair<int, float>);
    return args.size() == 2;                         // should be true
}
static_assert(test_flat(), "flat template: expected 2 args");

// ── Level 2: template that inherits from a simple template ──────────────────
// Expected: should still work — the outer type is still a class template
// specialisation and template_arguments_of must return its arguments.

template <typename T, typename U>
struct inner_simple {};

template <typename T, typename U>
struct outer_simple : inner_simple<T, U> {};

consteval bool test_outer_simple() {
    auto args = std::meta::template_arguments_of(^^outer_simple<int, float>);
    return args.size() == 2;
}
static_assert(test_outer_simple(), "outer-simple: expected 2 args");

// ── Level 3: multi-level inheritance with index_sequence (mirrors product_type)
// product_type in beman::execution uses this pattern.

template <std::size_t I, typename T>
struct element { T value; };

template <typename IndexSeq, typename... T>
struct product_base;

template <std::size_t... I, typename... T>
struct product_base<std::index_sequence<I...>, T...>
    : element<I, T>... {};

template <typename... T>
struct product_like
    : product_base<std::index_sequence_for<T...>, T...> {};

// ── Level 4: outer template that inherits from product_like with same args ──
// This mirrors basic_sender<Tag, Data, Child...> : product_type<Tag, Data, Child...>
// HYPOTHESIS: this is where template_arguments_of begins to throw.

template <typename Tag, typename Data, typename... Child>
struct basic_like : product_like<Tag, Data, Child...> {};

struct my_tag {};

consteval bool test_basic_like_direct() {
    // Attempt direct access — expected to work but reportedly throws in GCC16.
    auto r    = ^^basic_like<my_tag, product_like<int>>;
    auto args = std::meta::template_arguments_of(r);  // <-- throws?
    return args.size() == 2;
}
// Uncomment to test:
// static_assert(test_basic_like_direct(), "direct: expected 2 args");

// ── Workaround: go through the base ────────────────────────────────────────
// Accessing template_arguments_of on the product_like base succeeds.
// This is the approach currently used in build_scheme_tree.hpp.

consteval bool test_basic_like_via_base() {
    auto r     = ^^basic_like<my_tag, product_like<int>>;
    auto bases = std::meta::bases_of(r, std::meta::access_context::unchecked());
    // bases[0] reflects product_like<my_tag, product_like<int>>
    auto args  = std::meta::template_arguments_of(std::meta::type_of(bases[0]));
    return args.size() == 2;                         // Tag + Data
}
static_assert(test_basic_like_via_base(), "via-base workaround: expected 2 args");

int main() {}
```

---

## Expected Behaviour

All four `test_*` functions should compile and return `true`.

`basic_like<my_tag, product_like<int>>` is plainly a specialisation of
`basic_like<typename Tag, typename Data, typename... Child>`.
P2996 §3.4 requires `template_arguments_of` to succeed on any reflection of a
class template specialisation and return the corresponding argument list.
Throwing is only permitted when the operand is *not* a specialisation.

---

## Actual Behaviour (GCC 16)

`test_basic_like_direct()` — and the beman::execution equivalent calling
`template_arguments_of(^^basic_sender<just_t, product_type<int>>)` — throws at
`consteval` evaluation time.

`test_basic_like_via_base()` — accessing the same arguments by reflecting on the
`product_like` base class — succeeds.

---

## Distinguishing Factors

To help bisect the root cause, the MRE tests four structural levels:

| Level | Structure | Expected |
| ----- | --------- | -------- |
| 1 | `flat_pair<T,U>` — flat template | Works |
| 2 | `outer_simple<T,U> : inner_simple<T,U>` — same-arg base | Verify |
| 3 | `product_like<T...>` — partial spec on `index_sequence` | Verify |
| 4 | `basic_like<Tag,Data> : product_like<Tag,Data>` | **Throws** |

Determining which level first causes the failure will identify whether
the bug is in handling of:

- inherited base classes generally,
- partial specialisations in the base chain, or
- multi-level pack-expansion inheritance (`product_element<I,T>...`).

---

## Workaround

Use `bases_of` to reach the immediately-inherited base, then call
`template_arguments_of` on that base's type reflection:

```cpp
consteval auto get_tag_args(std::meta::info sender_type) {
    auto bases = std::meta::bases_of(sender_type,
                                     std::meta::access_context::unchecked());
    return std::meta::template_arguments_of(std::meta::type_of(bases[0]));
    // args[0] = Tag, args[1] = Data, args[2+] = Child senders
}
```

This is the approach currently used in
`src/smd/smdscheme/sender/build_scheme_tree.hpp` in the SchemePoC project.
The workaround is reliable for GCC 16 but is obviously undesirable: it requires
knowledge of the internal inheritance layout and breaks if the base class is
changed.

---

## Related Limitation

A separate GCC 16 constraint was discovered at the same time:
`[:r:]` splice syntax fails when `r` is a `std::meta::info` value passed as a
`consteval` function parameter (i.e., when it is not a compile-time constant from
the caller's perspective). This is a separate issue and does not affect the
`template_arguments_of` bug described here.

---

## References

- P2996 "Reflection for C++26": <https://wg21.link/p2996>
- Discovery commit in SchemePoC: `676d316`
  ("feat: Add reflection bridge for just senders")
- Workaround commit: `1c5fc7c`
  ("feat: Extend reflection bridge for then and when_all senders")
- Affected file: `src/smd/smdscheme/sender/build_scheme_tree.hpp`
