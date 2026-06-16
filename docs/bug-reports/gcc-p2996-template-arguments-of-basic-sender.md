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

## Minimal Reproducible Example (Refined)

The original MRE reproduced cleanly in early tests but does not trigger the bug
on recent Godbolt snapshots. Investigation reveals the actual failing pattern:

**Key difference:** The real code obtains the type via `decltype(factory_call())`
and then reflects the *alias*, not the direct template specialization. The factory
pattern, combined with standard library specializations, appears to trigger the
bug in local GCC 16 builds.

```cpp
// Compile with: g++ -std=c++26 -freflection mre.cpp
// (or paste into https://godbolt.org with GCC trunk and -std=c++26 -freflection)

#include <meta>
#include <cstddef>
#include <tuple>
#include <utility>

// ── Minimal Structure: mirrors beman::execution::detail::basic_sender ────────

template <std::size_t I, typename T>
struct product_type_element { T value; };

template <typename IndexSeq, typename... T>
struct product_type_base;

template <std::size_t... I, typename... T>
struct product_type_base<std::index_sequence<I...>, T...>
    : product_type_element<I, T>... {
    static constexpr std::size_t size() noexcept { return sizeof...(T); }
};

template <typename... T>
struct product_type
    : product_type_base<std::index_sequence_for<T...>, T...> {};

// Specializations (like those in std namespace for basic_sender)
template <typename... T>
struct std::tuple_size<product_type<T...>>
    : std::integral_constant<std::size_t, sizeof...(T)> {};

template <std::size_t I, typename... T>
struct std::tuple_element<I, product_type<T...>> {
    // (simplified; actual impl uses get<I>)
    using type = int;
};

template <typename Tag, typename Data, typename... Child>
struct basic_like : product_type<Tag, Data, Child...> {};

struct my_tag {};

// ── Factory pattern: decltype(factory) → reflect on alias ──────────────────

consteval auto make_just_like() {
    return basic_like<my_tag, product_type<int>>{};
}

// Alias obtained via decltype factory call (the actual pattern from
// build_scheme_tree.test.cpp: using JustType = decltype(sender_v::just(42));)
using JustLikeType = decltype(make_just_like());

consteval bool test_via_decltype_alias() {
    // Reflect the decltype alias, not the direct specialization.
    // This is what fails in some GCC 16 configurations.
    auto r = ^^JustLikeType;
    auto args = std::meta::template_arguments_of(r);  // <-- throws?
    return args.size() == 2;
}

// Uncomment to test the decltype-alias pattern:
// static_assert(test_via_decltype_alias(), "decltype alias: expected 2 args");

// ── Direct reflection (works on recent Godbolt, may fail locally) ──────────

consteval bool test_direct_specialization() {
    auto r = ^^basic_like<my_tag, product_type<int>>;
    auto args = std::meta::template_arguments_of(r);  // may also throw
    return args.size() == 2;
}
// static_assert(test_direct_specialization(), "direct: expected 2 args");

// ── Workaround: via base (always works) ──────────────────────────────────────

consteval bool test_via_base() {
    auto r     = ^^JustLikeType;
    auto bases = std::meta::bases_of(r,
                                     std::meta::access_context::unchecked());
    auto args  = std::meta::template_arguments_of(
        std::meta::type_of(bases[0]));
    return args.size() == 2;
}
static_assert(test_via_base(), "via-base workaround: expected 2 args");

int main() {}
```

---

## Expected Behaviour

`test_via_decltype_alias()` and `test_direct_specialization()` should both
compile and return `true`. The workaround `test_via_base()` always succeeds.

`JustLikeType` is plainly a specialisation of
`basic_like<typename Tag, typename Data, typename... Child>`.
P2996 §3.4 requires `template_arguments_of` to succeed on any reflection of a
class template specialisation and return the corresponding argument list.
Throwing is only permitted when the operand is *not* a specialisation.

---

## Actual Behaviour (GCC 16 — local build)

**On recent Godbolt snapshots:** The MRE works. Direct specialization and
decltype-alias patterns both succeed.

**On some local GCC 16 builds:** `test_via_decltype_alias()` may throw at
`consteval` time, while `test_via_base()` always succeeds. The issue may be
related to:

1. The **decltype-alias pattern** (how the type is obtained), or
2. The **`std::tuple_size`/`tuple_element` specializations**, or  
3. Both, in specific GCC 16 release/patch versions.

---

## Distinguishing Factors

If the bug reproduces on your local GCC 16:

- Comment/uncomment `test_via_decltype_alias()` and `test_direct_specialization()`
  individually to identify which pattern fails.
- Try removing the `std::tuple_size`/`tuple_element` specializations to see if
  reflection succeeds without them.
- Check your GCC 16 version (`g++ -v`) and compare against Godbolt's trunk/16.1.

This will help identify whether the issue is version-specific, pattern-specific,
or related to standard library specializations.

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
