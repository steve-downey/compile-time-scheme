<div class="abstract">
<p>
The elaborator transforms raw datum trees into a typed core AST. It recognizes
<code>if</code>, <code>lambda</code>, <code>quote</code>, <code>define</code>, <code>let</code>, and <code>let*</code> — turning unstructured
lists into semantic program nodes.
</p>
</div>

# Assigning Meaning to Structure

The reader produces a tree of data. It does not know that `(if #t 1 2)` is a
conditional — it sees a four-element list. The elaborator is where that
distinction is finally made. I walk the datum tree, inspect the shapes of
lists, and emit a strongly-typed core AST whose node types encode the
semantics of the program.

This separation is not accidental [cite:@abelson1996sicp]. A reader that
understands keywords would couple syntax and semantics in ways that make the
reader harder to reuse and the language harder to extend. Keeping them apart
means the reader never needs to change when the language gains new special
forms.

# The Core AST Types

The elaborated representation lives in `elaborated_core.hpp`. Unlike the
datum tree — which is a homogeneous variant — the core AST has node types
that carry exactly the structure required by each semantic form.

Leaf nodes are plain structs:

```cpp
// src/smd/smdscheme/elaborator/elaborated_core.hpp
struct core_integer { int value; };
struct core_boolean { bool value; };
struct core_symbol  { std::string_view name; };

struct core_quote {
    std::variant<int, bool, std::string_view> atom;
};
```

`core_quote` holds only atoms — integers, booleans, and symbols. Nested
list quotation is not yet supported. `core_symbol` stores a view into the
source string, so no allocation is required.

Recursive nodes are parameterized on the recursive self-reference `R` and
arena capacity:

```cpp
// src/smd/smdscheme/elaborator/elaborated_core.hpp
template <typename R, int MaxNodes>
struct core_if {
    foundation::arena_box<R, MaxNodes> condition;
    foundation::arena_box<R, MaxNodes> consequent;
    foundation::arena_box<R, MaxNodes> alternative;
};

template <typename R, int MaxNodes, int MaxList>
struct core_lambda {
    foundation::static_vector<std::string_view, MaxList> params;
    foundation::arena_box<R, MaxNodes> body;
};

template <typename R, int MaxNodes, int MaxList>
struct core_application {
    foundation::arena_box<R, MaxNodes> func;
    foundation::static_vector<foundation::arena_box<R, MaxNodes>, MaxList> args;
};

template <typename R, int MaxNodes>
struct core_define {
    std::string_view name;
    foundation::arena_box<R, MaxNodes> value;
};
```

The `arena_box<R, MaxNodes>` handles are integer indices into a
`tree_arena`. Children do not nest recursively by value — they live in the
arena and are referenced by index. This is the same arena pattern as the
datum tree, keeping everything `constexpr`-friendly.

The open-recursive variant factory ties it all together:

```cpp
// src/smd/smdscheme/elaborator/elaborated_core.hpp
template <int MaxNodes, int MaxList>
struct core_f_factory {
    template <typename R>
    using type =
        std::variant<core_integer, core_boolean, core_symbol, core_quote,
                     core_if<R, MaxNodes>, core_lambda<R, MaxNodes, MaxList>,
                     core_application<R, MaxNodes, MaxList>,
                     core_define<R, MaxNodes>>;
};

template <int MaxNodes, int MaxList>
using core_type =
    foundation::fix<core_f_factory<MaxNodes, MaxList>::template type>;
```

`core_type` is the fixed point of `core_f_factory`. The same `Fix<F>`
combinator that drives the datum tree drives the core tree. Adding a new
node type means extending the variant — the Fix machinery does not change.

# Recognizing Special Forms

The entry point `elaborate` calls `elaborate_node`, which dispatches on
the datum variant. Atoms map directly to their core counterparts. Lists go
to `elaborate_list`, which inspects the head element.

```cpp
// src/smd/smdscheme/elaborator/elaborate.hpp
auto const &first = datum_arena.get(lst.elements[0]);
if (std::holds_alternative<reader::datum_symbol>(first.inner)) {
    auto name = std::get<reader::datum_symbol>(first.inner).name;

    if (name == "if")     { /* ... */ }
    if (name == "quote")  { /* ... */ }
    if (name == "define") { /* ... */ }
    if (name == "lambda") { /* ... */ }
    if (name == "let")    { /* ... */ }
    if (name == "let*")   { /* ... */ }
}
// default: general application
```

Every recognized keyword validates arity before doing anything else. `if`
must have exactly four elements (keyword plus three arguments):

```cpp
// src/smd/smdscheme/elaborator/elaborate.hpp
if (name == "if") {
    if (lst.elements.size() != 4)
        return foundation::parse_error{{}, "if: expected 3 arguments"};

    auto cond_r = elaborate_node<MaxNodes, MaxList>(
        datum_arena.get(lst.elements[1]), datum_arena, core_arena);
    if (!cond_r.has_value()) return cond_r;

    auto cons_r = elaborate_node<MaxNodes, MaxList>(
        datum_arena.get(lst.elements[2]), datum_arena, core_arena);
    if (!cons_r.has_value()) return cons_r;

    auto alt_r = elaborate_node<MaxNodes, MaxList>(
        datum_arena.get(lst.elements[3]), datum_arena, core_arena);
    if (!alt_r.has_value()) return alt_r;

    return core{core_f{core_if<core, MaxNodes>{
        make_arena_box(core_arena, std::move(cond_r.value())),
        make_arena_box(core_arena, std::move(cons_r.value())),
        make_arena_box(core_arena, std::move(alt_r.value()))}}};
}
```

