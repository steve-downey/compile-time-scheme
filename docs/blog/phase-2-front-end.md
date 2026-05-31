<div class="abstract" id="orgd7b68e2">
<p>
Parsing text into meaningful structure is the first visible step of any compiler.
Instead of traditional lexer/parser generators like Lex/Yacc or hand-rolling a massive recursive descent state machine, we lean heavily into Category Theory and Haskell-inspired functional patterns by using Parser Combinators.
This post walks through the exact machinery: from the immutable <code>cursor</code> that carries input state, through the <code>parse_result</code> that models success and failure, to the combinators that snap together into a working Scheme lexer—all in zero-allocation <code>constexpr</code> C++26.
</p>

</div>


# Why Not Recursive Descent or LL(1)?

Recursive descent is the industry standard for production compilers (like GCC or Clang) because it offers unparalleled control over error recovery and diagnostics. However, for a purposely simple language like Scheme—with its highly uniform S-expression syntax—a recursive descent parser involves writing verbose, repetitive loops and state-tracking mechanisms.

We also avoid parser generators (like Bison/Yacc). While mathematically rigorous, they introduce massive build-system complexity and generate code that is historically difficult to make strictly `constexpr` compliant in modern C++. Trying to force a tool from 1975 to emit C++26 constant expressions is an exercise in misery.

There is a deeper constraint: inside `constexpr` evaluation, you cannot easily juggle lambdas that capture variables and convert them back to function pointers. Every parser must be a concrete callable object whose type the compiler can reason about statically. Parser combinators, expressed as C++ templates, satisfy this requirement naturally.


# The Immutable Cursor

Before any parser can run, we need a way to represent "where we are in the input." The simplest possible design is a `std::string_view` plus a position counter—but the critical design decision is that advancing the cursor returns a *new* cursor rather than mutating the existing one.

```cpp
/// An immutable view into the remaining input with an associated source
/// position.
///
/// All advancing operations return a new @c cursor rather than mutating this
/// one, so parsers can checkpoint and backtrack freely.
class cursor {
    std::string_view input_{};
    foundation::source_pos pos_{};

  public:
    /// Constructs a cursor at the beginning of @p input.
    constexpr explicit cursor(std::string_view input) : input_{input} {}

    /// Returns true when no input remains.
    constexpr auto empty() const -> bool { return input_.empty(); }

    /// Returns the next character without consuming it.
    /// @pre !empty()
    constexpr auto peek() const -> char { return input_.front(); }

    /// Returns a new cursor that has consumed the next character, updating
    /// the position (line/column/offset).
    /// @pre !empty()
    constexpr auto bump() const -> cursor {
        cursor next{*this};
        if (!input_.empty()) {
            char c = input_.front();
            next.input_.remove_prefix(1);
            ++next.pos_.offset;
            if (c == '\n') {
                ++next.pos_.line;
                next.pos_.column = 1;
            } else {
                ++next.pos_.column;
            }
        }
        return next;
    }

    /// Returns the current source position.
    constexpr auto position() const -> foundation::source_pos { return pos_; }

    /// Returns the unconsumed portion of the input as a string_view.
    constexpr auto remaining() const -> std::string_view { return input_; }
};
```

`peek()` reads the next character without consuming it. `bump()` returns a brand-new `cursor` with one character removed from the front and the position updated. Neither mutates anything. This immutability is what makes backtracking free: to "go back," a parser simply discards the advanced cursor and uses the one it saved earlier—no undo stack, no state restoration.

The position tracks line, column, and absolute byte offset. The offset is the key field for the Alternative combinator: it detects whether a parser consumed any input before failing.

The helper functions that classify characters live in the same header:

```cpp
/// Returns true if @p c is ASCII whitespace.
constexpr auto is_space(char c) -> bool {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/// Returns true if @p c is a valid first character of a Scheme symbol.
///
/// Allows letters and the operator characters @c + @c - @c * @c / @c =
/// @c < @c > @c ! @c ?.
constexpr auto is_initial_symbol_char(char c) -> bool {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        return true;
    }
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '=' ||
           c == '<' || c == '>' || c == '!' || c == '?';
}

/// Returns true if @p c may appear anywhere (including the first position)
/// in a Scheme symbol — letter, operator character, or decimal digit.
constexpr auto is_symbol_char(char c) -> bool {
    return is_initial_symbol_char(c) || (c >= '0' && c <= '9');
}

/// Returns true if @p c is a token boundary in Scheme source:
/// whitespace, @c ( @c ) or @c '.
constexpr auto is_delimiter(char c) -> bool {
    return is_space(c) || c == '(' || c == ')' || c == '\'';
}

/// Advances @p cur past all leading whitespace, returning the updated cursor.
constexpr auto skip_intertoken_space(cursor cur) -> cursor {
    while (!cur.empty() && is_space(cur.peek())) {
        cur = cur.bump();
    }
    return cur;
}
```

