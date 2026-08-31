# Handoff to B3

## Exact spellings, since your own step file names things loosely

`src/smd/kit/parser/parser.hpp`: `parser<F>` (CTAD via `parser{lambda}`),
`parse_result<T> = foundation::result<parse_state<T>>`, `pure(T value)`,
`map(P p, F f)`, and
`template <class P, class Ctx> concept parser_like = requires(P p, cursor c, Ctx &ctx) { p(c, ctx); };`.
**Correction to your step file's "what already exists" list: `bind` is not
declared in `parser.hpp`.** It is the generic `smd::kit::foundation::bind`
CPO (`<smd/kit/foundation/monad.hpp>`), and `parser<F>` is registered
against it as a Monad instance in the new
`src/smd/kit/parser/parser_instances.hpp` (`parser_monad_impl`,
`parser_monad_map`, `monad_typeclass<parser<F>>`). Include that header and
either qualify `smd::kit::foundation::bind(...)` or bring it in with a
`using` declaration — a CPO is an object, ADL will not find it from a
`parser<F>` argument. `parser_like` itself has no user yet; nothing
constrains anything with it, so it is currently decoration, not a caught
mistake.

`src/smd/kit/parser/parse_context.hpp`:
`template <class T> concept parse_context = !std::same_as<std::remove_cvref_t<T>, cursor>;`
and `struct no_context {};`. Neither is a default template/function
argument anywhere, by design (D29).

## Where `token_p` landed

`src/smd/cl/reader/detail/read_context.hpp`, inside `namespace
smd::cl::reader::detail`, immediately after the new `reader_context`
concept and before `add_leaf_checked`. Same file also now carries `using
smd::kit::foundation::bind;` right next to the pre-existing `using
foundation::and_then;`.

## `reader_context`: decoration so far, not yet a caught mistake

`reader_context` (same file, same namespace) refines `parse_context` and
requires `ctx.tree`, `ctx.symbols`, `ctx.table`, `typename Ctx::child_list`.
Only `read_radix_number` is constrained with it — B2 did not hit a real
argument-order bug it caught; it is there because D29's design calls for a
named concept, not because it fired. `readtable const` is a different shape
entirely (no `.tree`/`.symbols`/`.child_list`), so it will satisfy
`parse_context` but not `reader_context` — that's expected and matches your
step file's plan to thread `readtable const` directly rather than wrapping
it.

## The parser-equality test helper — reuse this shape verbatim

Two parsers are equal when they produce the same `parse_result` for the
same input and context. No shared header exists for this (none is in
either step's declared scope), so it is duplicated once per test file today:

```cpp
template <class P, class Ctx>
constexpr auto run(P const &p, std::string_view text, Ctx &ctx) {
    return p(cursor{text}, ctx);
}

template <class P, class Q, class Ctx>
constexpr auto same_parse(P const &p, Q const &q, std::string_view text,
                          Ctx &ctx) -> bool {
    return run(p, text, ctx) == run(q, text, ctx);
}
```

It lives in `src/smd/kit/parser/parser.test.cpp` and
`parser_instances.test.cpp` today. Copy the same shape into
`repeat.test.cpp` rather than inventing a variant — if a third copy makes
duplication the wrong call, that is a `docs/backlog/BL-` candidate, not
something to solve mid-step.

## Compile time: no red flag, but no rigorous D30 measurement either

`make test-matrix` wall time before/after this step's whole diff: Debug
leg ~1.05s to ~1.08s, Asan leg ~6.95s to ~6.74-7.23s (both within normal
run-to-run noise for this suite). That is an incidental observation from
running the matrix repeatedly, not a controlled compile-time-only
measurement — B8 collecting D30's real numbers should not treat this as
that evidence.

## A namespace gotcha, in case a later step in this series needs it

Not applicable to B3 as scoped (you are adding plain functions, no new
typeclass instance), but recorded so nobody rediscovers it under time
pressure: a variable-template partial specialization (like
`monad_typeclass<parser<F>>`) must be declared in a namespace that
*encloses* the primary template's namespace. `smd::kit::parser` does not
enclose `smd::kit::foundation` — they're siblings — so such a
specialization cannot be written from inside `namespace smd::kit::parser`
even fully qualified; it needs its own `namespace smd::kit::foundation { … }`
block naming the parser type fully qualified, the same shape
`src/smd/cl/foundation/tagged_tree_instances.hpp` already uses. Confirmed
against GCC 16 directly, not inferred.
