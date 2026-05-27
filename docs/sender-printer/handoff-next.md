# Sender Graph Printer — Handoff Notes

This file is rewritten by each step's agent with notes for the next step.

## Current State

Step 3 is complete. All 137 tests pass (up from 135 — 2 new BuildSchemeTreeTest tests). Linters pass.

The branch `worktree-sender-printer-step-03` contains Step 3 work built on top of Step 2.

## What Was Done in Step 3

- Created `src/smd/smdscheme/sender/build_scheme_tree.hpp` — the `classify_sender` and
  `build_scheme_tree` functions using C++26 reflection via `<meta>`.
- Created `src/smd/smdscheme/sender/build_scheme_tree.test.cpp` — idempotency test and
  `JustSender` constexpr test that allocates a one-node tree.
- Updated `src/smd/smdscheme/sender/CMakeLists.txt` to add `build_scheme_tree.hpp` to FILE_SET.
- clang-format also fixed minor formatting in `scheme_tree.hpp` and `scheme_tree.test.cpp`
  (reference spacing, alignment).

## Key Discovery: GCC16 Reflection API Behaviour

**`std::meta::template_arguments_of` DOES NOT WORK for Beman Execution sender types.**

Calling `std::meta::template_arguments_of(type)` where `type` is the reflection of
`decltype(sender_v::just(42))` (i.e. `beman::execution::detail::basic_sender<...>`) throws
a consteval exception at compile time:

```
error: uncaught exception of type 'std::meta::exception';
  'what()': 'reflection does not have template arguments'
```

This means GCC16's P2996 implementation does not expose template arguments for class
template specializations of `basic_sender` through `template_arguments_of`.

**What DOES work: `std::meta::display_string_of`.**

`std::meta::display_string_of(type)` returns a `std::string_view` containing the full
qualified type name. For `decltype(sender_v::just(42))`, the display string contains
`"just_t"` (as part of the full name including
`"beman::execution::detail::basic_sender<beman::execution::detail::just_t<...>, ...>"`).

Confirmed working:
- `just` senders: display string contains `"just_t"`

Expected (step 4 must confirm):
- `then` senders: display string should contain `"then_t"`
- `when_all` senders: display string should contain `"when_all_t"`

The working `classify_sender` implementation (in `build_scheme_tree.hpp`):
```cpp
consteval auto classify_sender(std::meta::info type) -> std::string_view {
    std::string_view name = std::meta::display_string_of(type);
    if (name.find("just_t") != std::string_view::npos) return "just";
    return "unknown";
}
```

## Notes for Step 4

Begin with `step-04.md`. Step 4 extends `build_scheme_tree` to handle `then` and `when_all`
senders, which requires recursive traversal into child senders.

### Critical Challenge: Recursive Traversal Without template_arguments_of

Since `template_arguments_of` throws on `basic_sender` types, step 4 must find an
alternative for extracting child sender type reflections. Options to explore in order:

**Option A: bases_of + template_arguments_of on the base class.**
`basic_sender<Tag, Data, Child...>` inherits from `product_type<Tag, Data, Child...>`.
Try `std::meta::bases_of(type)` to get the base class reflection, then
`std::meta::template_arguments_of` on the base. The base is a standard aggregate and
may be handled differently by GCC16.

**Option B: Use sender_decompose type traits at the type level.**
`vendor/execution/include/beman/execution/detail/sender_decompose.hpp` provides
`get_sender_meta(sender)` which yields a `sender_meta<TagType, DataType, ChildrenTupleType>`.
At the type level:
```cpp
using Meta = decltype(beman::execution::detail::get_sender_meta(
    std::declval<SenderType>()));
// Meta::tag_type = just_t<set_value_t> (or then_t, when_all_t)
// Meta::children_type = std::tuple<Child...>
```
Then reflect on individual child types via `std::tuple_element_t<I, Meta::children_type>`:
```cpp
constexpr std::size_t n = std::tuple_size_v<typename Meta::children_type>;
// for each I in [0, n):
//   auto child_id = build_scheme_tree<MaxNodes>(
//       ^^std::tuple_element_t<I, typename Meta::children_type>, tree);
```

**Option C: Template metaprogramming partial specialization.**
Define a trait outside the consteval context:
```cpp
template <typename Sender>
struct sender_child_count { static constexpr int value = 0; };
template <typename Tag, typename Data, typename... Child>
struct sender_child_count<beman::execution::detail::basic_sender<Tag, Data, Child...>> {
    static constexpr int value = sizeof...(Child);
};
template <int I, typename Sender>
struct sender_child_type;
template <int I, typename Tag, typename Data, typename... Child>
struct sender_child_type<I, beman::execution::detail::basic_sender<Tag, Data, Child...>> {
    using type = std::tuple_element_t<I, std::tuple<Child...>>;
};
```
Then in `build_scheme_tree`, use these traits with `^^typename sender_child_type<I, SenderType>::type`.

### Beman Sender Structure Reference

For `then(pred_sender, fn)`:
- Type: `basic_sender<then_t, product_type<Fn>, PredSender>`
- Tag at index 0, data (fn) at index 1, predecessor sender at index 2
- Only ONE child sender: the predecessor

For `when_all(s1, s2, ...)`:
- Type: `basic_sender<when_all_t, make_sender_empty, S1, S2, ...>`
- Tag at index 0, empty data at index 1, child senders at indices 2+
- N child senders

### Classification extensions for step 4

Extend `classify_sender` in `build_scheme_tree.hpp` with:
```cpp
if (name.find("then_t") != std::string_view::npos) return "then";
if (name.find("when_all_t") != std::string_view::npos) return "when_all";
```
(Add these before the final `return "unknown"` line.)

### Template parameter for step 4's build_scheme_tree

The step-04.md design proposes a template approach. Since `classify_sender` returns the
name but the function signature takes `std::meta::info`, the agent will need to figure
out how to make recursion work. The key signature to aim for:

```cpp
template <int MaxNodes = 64>
consteval auto build_scheme_tree(std::meta::info sender_type,
                                 scheme_tree<MaxNodes>& tree) -> int;
```

The `^^ChildType` reflections for recursion must come from somewhere — try Options A/B/C
above. Option B (sender_decompose) is most likely to work without hitting the
`template_arguments_of` limitation.

### No CMake changes needed for step 4

`build_scheme_tree.hpp` is already in the FILE_SET. Step 4 only modifies that header and
its test file. No new files need to be added to CMakeLists.txt.
