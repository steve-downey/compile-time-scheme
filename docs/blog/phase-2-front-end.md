<div class="abstract" id="org4e15526">
<p>
Parsing Scheme starts with an immutable cursor and a library of applicative
combinators. No raw function pointers, no mutation, no heap — just composition.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 1 - Foundation ←](phase-1-foundation.md)

</nav>


# The Front End: Parsing with Applicative Combinators

Before I can elaborate, evaluate, or compile anything, I need to turn source text into a tree. The front end does that work: a cursor that tracks position without mutating, a result type that models success and failure, and a library of parser combinators that snap together like LEGO.

The entire parser is `constexpr`. No allocation escapes. Everything composes from the same five primitive forms.


## The Immutable Cursor

```c++
// src/smd/smdscheme/parser/cursor.hpp
class cursor {
    std::string_view input_{};
    foundation::source_pos pos_{};

  public:
    constexpr explicit cursor(std::string_view input) : input_{input} {}

    constexpr auto empty() const -> bool { return input_.empty(); }
    constexpr auto peek() const -> char { return input_.front(); }

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

    constexpr auto position() const -> foundation::source_pos { return pos_; }
    constexpr auto remaining() const -> std::string_view { return input_; }
};
```

`peek()` reads the next character without consuming it. `bump()` returns a brand-new cursor with one character removed from the front and the position updated. Neither mutates anything. Backtracking is free: to "go back," a parser discards the advanced cursor and uses the checkpoint it saved earlier — no undo stack, no state restoration.

The position tracks line, column, and absolute byte offset. The offset is the critical field for the alternative combinator: it detects whether a parser consumed any input before failing.


## Lexical Primitives

The character classification predicates live in the same header:

```c++
// src/smd/smdscheme/parser/cursor.hpp
constexpr auto is_space(char c) -> bool {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

constexpr auto is_initial_symbol_char(char c) -> bool {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        return true;
    }
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '=' ||
           c == '<' || c == '>' || c == '!' || c == '?';
}

constexpr auto is_symbol_char(char c) -> bool {
    return is_initial_symbol_char(c) || (c >= '0' && c <= '9');
}

constexpr auto is_delimiter(char c) -> bool {
    return is_space(c) || c == '(' || c == ')' || c == '\'';
}

constexpr auto skip_intertoken_space(cursor cur) -> cursor {
    while (!cur.empty() && is_space(cur.peek())) {
        cur = cur.bump();
    }
    return cur;
}
```

`is_initial_symbol_char` and `is_symbol_char` encode Scheme's two-phase symbol alphabet: the initial character must be a letter or operator symbol, but subsequent characters can also include digits. `is_delimiter` detects token boundaries without consuming them. `skip_intertoken_space` is a plain loop — no parser abstraction needed for something this simple.


## Parser Objects

Every parser is a callable that takes a `cursor` and returns a `parse_result<T>`. The `parse_result<T>` is an alias for `foundation::result<parse_state<T>>` — either a value plus the remaining cursor, or a `parse_error` with position and message:

```c++
// src/smd/smdscheme/parser/parser.hpp
template <class T>
struct parse_state {
    T value;
    cursor rest;
};

template <class T>
using parse_result = foundation::result<parse_state<T>>;
```

Parsers cannot be raw function pointers because they need captures: a combinator closes over sub-parsers. Instead, I wrap any callable `F` with the right signature in a `parser<F>` class:

```c++
// src/smd/smdscheme/parser/parser.hpp
template <class F>
class parser {
  public:
    constexpr explicit parser(F f) : f_{f} {}
    constexpr auto operator()(cursor cur) const { return f_(cur); }
  private:
    F f_;
};

template <class F>
parser(F) -> parser<F>;
```

The class is intentionally thin. It stores `F` by value (critical for `constexpr` — lambda addresses are unavailable at compile time), exposes `operator()(cursor)`, and satisfies the `parser_like` concept. The CTAD guide means `parser{lambda}` works without spelling out `F`.


## Primitive Parsers

Two functions build every other parser in the library.

`pure(v)` always succeeds, consumes nothing, and returns `v`:

```c++
// src/smd/smdscheme/parser/parser.hpp
template <class T>
[[nodiscard]] constexpr auto pure(T value) {
    return parser{[v = value](cursor cur) -> parse_result<T> {
        return parse_state<T>{v, cur};
    }};
}
```

`satisfy(pred, desc)` is the single atomic building block:

```c++
// src/smd/smdscheme/parser/parser.hpp
[[nodiscard]] constexpr auto satisfy(auto pred, char const *expected) {
    return parser{[pred, expected](cursor cur) -> parse_result<char> {
        if (!cur.empty() && pred(cur.peek())) {
            return parse_state<char>{cur.peek(), cur.bump()};
        }
        return foundation::parse_error{cur.position(), expected};
    }};
}

[[nodiscard]] constexpr auto char_p(char expected) {
    return satisfy([expected](char c) { return c == expected; },
                   "expected char");
}
```

Every delimiter is parsed with two lines: `char_p('(')`, `char_p(')')`, `char_p('\'')`.


## Applicative Combinators

With the primitives in place, I can lift Applicative composition into the parser library (McBride, Conor and Paterson, Ross, 2008). The idea: build complex parsers by combining simple ones without monadic bind (Hutton, Graham and Meijer, Erik, 1992). Sequential composition and transformation are enough for a Scheme lexer.

`map` applies a function to a successfully parsed value:

```c++
// src/smd/smdscheme/parser/parser.hpp
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

`lift2` sequences two parsers and combines their values:

```c++
// src/smd/smdscheme/parser/parser.hpp
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
```

`pb` runs starting at `ra.value().rest` — the two parsers are strictly ordered and together consume the concatenation of their individual inputs. The degenerate cases `sequence_left` and `sequence_right` drop one of the two values:

```c++
// src/smd/smdscheme/parser/parser.hpp
template <parser_like PA, parser_like PB>
[[nodiscard]] constexpr auto sequence_left(PA pa, PB pb) {
    return lift2(pa, pb, [](auto a, auto) { return a; });
}

template <parser_like PA, parser_like PB>
[[nodiscard]] constexpr auto sequence_right(PA pa, PB pb) {
    return lift2(pa, pb, [](auto, auto b) { return b; });
}
```

Both are one-liners on top of `lift2`. There is no separate machinery.


## Alternative: `operator|`

The choice combinator tries the first parser and, if it fails **without consuming any input**, tries the second:

```c++
// src/smd/smdscheme/parser/parser.hpp
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

If `pa` consumed at least one character before failing, the offset advanced past `start` and the error is propagated immediately without trying `pb`. This is ordered choice, not full backtracking. For Scheme's uniform prefix syntax this is exactly right: once the reader has consumed `(if`, it is committed to the if-form branch.


## Repetition: `many` and `some`

Repeating a parser zero or more times requires a fixed-capacity container — dynamic allocation is unavailable in `constexpr`. `many` collects results into the `foundation::static_vector` introduced in Phase 1:

```c++
// src/smd/smdscheme/parser/alt.hpp
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

`many<Capacity>(p)` loops until `p` fails or the vector is full, always succeeding (zero matches is a valid result). `some<Capacity>(p)` requires at least one match and propagates the error if `p` immediately fails. The `Capacity` is a non-type template parameter — the compiler must know the maximum repetitions at compile time.

`lexeme(p)` strips surrounding inter-token whitespace before and after `p`:

```c++
// src/smd/smdscheme/parser/alt.hpp
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

This is the standard Parsec approach (Leijen, Daan and Meijer, Erik, 2001): make each token parser responsible for the whitespace that follows it. `(if #t 1 2)` and `( if #t 1 2 )` parse identically.


## Compile-Time Specification

The `static_assert` suite in the test file is not documentation — it is the machine-checked specification:

```c++
static_assert(char_p('x')(cursor{"xyz"}).has_value());
static_assert(char_p('x')(cursor{"xyz"}).value().value == 'x');
static_assert(char_p('x')(cursor{"xyz"}).value().rest.peek() == 'y');

static_assert(pure(42)(cursor{"abc"}).value().value == 42);
static_assert(pure(42)(cursor{"abc"}).value().rest.remaining() == "abc");

static_assert(!char_p('x')(cursor{"abc"}).has_value());

static_assert((char_p('a') | char_p('b'))(cursor{"b"}).value().value == 'b');
static_assert(!(char_p('a') | char_p('b'))(cursor{"c"}).has_value());
```

Reading these top to bottom tells you the contract precisely. The compiler verifies them on every build.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [Next: Phase 3 - Reader →](phase-3-reader.md)

</nav>


# References

Hutton, Graham and Meijer, Erik (1992). **Monadic Parser Combinators**, Technical Report NOTTCS-TR-92-4. (Hutton, Graham and Meijer, Erik, 1992)

Leijen, Daan and Meijer, Erik (2001). **Parsec: Direct Style Monadic Parser Combinators for the Real World**, Department of Computer Science, Universiteit Utrecht. (Leijen, Daan and Meijer, Erik, 2001)

McBride, Conor and Paterson, Ross (2008). **Applicative Programming with Effects**, Journal of Functional Programming. (McBride, Conor and Paterson, Ross, 2008)