`is_initial_symbol_char` and `is_symbol_char` encode the Scheme symbol alphabet directly. `is_delimiter` is used by the boolean parser in the reader to detect token boundaries without consuming them. `skip_intertoken_space` is a plain loop—no parser abstraction needed for something this simple.


# Success and Failure: `parse_result<T>`

Every parser returns the same shape of result: either a value plus the remaining cursor, or an error with a position. This is modelled with two types:

```cpp
/// Carries a successfully parsed value together with the unconsumed @ref
/// cursor.
///
/// @tparam T The type of the parsed value.
template <class T>
struct parse_state {
    T value;     ///< The successfully parsed value.
    cursor rest; ///< The cursor positioned after the parsed input.
};

/// Alias for the result type returned by all parsers.
/// Holds either a @ref parse_state on success or a @ref foundation::parse_error
/// on failure.
///
/// @tparam T The success value type.
template <class T>
using parse_result = foundation::result<parse_state<T>>;
```

`parse_state<T>` bundles the successfully parsed value with the cursor positioned just after the consumed input. `parse_result<T>` is an alias for `foundation::result<parse_state<T>>~—our project's ~std::expected`-like type that holds either the state or a `parse_error` (position + message string).

The `T` is a template parameter, so parsers are naturally typed: a parser for a digit character returns `parse_result<char>`, a parser for an integer returns `parse_result<atom_integer>`. The type propagates automatically through every combinator.


# The `parser<F>` Wrapper

Parsers are callable objects. The `parser<F>` class wraps any callable `F` with the right signature and gives it a concrete type that satisfies the `parser_like` concept:

```cpp
/// A type-erased callable wrapper for a single-pass parser.
///
/// @c parser<F> wraps a callable @p F with signature
/// @c parse_result<T>(cursor). It satisfies @ref parser_like so it
/// composes with all the combinator functions in this header.
///
/// @tparam F Callable type; deduced via the deduction guide.
template <class F>
class parser {
  public:
    /// Constructs a parser wrapping @p f.
    constexpr explicit parser(F f) : f_{f} {}

    /// Runs the parser starting at @p cur.
    constexpr auto operator()(cursor cur) const { return f_(cur); }

  private:
    F f_;
};

/// Deduction guide: @c parser(f) deduces @c parser<F>.
template <class F>
parser(F) -> parser<F>;
```

The class is intentionally thin. It stores `F` by value (critical for `constexpr` since we cannot take addresses of lambdas at compile time), exposes `operator()(cursor)`, and satisfies `parser_like`. The CTAD guide `parser(f) -> parser<F>` means you can write `parser{lambda}` without spelling out `F`.

The `parser_like` concept at the top of the file requires only that a type be callable with a `cursor`. This lets free functions and other callable objects participate in the combinator algebra without being wrapped, while still being checkable at the point of use.


# The Primitives

Two functions build every other parser in the library.


## `pure`: Inject a Value

```cpp
/// Returns a parser that always succeeds, consuming no input and yielding
/// @p value.
///
/// @tparam T Value type.
/// @param  value The constant value to produce.
template <class T>
[[nodiscard]] constexpr auto pure(T value) {
    return parser{[v = value](cursor cur) -> parse_result<T> {
        return parse_state<T>{v, cur};
    }};
}
```

`pure(v)` always succeeds, consumes nothing, and returns `v`. It is the identity element of the Applicative structure: it lifts a known value into the parser context without touching the input. In practice it is used to provide default values—for example, after `optional(char_p('-'))` produces `std::nullopt`, `pure(+1)` seeds the sign for a positive integer.


## `satisfy` and `char_p`: Consume One Character

