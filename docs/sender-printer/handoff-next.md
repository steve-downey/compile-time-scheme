# Sender Graph Printer — Handoff Notes

This file is rewritten by each step's agent with notes for the next step.

## Current State

Step 4 is complete. All 140 tests pass (up from 137 — 3 new BuildSchemeTreeTest tests:
ThenSender, WhenAllSender, NestedSender). Linters pass.

The branch `worktree-sender-printer-step-04` contains Step 4 work built on top of Step 3.

## What Was Done in Step 4

- Extended `classify_sender` with position-based min-find to correctly identify the
  OUTERMOST sender tag even when child tag names appear in the full display string.
  Key fix: `std::min({just_pos, then_pos, when_all_pos})` finds the first occurrence.
  Order of checks: when_all first, then then, then just (outermost tag is always first).
- Extended `build_scheme_tree` to recursively traverse child senders via:
  1. `std::meta::bases_of(sender_type, std::meta::access_context::unchecked())`
     to get the `product_type<Tag, Data, Child...>` base of `basic_sender`.
  2. `std::meta::type_of(base)` to convert the base specifier to a type reflection.
  3. `std::meta::template_arguments_of(base_type)` to get [Tag(0), Data(1), Child...(2+)].
  4. For loop over indices 2+ calling `build_scheme_tree(args[i], tree)` recursively.
- Updated `build_scheme_tree.test.cpp` with ThenSender, WhenAllSender, NestedSender tests.
- No CMakeLists.txt changes (build_scheme_tree.hpp was already in the FILE_SET).

## Critical GCC16 Reflection Constraints (MUST READ for all future steps)

### 1. `[:r:]` splice DOES NOT WORK on consteval function parameters

GCC16 requires the splice argument to be a constant expression at TEMPLATE DEFINITION
time. A consteval function parameter does NOT satisfy this — even though the parameter IS
a constant expression at evaluation time. Error seen when attempting `using T = [:r:]`:

```
error: 'sender_type' is not a constant expression
error: splice argument must be an expression of type 'std::meta::info'
```

Workaround: pass the `std::meta::info` directly to other reflection functions
(`display_string_of`, `bases_of`, etc.) without splicing. Do NOT attempt splice.

### 2. `template_arguments_of` fails on `basic_sender` but works on its base `product_type`

Step 3 discovered: `template_arguments_of(^^basic_sender_type)` throws at consteval time.
Step 4 confirms: `template_arguments_of(type_of(bases_of(basic_sender_type)[0]))` WORKS.
The base class `product_type<Tag, Data, Child...>` supports `template_arguments_of`.

Template argument structure for basic_sender children:
- args[0] = Tag type (just_t, then_t, when_all_t, etc.)
- args[1] = Data type (Fn, make_sender_empty, product_type<V>, etc.)
- args[2+] = Child sender types (zero for just, one for then, N for when_all)

### 3. `bases_of` requires explicit `access_context`

No default parameter in GCC16. Must always pass:
```cpp
std::meta::bases_of(type, std::meta::access_context::unchecked())
```

### 4. Fold expressions with 2+ pack elements fail in consteval recursive contexts

Avoid `(... , expr_depending_on_pack)` folds inside consteval functions when the pack
has 2+ elements. A regular `for` loop over a `std::vector<std::meta::info>` works fine
(as demonstrated in the existing `reified_environment.hpp`).

### 5. The working classify_sender (position-based, no splice)

```cpp
consteval auto classify_sender(std::meta::info type) -> std::string_view {
    std::string_view name  = std::meta::display_string_of(type);
    auto             jp    = name.find("just_t");
    auto             tp    = name.find("then_t");
    auto             wp    = name.find("when_all_t");
    auto             first = std::min({jp, tp, wp});
    if (first == std::string_view::npos) return "unknown";
    if (first == wp)                     return "when_all";
    if (first == tp)                     return "then";
    return "just";
}
```

## Notes for Step 5

Begin with `step-05.md`. Step 5 creates `string_writer.hpp` — a Writer applicative for
runtime string accumulation. It is RUNTIME-ONLY (uses `std::string`), NOT constexpr.

Step 5 is **fully independent** of Phase 2 reflection work. It does not use `<meta>`,
`build_scheme_tree`, or any Beman execution types. Pure C++ template code.

### Files to create/modify for Step 5

- Create: `src/smd/smdscheme/sender/string_writer.hpp`
- Create: `src/smd/smdscheme/sender/string_writer.test.cpp`
- Modify: `src/smd/smdscheme/sender/CMakeLists.txt` — add `string_writer.hpp` to FILE_SET

### CMakeLists.txt current FILE_SET (after step 4)

```cmake
FILES
    sender_program.hpp
    sender_v.hpp
    sender_cps_program.hpp
    scheme_node_data.hpp
    scheme_tree.hpp
    build_scheme_tree.hpp
```

Add `string_writer.hpp` (gersemi formats one-per-line; match existing style).

### std::monostate

The step-05.md design uses `string_writer<std::monostate>` for `tell()`. Include
`<variant>` in `string_writer.hpp` since `std::monostate` is defined there.
