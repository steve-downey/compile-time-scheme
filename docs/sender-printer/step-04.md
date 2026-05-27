# Step 4: Extend reflection bridge for `then` and `when_all`

**Phase:** 2 — Reflection Bridge
**Prereqs:** Step 3 (build_scheme_tree handles `just`)
**Next step:** step-05.md

## What To Do

Extend `build_scheme_tree.hpp` to recognize and recursively process `then` and `when_all` senders. Read `handoff-next.md` for the actual type names discovered in Step 3.

## Design

- **`then`** senders have a predecessor sender (first template arg) and a function (second template arg). Recurse into the predecessor and add it as a child.
- **`when_all`** senders have N child senders as template arguments. Recurse into each and add them all as children.

## Changes to Make

### 1. Modify `src/smd/smdscheme/sender/build_scheme_tree.hpp`

Extend `classify_sender`:
```cpp
consteval auto classify_sender(std::meta::info type) -> std::string_view {
    // Use the matching approach discovered in Step 3 (see handoff-next.md)
    // Match "then", "when_all" in addition to "just"
}
```

Extend `build_scheme_tree` to recurse:
```cpp
template <int MaxNodes = 64>
consteval auto build_scheme_tree(std::meta::info sender_type,
                                  scheme_tree<MaxNodes>& tree) -> int {
    auto algo = classify_sender(sender_type);

    scheme_node_data node{};
    node.sender_algo = algo;

    if (algo == "then") {
        // then<Predecessor, Function>
        auto args = std::meta::template_arguments_of(sender_type);
        // First arg is the predecessor sender type
        auto child_id = build_scheme_tree<MaxNodes>(args[0], tree);
        auto id = tree.allocate(node);
        tree.get(id).child_ids.push_back(child_id);
        return id;
    }

    if (algo == "when_all") {
        // when_all<Sender1, Sender2, ...>
        auto args = std::meta::template_arguments_of(sender_type);
        auto id = tree.allocate(node);
        for (auto arg : args) {
            auto child_id = build_scheme_tree<MaxNodes>(arg, tree);
            tree.get(id).child_ids.push_back(child_id);
        }
        return id;
    }

    // Default (just, unknown): leaf node
    auto id = tree.allocate(node);
    return id;
}
```

**IMPORTANT:** The exact structure of `std::meta::template_arguments_of()` output depends on the Beman implementation. Some senders may wrap their state differently. The agent must:
1. Inspect `template_arguments_of` for `then(just(1), fn)` type
2. Determine which arg is the predecessor vs the function
3. Skip non-sender arguments (functions, value wrappers)

### 2. Extend `src/smd/smdscheme/sender/build_scheme_tree.test.cpp`

Add tests:

```cpp
TEST_CASE("BuildSchemeTreeTest - ThenSender") {
    constexpr auto tree = [] {
        sender::scheme_tree<16> t{};
        using ThenType = decltype(sender_v::then(sender_v::just(1),
                                                  [](int x) { return x + 1; }));
        auto root = sender::build_scheme_tree<16>(^^ThenType, t);
        t.root_id = root;
        return t;
    }();

    REQUIRE(tree.nodes.size() == 2);
    CHECK(tree.get(tree.root_id).sender_algo == "then");
    CHECK(tree.get(tree.root_id).child_ids.size() == 1);
    auto child_id = tree.get(tree.root_id).child_ids[0];
    CHECK(tree.get(child_id).sender_algo == "just");
}

TEST_CASE("BuildSchemeTreeTest - WhenAllSender") {
    constexpr auto tree = [] {
        sender::scheme_tree<16> t{};
        using WhenAllType = decltype(sender_v::when_all(
            sender_v::just(1), sender_v::just(2)));
        auto root = sender::build_scheme_tree<16>(^^WhenAllType, t);
        t.root_id = root;
        return t;
    }();

    REQUIRE(tree.nodes.size() == 3);
    CHECK(tree.get(tree.root_id).sender_algo == "when_all");
    CHECK(tree.get(tree.root_id).child_ids.size() == 2);
}

TEST_CASE("BuildSchemeTreeTest - NestedSender") {
    constexpr auto tree = [] {
        sender::scheme_tree<16> t{};
        using NestedType = decltype(sender_v::then(
            sender_v::when_all(sender_v::just(1), sender_v::just(2)),
            [](int a, int b) { return a + b; }));
        auto root = sender::build_scheme_tree<16>(^^NestedType, t);
        t.root_id = root;
        return t;
    }();

    REQUIRE(tree.nodes.size() == 4); // then, when_all, just, just
    CHECK(tree.get(tree.root_id).sender_algo == "then");
}
```

## Verification

```bash
make compile && make test
```

All `BuildSchemeTreeTest` tests must pass including the new then/when_all/nested cases.

## Handoff

Update `docs/sender-printer/handoff-next.md` with:
- How `template_arguments_of` works for then and when_all types
- Which argument indices are senders vs functions
- Any edge cases or workarounds needed for the Beman types
- Mark Step 4 complete in `docs/sender-printer/checklist.md`