```cpp
/// Returns a parser that succeeds when the next character satisfies @p pred.
///
/// On success consumes one character. On failure reports @p expected at the
/// current position.
///
/// @param pred     Predicate on @c char.
/// @param expected Human-readable description of the expected token (used in
///                 @ref foundation::parse_error::message).
[[nodiscard]] constexpr auto satisfy(auto pred, char const *expected) {
    return parser{[pred, expected](cursor cur) -> parse_result<char> {
        if (!cur.empty() && pred(cur.peek())) {
            return parse_state<char>{cur.peek(), cur.bump()};
        }
        return foundation::parse_error{cur.position(), expected};
    }};
}

/// Returns a parser that matches exactly the character @p expected.
///
/// @param expected The character to match.
[[nodiscard]] constexpr auto char_p(char expected) {
    return satisfy([expected](char c) { return c == expected; },
                   "expected char");
}
```

`satisfy(pred, desc)` is the single atomic building block of the entire parser. It peeks at the next character, checks `pred`, and either consumes the character (returning the new cursor from `cur.bump()`) or fails with a `parse_error` at the current position. The `desc` string names what was expected and ends up in error messages.

`char_p(c)` is sugar: it builds `satisfy` with a lambda that compares against a single character. These two lines are how every delimiter is parsed: `char_p('(')`, `char_p(')')`, `char_p('\'')`.

The static\_asserts in the test file serve as a machine-checked specification. They compile with the rest of the project:

```cpp
static_assert(char_p('x')(cursor{"xyz"}).has_value());
static_assert(char_p('x')(cursor{"xyz"}).value().value == 'x');
static_assert(char_p('x')(cursor{"xyz"}).value().rest.peek() == 'y');

static_assert(pure(42)(cursor{"abc"}).has_value());
static_assert(pure(42)(cursor{"abc"}).value().value == 42);
static_assert(pure(42)(cursor{"abc"}).value().rest.remaining() == "abc");

static_assert(!char_p('x')(cursor{"abc"}).has_value());
static_assert(!char_p('x')(cursor{""}).has_value());

static_assert(
    !satisfy([](char c) { return c == 'z'; }, "z")(cursor{""}).has_value());

// map: success
static_assert(
    map(char_p('x'), [](char c) { return int(c); })(cursor{"xyz"}).has_value());
static_assert(map(char_p('x'), [](char c) { return int(c); })(cursor{"xyz"})
                  .value()
                  .value == int('x'));

// map: failure propagates
static_assert(!map(char_p('x'), [](char c) { return int(c); })(cursor{"abc"})
                   .has_value());

// lift2: both succeed
static_assert(lift2(char_p('a'), char_p('b'),
                    [](char a, char b) {
                        return a == 'a' && b == 'b';
                    })(cursor{"ab"})
                  .value()
                  .value == true);

// sequence_left: cursor is past both chars
static_assert(
    sequence_left(char_p('a'), char_p('b'))(cursor{"ab"}).value().value == 'a');
static_assert(sequence_left(char_p('a'), char_p('b'))(cursor{"ab"})
                  .value()
                  .rest.remaining() == "");

// sequence_right: keeps the right value
static_assert(
    sequence_right(char_p('a'), char_p('b'))(cursor{"ab"}).value().value ==
    'b');

// operator|: second branch taken when first fails without consuming
static_assert((char_p('a') | char_p('b'))(cursor{"b"}).has_value());
static_assert((char_p('a') | char_p('b'))(cursor{"b"}).value().value == 'b');

// operator|: both fail
static_assert(!(char_p('a') | char_p('b'))(cursor{"c"}).has_value());

// operator|: first branch succeeds
static_assert((char_p('a') | char_p('b'))(cursor{"a"}).value().value == 'a');
```

Reading these top to bottom tells you the contract precisely: `char_p('x')` on `"xyz"` yields value `'x'` and leaves the cursor at `'y'`; `pure(42)` on `"abc"` yields `42` and leaves the cursor entirely unchanged; `char_p('x')` on `"abc"` or `""` fails. These are not documentation—they are the spec, and the compiler verifies them.


# Functor: `map`

Once you can parse a character, you need to transform the value into something more useful. That is what `map` does:

```cpp
/// Returns a parser that applies @p f to the result of @p pa.
///
/// Fails with @p pa's error if @p pa fails; the error position is preserved
/// so the caller can decide whether to try alternatives.
///
/// @tparam PA Parser type.
/// @tparam F  Callable to apply to the parse value.
template <parser_like PA, class F>
[[nodiscard]] constexpr auto map(PA pa, F f) {
    return parser{[pa, f](cursor cur) {
        auto r = pa(cur);
        if (!r.has_value()) {
            using R = decltype(f(r.value().value));
            return parse_result<R>{r.error()};
        }
        using R = decltype(f(r.value().value));
        return parse_result<R>{
            parse_state<R>{f(r.value().value), r.value().rest}};
    }};
}
```

