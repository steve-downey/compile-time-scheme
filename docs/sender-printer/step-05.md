# Step 5: Create string_writer applicative

**Phase:** 3 — Applicative Traverser  
**Prereqs:** None (independent of Phase 2, but should run after Steps 1–2 exist)  
**Next step:** step-06.md

## What To Do

Create a Writer monad / Applicative for accumulating DOT output strings. This is runtime-only (uses `std::string`) since DOT output is a diagnostic feature, not compile-time.

## Files to Create

### 1. `src/smd/smdscheme/sender/string_writer.hpp`

```cpp
// src/smd/smdscheme/sender/string_writer.hpp                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_STRING_WRITER_HPP
#define SRC_SMD_SMDSCHEME_SENDER_STRING_WRITER_HPP

#include <string>
#include <utility>

namespace smd::smdscheme::sender {

template <typename T>
struct string_writer {
    std::string log;
    T value;

    static auto pure(T val) -> string_writer {
        return string_writer{"", std::move(val)};
    }
};

// Applicative liftA2: combine two writers, concatenating logs
template <typename F, typename A, typename B>
auto lift_a2(F func, string_writer<A> const &a, string_writer<B> const &b)
    -> string_writer<decltype(func(a.value, b.value))> {
    return {a.log + b.log, func(a.value, b.value)};
}

// Monoidal append: combine two writers that both produce monostate-like values
template <typename T>
auto combine(string_writer<T> const &a, string_writer<T> const &b)
    -> string_writer<T> {
    return {a.log + b.log, b.value};
}

// Map: transform the value without changing the log
template <typename F, typename A>
auto fmap(F func, string_writer<A> const &w)
    -> string_writer<decltype(func(w.value))> {
    return {w.log, func(w.value)};
}

// Tell: write to the log with no meaningful value
inline auto tell(std::string msg) -> string_writer<std::monostate> {
    return {std::move(msg), std::monostate{}};
}

} // namespace smd::smdscheme::sender

#endif
```

Key design:
- `string_writer<T>` holds a log (accumulated string) and a value.
- `pure(val)` creates a writer with empty log.
- `lift_a2(f, a, b)` applies f to values while concatenating logs.
- `combine(a, b)` appends logs (for monostate writers).
- `tell(msg)` writes to log with no value — the primary operation for DOT output.
- `fmap(f, w)` transforms the value.

### 2. `src/smd/smdscheme/sender/string_writer.test.cpp`

```cpp
// src/smd/smdscheme/sender/string_writer.test.cpp              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/sender/string_writer.hpp>
#include <smd/smdscheme/sender/string_writer.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <variant>

using smd::smdscheme::sender::string_writer;
using smd::smdscheme::sender::tell;
using smd::smdscheme::sender::combine;
using smd::smdscheme::sender::lift_a2;
using smd::smdscheme::sender::fmap;

TEST_CASE("StringWriterTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("StringWriterTest - Pure") {
    auto w = string_writer<int>::pure(42);
    CHECK(w.log.empty());
    CHECK(w.value == 42);
}

TEST_CASE("StringWriterTest - Tell") {
    auto w = tell("hello");
    CHECK(w.log == "hello");
}

TEST_CASE("StringWriterTest - Combine") {
    auto a = tell("foo");
    auto b = tell("bar");
    auto c = combine(a, b);
    CHECK(c.log == "foobar");
}

TEST_CASE("StringWriterTest - LiftA2") {
    auto a = string_writer<int>{"log_a:", 3};
    auto b = string_writer<int>{"log_b:", 4};
    auto c = lift_a2([](int x, int y) { return x + y; }, a, b);
    CHECK(c.log == "log_a:log_b:");
    CHECK(c.value == 7);
}

TEST_CASE("StringWriterTest - Fmap") {
    auto w = string_writer<int>{"some_log", 5};
    auto w2 = fmap([](int x) { return x * 2; }, w);
    CHECK(w2.log == "some_log");
    CHECK(w2.value == 10);
}

TEST_CASE("StringWriterTest - CombineMultiple") {
    auto result = combine(combine(tell("a\n"), tell("b\n")), tell("c\n"));
    CHECK(result.log == "a\nb\nc\n");
}
```

## Files to Modify

### 3. `src/smd/smdscheme/sender/CMakeLists.txt`

Add `string_writer.hpp` to FILES list.

## Verification

```bash
make compile && make test
```

All `StringWriterTest` tests must pass.

## Handoff

Update `docs/sender-printer/handoff-next.md` with:
- Confirmation that string_writer compiles (it uses `std::string`, so NOT constexpr)
- Any issues with `std::monostate` or header includes
- Mark Step 5 complete in `docs/sender-printer/checklist.md`
