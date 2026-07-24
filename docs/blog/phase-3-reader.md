<div class="abstract" id="orgb1f52e1">
<p>
The reader takes a string and produces a tree of datums — integers, booleans,
symbols, lists, and quoted forms. It sees data, not programs. That separation
is the key to homoiconicity.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 2 - Front End ←](phase-2-front-end.md)

</nav>


# The Reader: Turning Text into Datum Trees

Before the elaborator can classify an `if` or a `lambda`, before evaluation can run, something has to take the raw source string and produce structure. That is the reader's job. It asks only one question of every character sequence: is this a valid Scheme datum? It never asks what the datum means.

This separation is not accidental. It is homoiconicity: the same tree that represents data also represents code — the data/code duality of S-expressions that goes back to McCarthy's original Lisp (McCarthy, John, 1960). The reader produces the same structure for the list `(1 2 3)` and for the expression `(+ 1 2)`. The symbol `+` and the integer `1` are just atoms — the reader does not know that `+` is addition. That knowledge belongs to the elaborator.


## Datum Types

A Scheme datum is one of five things: an integer, a symbol, a boolean, a list, or a quoted form. I model each as a struct:

```c++
// src/smd/smdscheme/reader/datum_type.hpp
struct datum_integer { int value{}; };
struct datum_symbol  { std::string_view name{}; };
struct datum_boolean { bool value{}; };
```

The recursive cases — list and quote — need to refer to child datums. The open-recursive fixpoint pattern does the work here.

A `datum_list` holds a `static_vector` of `arena_box` handles:

```c++
// src/smd/smdscheme/reader/datum_type.hpp
template <typename R, int MaxNodes, int MaxList>
struct datum_list {
    foundation::static_vector<foundation::arena_box<R, MaxNodes>, MaxList>
        elements{};
};
```

`R` is the self-referential element type — the recursive datum. Rather than embedding a `datum` directly (which would make the struct infinitely large), `datum_list` stores integer handles into an arena. The handles are typed as `arena_box<R, MaxNodes>`: they name an index in a `tree_arena` of capacity `MaxNodes`. The children live in the arena; the list only holds their addresses.

`datum_quote` is similar:

```c++
// src/smd/smdscheme/reader/datum_type.hpp
template <typename R, int MaxNodes>
struct datum_quote {
    foundation::arena_box<R, MaxNodes> quoted{};
};
```


## The Open-Recursive Factory

With the leaf and recursive structs in hand, I need to tie the knot. The open-recursive factory `datum_f_factory` expresses the one-layer variant without committing to the self-reference:

```c++
// src/smd/smdscheme/reader/datum_type.hpp
template <int MaxNodes, int MaxList>
struct datum_f_factory {
    template <typename R>
    using type = std::variant<datum_integer, datum_symbol, datum_boolean,
                              datum_list<R, MaxNodes, MaxList>,
                              datum_quote<R, MaxNodes>>;
};

template <int MaxNodes, int MaxList>
using datum_type =
    foundation::fix<datum_f_factory<MaxNodes, MaxList>::template type>;
```

`foundation::fix<F>` wraps `F<fix<F>>` so `datum_type` is the recursive type. The template parameters `MaxNodes` and `MaxList` are compile-time capacities: the arena can hold at most `MaxNodes` nodes, and each list can hold at most `MaxList` children. Both are statically checked — no allocation, no overflow.


## Atom Parsers

The leaf parsers are built from the combinator library introduced in Phase 2. `integer_p` combines `optional(char_p('-'))` with `some<20>(digit)` and accumulates the result without any intermediate string allocation:

```c++
// src/smd/smdscheme/reader/atom.hpp
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
        int sign = negative ? -1 : 1;
        int n = 0;
        for (int i = 0; i < digits.value().value.size(); ++i)
            n = n * 10 + (digits.value().value[i] - '0');
        return parser::parse_result<atom_integer>{
            parser::parse_state<atom_integer>{atom_integer{sign * n},
                                              digits.value().rest}};
    }};
}
```