`map(pa, f)` runs `pa`. If `pa` succeeds, `f` is applied to the value and the cursor position from `pa` is inherited. If `pa` fails, the error is forwarded unchanged. The return type `R` is deduced from `decltype(f(r.value().value))~—the compiler figures out what type ~f` produces.

This is the Functor operation from category theory (written `fmap f pa` in Haskell). A typical use:

```cpp
// Parse a digit character, then convert it to an integer offset from '0'
auto digit_int = map(
    satisfy([](char c) { return c >= '0' && c <= '9'; }, "digit"),
    [](char c) { return c - '0'; });
// digit_int invoked on "5abc" returns parse_result<int>{5, cursor pointing at "abc"}
```

The transformation happens entirely inside the parse result. The cursor positions are managed by the combinator, not by `f`.


# Applicative: `lift2` and Sequencing

Two parsers in sequence, whose results are combined, is the Applicative `lift2`:

```cpp
/// Returns a parser that runs @p pa then @p pb in sequence, combining
/// their values with @p f.
///
/// Fails if either @p pa or @p pb fails, forwarding the earliest error.
///
/// @tparam PA Parser for the first value.
/// @tparam PB Parser for the second value.
/// @tparam F  Binary combiner callable.
template <parser_like PA, parser_like PB, class F>
[[nodiscard]] constexpr auto lift2(PA pa, PB pb, F f) {
    return parser{[pa, pb, f](cursor cur) {
        auto ra = pa(cur);
        if (!ra.has_value()) {
            using V = decltype(f(ra.value().value, pb(cur).value().value));
            return parse_result<V>{ra.error()};
        }
        auto rb = pb(ra.value().rest);
        if (!rb.has_value()) {
            using V = decltype(f(ra.value().value, rb.value().value));
            return parse_result<V>{rb.error()};
        }
        using V = decltype(f(ra.value().value, rb.value().value));
        return parse_result<V>{parse_state<V>{
            f(ra.value().value, rb.value().value), rb.value().rest}};
    }};
}

/// Returns a parser that runs @p pa then @p pb, discarding @p pb's value.
template <parser_like PA, parser_like PB>
[[nodiscard]] constexpr auto sequence_left(PA pa, PB pb) {
    return lift2(pa, pb, [](auto a, auto) { return a; });
}

/// Returns a parser that runs @p pa then @p pb, discarding @p pa's value.
template <parser_like PA, parser_like PB>
[[nodiscard]] constexpr auto sequence_right(PA pa, PB pb) {
    return lift2(pa, pb, [](auto, auto b) { return b; });
}
```

`lift2(pa, pb, f)` runs `pa` first. If it succeeds, it runs `pb` starting from `pa`'s remaining cursor `ra.value().rest~—the two parsers are strictly ordered, and together they consume the concatenation of their individual inputs. If either fails, the earliest error is forwarded. ~f` combines the two values into a single result.

`sequence_left` and `sequence_right` are the two common degenerate cases: run both parsers in order but keep only one value:

```cpp
// Parse '(' then a symbol; return the symbol (discard the paren)
auto after_open = sequence_right(char_p('('), symbol_parser);

// Parse a symbol then ')'; return the symbol (discard the paren)
auto before_close = sequence_left(symbol_parser, char_p(')'));
```

Both are implemented directly on `lift2` with a lambda that ignores one argument. There is no separate machinery.


# Alternative: `operator|`

The choice combinator tries the first parser and, if it fails *without consuming any input*, tries the second:

```cpp
/// Ordered-choice combinator: tries @p pa; if it fails *at the same position*
/// it started, tries @p pb.
///
/// If @p pa consumes input before failing the error is propagated without
/// trying @p pb, which prevents accidental backtracking into already-consumed
/// tokens.
///
/// @tparam PA First alternative parser.
/// @tparam PB Second alternative parser.
template <parser_like PA, parser_like PB>
[[nodiscard]] constexpr auto operator|(PA pa, PB pb) {
    return parser{[pa, pb](cursor cur) {
        auto start = cur.position().offset;
        auto ra = pa(cur);
        if (ra.has_value())
            return ra;
        if (ra.error().where.offset != start)
            return ra;
        return pb(cur);
    }};
}
```

