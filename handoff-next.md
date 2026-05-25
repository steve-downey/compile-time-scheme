# Next step: Step 13 (typeclass-object facade for parser operations)

## Goal

Introduce a typeclass-object facade over the existing parser free functions.
This is a presentation/refactoring step with no behavior changes.
The facade exposes `map`, `apply`, `alt`, and `pure` as methods on a global constant object.
All existing parser tests must continue to pass unchanged.

## Files expected to change

```txt
src/smd/schemepoc/parser_ops.hpp        (new)
src/smd/schemepoc/parser_ops.test.cpp   (new)
src/smd/schemepoc/CMakeLists.txt        (add new files)
```

No existing files need to change.

## Context from previous steps

Steps 11 and 12 (combined) completed the first end-to-end milestone:
`source string → datum_tree → core_tree → value`.

What is in the codebase now:

- `value.hpp`: `enum class builtin_op { add, multiply }`, `struct builtin`, `using value = std::variant<int, bool, builtin>`, `template <int MaxBindings> class env`, `template <int MaxBindings> constexpr auto default_env() -> env<MaxBindings>`
- `eval_direct.hpp`: `template <int MaxNodes, int MaxList, int MaxBindings> constexpr auto eval_direct(core_tree<MaxNodes, MaxList> const &ct, node_id root, env<MaxBindings> const &environment) -> result<value>`
- `elaborator.hpp`: all core types, `elaborate(datum_tree)` function
- `reader.hpp`: `read_datum<MaxNodes, MaxList>(cursor{sv})` returning `parse_result<datum_tree<...>>`

104 tests pass. `make compile`, `make test`, `make lint` are all green.

The existing free-function parser API (in `parser.hpp` and `parser_alternative.hpp`):
- `pure(value)` → parser that always succeeds
- `satisfy(pred, expected)` → char parser
- `char_p(ch)` → char parser
- `map(p, f)` → transforms parser output
- `apply(pf, pa)` → applicative apply
- `lift2(f, pa, pb)` → applies binary function over two parsers
- `sequence_left(left, right)`, `sequence_right(left, right)` → sequence discarding one side
- `alt(pa, pb)` → try first, fall back to second
- `many<Capacity>(p)`, `some<Capacity>(p)` → repetition
- `optional(p)` → zero or one
- `lexeme(p)` → skips surrounding intertoken whitespace

## Required implementation

Define in `namespace smd::schemepoc`:

```cpp
struct parser_applicative_ops {
    template <class T>
    [[nodiscard]] constexpr auto pure(T value) const;

    template <class P, class F>
    [[nodiscard]] constexpr auto map(P p, F f) const;

    template <class PF, class PA>
    [[nodiscard]] constexpr auto apply(PF pf, PA pa) const;

    template <class F, class PA, class PB>
    [[nodiscard]] constexpr auto lift2(F f, PA pa, PB pb) const;

    template <class PLeft, class PRight>
    [[nodiscard]] constexpr auto sequence_left(PLeft left, PRight right) const;

    template <class PLeft, class PRight>
    [[nodiscard]] constexpr auto sequence_right(PLeft left, PRight right) const;
};

struct parser_alternative_ops {
    template <class PA, class PB>
    [[nodiscard]] constexpr auto alt(PA pa, PB pb) const;

    template <int Capacity, class P>
    [[nodiscard]] constexpr auto many(P p) const;

    template <int Capacity, class P>
    [[nodiscard]] constexpr auto some(P p) const;

    template <class P>
    [[nodiscard]] constexpr auto optional(P p) const;

    template <class P>
    [[nodiscard]] constexpr auto lexeme(P p) const;
};

struct parser_ops : parser_applicative_ops, parser_alternative_ops {};

inline constexpr parser_ops parser_v{};
```

Each method delegates directly to the corresponding free function.
For example:

```cpp
template <class T>
[[nodiscard]] constexpr auto parser_applicative_ops::pure(T value) const {
    return smd::schemepoc::pure(value);
}

template <class P, class F>
[[nodiscard]] constexpr auto parser_applicative_ops::map(P p, F f) const {
    return smd::schemepoc::map(p, f);
}
```

Do not declare members in the class body and define them out of line unless the compiler requires it.
Inline templated definitions in the class body are acceptable here.

## Required tests

```cpp
// Usage via parser_v
static_assert([] {
    using namespace smd::schemepoc;
    auto p = parser_v.map(char_p('x'), [](char) { return 42; });
    auto r = p(cursor{"x"});
    return r.has_value() && r.value().value == 42;
}());

static_assert([] {
    using namespace smd::schemepoc;
    auto p = parser_v.alt(char_p('a'), char_p('b'));
    auto ra = p(cursor{"a"});
    auto rb = p(cursor{"b"});
    return ra.has_value() && ra.value().value == 'a' &&
           rb.has_value() && rb.value().value == 'b';
}());

static_assert([] {
    using namespace smd::schemepoc;
    auto p = parser_v.pure(7);
    auto r = p(cursor{"anything"});
    return r.has_value() && r.value().value == 7;
}());
```

Add a `TEST_CASE("ParserOpsTest - HeaderIsIdempotent")` runtime test.

## CMakeLists.txt changes

Add to `FILE_SET HEADERS`:
- `parser_ops.hpp`

Add to `target_sources` for `schemepoc_test`:
- `parser_ops.test.cpp`

Follow the same pattern as `elaborator.hpp` / `elaborator.test.cpp`.

## Required commands

```bash
make compile
make test
make lint
```

## Do not do

- Do not change any existing free functions.
- Do not change `parser.hpp`, `parser_alternative.hpp`, or any earlier file.
- Do not change any existing tests.
- Do not introduce behavior changes.
- Do not add CPS, closure backend, or evaluator changes.
- Do not proceed to Step 14 (generic fixpoint playground) in the same commit.
- Do not add `using namespace` in headers.
- Do not change architecture decisions without documenting the reason in `handoff.md`.