`symbol_p` uses `is_initial_symbol_char` for the first character and `is_symbol_char` for the tail. The result is a `string_view` into the original source, so no copy is made:

```c++
// src/smd/smdscheme/reader/atom.hpp
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


## The Recursive Descent

`read_datum_node` does the recursive descent: it dispatches on the first non-whitespace character and recursively builds the datum tree into the arena:

```c++
// src/smd/smdscheme/reader/read_datum.hpp
template <int MaxNodes, int MaxList>
constexpr auto read_datum_node(
    parser::cursor cur,
    foundation::tree_arena<datum_type<MaxNodes, MaxList>, MaxNodes> &arena)
    -> parser::parse_result<datum_type<MaxNodes, MaxList>> {
    using datum = datum_type<MaxNodes, MaxList>;
    using datum_f =
        typename datum_f_factory<MaxNodes, MaxList>::template type<datum>;

    cur = skip_intertoken_space(cur);
    if (cur.empty())
        return foundation::parse_error{cur.position(), "unexpected end of input"};

    char c = cur.peek();

    if (c == '#') { /* boolean */ }
    if (c == '\'') { /* quote  */ }
    if (c == '(')  { /* list   */ }
    /* integer and symbol as fallthrough */
}
```

The list case accumulates arena handles as it goes:

```c++
if (c == '(') {
    parser::cursor after = cur.bump();
    datum_list<datum, MaxNodes, MaxList> list{};
    while (true) {
        after = skip_intertoken_space(after);
        if (after.empty())
            return foundation::parse_error{after.position(), "expected ')'"};
        if (after.peek() == ')')
            return parser::parse_state<datum>{datum{datum_f{list}}, after.bump()};
        auto elem = read_datum_node<MaxNodes, MaxList>(after, arena);
        if (!elem.has_value()) return elem;
        list.elements.push_back(make_arena_box(arena, elem.value().value));
        after = elem.value().rest;
    }
}
```

Each child datum is allocated into the arena with `make_arena_box`, which returns an `arena_box` handle. The list stores handles, not values, so there is no copying of child trees — only integer indices accumulate in the `static_vector`.


## Quote Preservation

The reader does not lower `'x` to `(quote x)`. It preserves the source notation as `datum_quote`:

```c++
if (c == '\'') {
    parser::cursor after = cur.bump();
    auto inner = read_datum_node<MaxNodes, MaxList>(after, arena);
    if (!inner.has_value()) return inner;
    datum d{datum_f{datum_quote<datum, MaxNodes>{
        make_arena_box(arena, inner.value().value)}}};
    return parser::parse_state<datum>{d, inner.value().rest};
}
```

The elaborator will later decide what `quote` means. The reader's job is only to record what was written.


## The Public Entry Point

`read_datum` strips the `parse_result` wrapper and converts to `foundation::result`, which is the error-handling type used throughout the pipeline:

```c++
// src/smd/smdscheme/reader/read_datum.hpp
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto read_datum(
    parser::cursor cur,
    foundation::tree_arena<datum_type<MaxNodes, MaxList>, MaxNodes> &arena)
    -> foundation::result<parser::parse_state<datum_type<MaxNodes, MaxList>>> {
    auto r = detail::read_datum_node<MaxNodes, MaxList>(cur, arena);
    if (!r.has_value()) return r.error();
    return r.value();
}
```

At the call site, the whole reader is `constexpr`:

```c++
constexpr auto run() {
    using namespace smd::smdscheme;
    foundation::tree_arena<reader::datum_type<64, 16>, 64> arena{};
    auto r = reader::read_datum(parser::cursor{"(+ 1 2)"}, arena);
    return r.has_value();
}
static_assert(run());
```

The arena and the datum tree live entirely in constant-evaluation memory. No heap allocation, no runtime cost.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [Next: Phase 4 - Elaboration →](phase-4-elaboration.md)

</nav>


# References

McCarthy, John (1960). **Recursive Functions of Symbolic Expressions and Their Computation by Machine, Part I**, Communications of the ACM. (McCarthy, John, 1960)