The key line is `if (ra.error().where.offset != start) return ra`. If `pa` consumed at least one character before failing, the offset advanced past `start`, and the error is propagated immediately without trying `pb`. This is ordered choice (the PEG $/$ operator), not full backtracking. It prevents a combinatorial explosion of retries and makes the grammar predictable.

For Scheme's uniform prefix syntax this is exactly right. By the time the reader has consumed `(if`, it is committed to the if-form branch. If something inside the if-form fails, retrying as an application would produce garbage results. The "no consume, no retry" rule makes the grammar deterministic.

The static\_asserts in the test file demonstrate both sides:

```cpp
// second branch taken when first fails without consuming
static_assert((char_p('a') | char_p('b'))(cursor{"b"}).value().value == 'b');

// first success wins, second never runs
static_assert((char_p('a') | char_p('b'))(cursor{"a"}).value().value == 'a');

// both alternatives fail
static_assert(!(char_p('a') | char_p('b'))(cursor{"c"}).has_value());
```


# Repetition: `many` and `some`

Repeating a parser zero or more times requires a fixed-capacity container because dynamic allocation is unavailable in `constexpr`. `many` uses `foundation::static_vector<V, Capacity>`:

```cpp
/// Returns a parser that applies @p p zero or more times, collecting at most
/// @c Capacity results into a @ref foundation::static_vector.
///
/// Always succeeds (zero repetitions is valid).
///
/// @tparam Capacity Maximum number of repetitions.
/// @tparam P        Parser to repeat.
template <int Capacity, parser_like P>
[[nodiscard]] constexpr auto many(P p) {
    return parser{[p](cursor cur) {
        using V = decltype(p(cur).value().value);
        foundation::static_vector<V, Capacity> result{};
        while (result.size() < Capacity) {
            auto r = p(cur);
            if (!r.has_value())
                break;
            result.push_back(r.value().value);
            cur = r.value().rest;
        }
        return parse_result<foundation::static_vector<V, Capacity>>{
            parse_state<foundation::static_vector<V, Capacity>>{result, cur}};
    }};
}
```

`many<Capacity>(p)` loops until `p` fails or the vector is full, always succeeding (zero matches is a valid result). `some<Capacity>(p)` requires at least one match and propagates the error if `p` immediately fails.

The `Capacity` is a non-type template parameter. This is a `constexpr` constraint: the compiler must know the maximum number of repetitions at compile time. For digit parsing (at most 20 digits in a 64-bit integer), `some<20>(digit_p)` is exact. For symbol tail characters (up to 64 characters), `many<64>(symbol_char_p)` is generous but bounded. The bounds are part of the type.


# Whitespace: `lexeme`

Token-level whitespace handling is handled by wrapping a parser with `lexeme`:

```cpp
/// Returns a parser that strips leading and trailing inter-token whitespace
/// around @p p.
///
/// This is the standard way to make a token parser whitespace-insensitive
/// in a recursive-descent setting.
///
/// @tparam P Parser to wrap.
template <parser_like P>
[[nodiscard]] constexpr auto lexeme(P p) {
    return parser{[p](cursor cur) {
        auto start = skip_intertoken_space(cur);
        auto r = p(start);
        if (!r.has_value())
            return r;
        auto rest = skip_intertoken_space(r.value().rest);
        using V = decltype(r.value().value);
        return parse_result<V>{parse_state<V>{r.value().value, rest}};
    }};
}
```

`lexeme(p)` calls `skip_intertoken_space` before running `p`, then skips whitespace again after. This is the standard Parsec approach: make each token parser responsible for consuming the whitespace that follows it. The reader uses this when composing token parsers so that `(if #t 1 2)` and `( if #t 1 2 )` parse identically—whitespace is not structurally significant in Scheme.


# Putting It Together: Real Scheme Atom Parsers

With all the combinators in hand, here are the actual parsers the reader uses for Scheme atoms:

