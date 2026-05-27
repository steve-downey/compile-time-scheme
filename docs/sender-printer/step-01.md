# Step 1: Create scheme_node_data struct

**Phase:** 1 — Runtime Topology
**Prereqs:** None
**Next step:** step-02.md

## What To Do

Create a plain data struct that captures one node of the sender execution topology.

## Files to Create

### 1. `src/smd/smdscheme/sender/scheme_node_data.hpp`

```cpp
// src/smd/smdscheme/sender/scheme_node_data.hpp               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_SCHEME_NODE_DATA_HPP
#define SRC_SMD_SMDSCHEME_SENDER_SCHEME_NODE_DATA_HPP

#include <smd/smdscheme/foundation/static_vector.hpp>

#include <string_view>

namespace smd::smdscheme::sender {

struct scheme_node_data {
    int node_id{};
    std::string_view sender_algo{};     // "just", "then", "when_all"
    std::string_view scheme_context{};  // "Atom: 5", "Apply: +", etc.
    foundation::static_vector<int, 8> child_ids{};

    friend constexpr auto operator==(scheme_node_data const &,
                                     scheme_node_data const &)
        -> bool = default;
};

} // namespace smd::smdscheme::sender

#endif
```

Key decisions:
- Use `std::string_view` (not `std::string`) — constexpr-compatible, referencing string literals.
- Use `foundation::static_vector<int, 8>` for child_ids — constexpr-compatible, max 8 children per node.
- Default `operator==` for testing.

### 2. `src/smd/smdscheme/sender/scheme_node_data.test.cpp`

```cpp
// src/smd/smdscheme/sender/scheme_node_data.test.cpp           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/sender/scheme_node_data.hpp>
#include <smd/smdscheme/sender/scheme_node_data.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

using smd::smdscheme::sender::scheme_node_data;

static_assert(scheme_node_data{} == scheme_node_data{});

static_assert([] {
    scheme_node_data n{};
    n.node_id = 1;
    n.sender_algo = "just";
    n.scheme_context = "Atom: 42";
    return n.sender_algo == "just" && n.node_id == 1;
}());

static_assert([] {
    scheme_node_data n{};
    n.node_id = 0;
    n.child_ids.push_back(1);
    n.child_ids.push_back(2);
    return n.child_ids.size() == 2 && n.child_ids[0] == 1 && n.child_ids[1] == 2;
}());

TEST_CASE("SchemeNodeDataTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("SchemeNodeDataTest - DefaultConstruction") {
    scheme_node_data n{};
    CHECK(n.node_id == 0);
    CHECK(n.sender_algo == "");
    CHECK(n.scheme_context == "");
    CHECK(n.child_ids.size() == 0);
}

TEST_CASE("SchemeNodeDataTest - WithChildren") {
    scheme_node_data n{};
    n.node_id = 5;
    n.sender_algo = "when_all";
    n.scheme_context = "Parallel Args";
    n.child_ids.push_back(6);
    n.child_ids.push_back(7);
    CHECK(n.child_ids.size() == 2);
    CHECK(n.child_ids[0] == 6);
    CHECK(n.child_ids[1] == 7);
}
```

## Files to Modify

### 3. `src/smd/smdscheme/sender/CMakeLists.txt`

Add `scheme_node_data.hpp` to the `FILES` list in the `target_sources` block.

Find the line:
```cmake
FILES sender_program.hpp sender_v.hpp sender_cps_program.hpp
```
Change to:
```cmake
FILES sender_program.hpp sender_v.hpp sender_cps_program.hpp scheme_node_data.hpp
```

## Verification

```bash
make compile && make test
```

All existing tests must still pass. New `SchemeNodeDataTest` tests must appear and pass.

## Handoff

Update `docs/sender-printer/handoff-next.md` with:
- Confirmation that `scheme_node_data` compiles and tests pass
- The exact `static_vector` capacity chosen (8) and any issues encountered
- Mark Step 1 complete in `docs/sender-printer/checklist.md`