Each sub-expression is elaborated recursively before the parent node is
constructed. `make_arena_box` allocates the child into the core arena and
returns an integer handle. The parent node stores those handles — no raw
pointers, no heap allocation.

`lambda` elaboration also checks that the formals list contains only
symbols and has no duplicates:

```cpp
// src/smd/smdscheme/elaborator/elaborate.hpp
for (int i = 0; i < formals.elements.size(); ++i) {
    auto const &p = datum_arena.get(formals.elements[i]);
    if (!std::holds_alternative<reader::datum_symbol>(p.inner))
        return foundation::parse_error{{}, "lambda: formal must be a symbol"};
    auto p_name = std::get<reader::datum_symbol>(p.inner).name;
    for (auto const &existing : lam.params) {
        if (existing == p_name)
            return foundation::parse_error{{}, "duplicate parameter"};
    }
    lam.params.push_back(p_name);
}
```

The duplicate-parameter check is an O(n^2) linear scan over a small
`static_vector`. At compile time with small parameter lists this is fine.

# let Desugaring

`let` introduces no new AST node. It desugars to a lambda applied to its
binding expressions:

```
(let ((x e1) (y e2)) body)
  -> ((lambda (x y) body) e1 e2)
```

The elaborator builds a `core_lambda` with the parameter names and the
elaborated body, then wraps it in a `core_application` with the elaborated
binding values as arguments:

```cpp
// src/smd/smdscheme/elaborator/elaborate.hpp
core_lambda<core, MaxNodes, MaxList> lam{};
foundation::static_vector<DatumHandle, MaxList> arg_ids;

for (int i = 0; i < bindings.elements.size(); ++i) {
    // ... extract name and value handle from each (name expr) pair ...
    lam.params.push_back(p_name);
    arg_ids.push_back(pair.elements[1]);
}

lam.body = make_arena_box(core_arena, std::move(body_r.value()));

core_application<core, MaxNodes, MaxList> app{};
app.func = make_arena_box(core_arena, core{core_f{std::move(lam)}});

for (int i = 0; i < arg_ids.size(); ++i) {
    auto arg_r = elaborate_node<MaxNodes, MaxList>(
        datum_arena.get(arg_ids[i]), datum_arena, core_arena);
    app.args.push_back(make_arena_box(core_arena, std::move(arg_r.value())));
}

return core{core_f{std::move(app)}};
```

The binding expressions are elaborated in the outer scope — they cannot
see each other or the bound names. That is the semantics of `let`: all
bindings are evaluated before any of them are in scope.

# let\* Desugaring

`let*` has different semantics: each binding is in scope for subsequent
bindings. This translates to nested single-binding lambdas:

```
(let* ((x e1) (y e2)) body)
  -> ((lambda (x) ((lambda (y) body) e2)) e1)
```

I build this inside-out, starting from the body and wrapping one layer per
binding in reverse order:

```cpp
// src/smd/smdscheme/elaborator/elaborate.hpp
auto inner = elaborate_node<MaxNodes, MaxList>(
    datum_arena.get(lst.elements[2]), datum_arena, core_arena);

for (int i = bindings.elements.size() - 1; i >= 0; --i) {
    // ... extract bname and val_r from binding[i] ...

    core_lambda<core, MaxNodes, MaxList> lam{};
    lam.params.push_back(bname);
    lam.body = make_arena_box(core_arena, std::move(inner.value()));

    core_application<core, MaxNodes, MaxList> app{};
    app.func = make_arena_box(core_arena, core{core_f{std::move(lam)}});
    app.args.push_back(make_arena_box(core_arena, std::move(val_r.value())));

    inner = foundation::result<core>{core{core_f{std::move(app)}}};
}

return inner;
```

Each iteration takes the current `inner` node (initially the body,
subsequently the partially-built application), wraps it in a
`((lambda (name) inner) val)` application, and replaces `inner` with the
result. After the loop, `inner` holds the fully nested structure.

The empty-bindings case is handled at the top: if there are no bindings,
`let*` reduces to evaluating the body directly.

# Error Propagation

Every elaboration step returns `foundation::result<core_type>`. A parse
failure short-circuits upward through the `.has_value()` check and early
return. No exceptions — errors are values that propagate through the call
chain. This makes the entire elaborator usable at compile time inside a
`static_assert`.

# What Comes Next

The core AST produced here is still arena-based: children are integer
handles that require the arena for dereferencing. The next step converts
this into a self-contained `Fix<CompF>` tree — an IR that carries its own
data and does not need an arena for traversal. That representation is what
the Mendler-style interpreter walks.

# References

Abelson, Harold and Sussman, Gerald Jay (1996). *Structure and Interpretation
of Computer Programs*, 2nd ed., MIT Press.