```cpp
/// Returns a parser for a Scheme integer literal.
///
/// Handles an optional leading @c - sign followed by one or more decimal
/// digits (up to 20 digits). The value is computed without allocating
/// intermediate strings.
[[nodiscard]] constexpr auto integer_p() {
    return parser::parser{[](parser::cursor cur)
                              -> parser::parse_result<atom_integer> {
        auto sign_r = parser::optional(parser::char_p('-'))(cur);
        bool negative = sign_r.value().value.has_value();
        auto after_sign = sign_r.value().rest;

        auto digits = parser::some<20>(parser::satisfy(
            [](char c) { return c >= '0' && c <= '9'; }, "digit"))(after_sign);
        if (!digits.has_value()) {
            return parser::parse_result<atom_integer>{digits.error()};
        }
        auto &d = digits.value().value;
        auto rest = digits.value().rest;

        int sign = negative ? -1 : 1;
        int n = 0;
        for (int i = 0; i < d.size(); ++i)
            n = n * 10 + (d[i] - '0');
        return parser::parse_result<atom_integer>{
            parser::parse_state<atom_integer>{atom_integer{sign * n}, rest}};
    }};
}
```

`integer_p()` is written directly as a `parser{lambda}` rather than composed from smaller combinators, because the digit-to-integer accumulation loop is clearer as imperative code. It still uses the primitives: `optional(char_p('-'))` for the optional sign, `some<20>(satisfy(digit_pred, "digit"))` for the digit sequence. The `optional` always succeeds—it returns `std::optional<char>` with `nullopt` when there is no minus sign. The parser then accumulates the integer value with a multiply-and-add loop over the `static_vector` of digit characters.

```cpp
/// Returns a parser for a Scheme symbol.
///
/// A symbol starts with a letter or operator character (per
/// @ref parser::is_initial_symbol_char) and continues with zero or more
/// symbol characters (per @ref parser::is_symbol_char). Up to 64 tail
/// characters are accepted. The returned @c atom_symbol::name is a view
/// into the original source.
[[nodiscard]] constexpr auto symbol_p() {
    return parser::parser{
        [](parser::cursor cur) -> parser::parse_result<atom_symbol> {
            auto start = cur;
            auto first =
                parser::satisfy(parser::is_initial_symbol_char, "symbol")(cur);
            if (!first.has_value())
                return parser::parse_result<atom_symbol>{first.error()};
            auto rest_cur = first.value().rest;
            auto tail = parser::many<64>(parser::satisfy(
                parser::is_symbol_char, "symbol char"))(rest_cur);
            auto end_cur = tail.value().rest;
            int len = end_cur.position().offset - start.position().offset;
            auto name = start.remaining().substr(0, len);
            return parser::parse_result<atom_symbol>{
                parser::parse_state<atom_symbol>{atom_symbol{name}, end_cur}};
        }};
}
```

`symbol_p()` shows a subtlety: Scheme symbols have two distinct character classes—the initial character must be a letter or operator symbol (`+`, `-`, `*`, `/`, `=`, `<`, `>`, `!`, `?`), but subsequent characters can also include digits. This requires two separate `satisfy` calls: one for the first character and `many<64>` for the tail. Rather than collecting characters into a `static_vector` and joining them, the parser computes the symbol's length by subtracting the byte offsets of the start and end cursors, then extracts a `string_view` directly into the original source. No allocation, no copy—the symbol text is a zero-cost view into the input string.

Both parsers are combined with `|` in the reader's full datum dispatcher. The reader tries `integer_p()` first; if that fails without consuming, it tries `symbol_p()`; if that fails, it tries boolean, quote, and list—all via the same `|` operator.


# The Design in Sum

The parser combinator library is built from five conceptual layers:

1.  **Input**: `cursor~—immutable, position-tracking view into source text. 2. *Output*: ~parse_result<T>~—either ~parse_state<T>{value, remaining}` or `parse_error{position, message}`. 3. **Wrapper**: `parser<F>~—any callable with signature ~parse_result<T>(cursor)` is a parser. 4. **Primitives**: `pure`, `satisfy`, `char_p~—the irreducible building blocks. 5. *Combinators*: ~map` (Functor), `lift2~/~sequence_left~/~sequence_right` (Applicative), `operator|` (Alternative), `many~/~some~/~optional`, `lexeme`.

Everything in the reader is assembled from these five layers with no heap allocation, and all parsing behavior is verified at compile time through the `static_assert` suite. The type system enforces that parsers are combined correctly—mismatching types fail to compile rather than producing wrong results at runtime.


# References

-   Leijen, D., & Meijer, E. (2001). "Parsec: Direct Style Monadic Parser Combinators for the Real World." Department of Computer Science, Universiteit Utrecht.
-   Hutton, G., & Meijer, E. (1992). "Monadic Parser Combinators." Technical Report NOTTCS-TR-92-4.
-   McBride, C., & Paterson, R. (2008). "Applicative Programming with Effects." *Journal of Functional Programming*, 18(1).
