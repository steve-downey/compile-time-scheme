**DRAFT &#x2014; pending author revision**

<div class="abstract" id="orgb9b4875">
<p>
The Scheme reader from Phase 3 asked one question of every character run: is this a valid datum?
The Common Lisp reader in <code>smd/smdlisp</code> asks the same question, but the answer now depends on a readtable's worth of decisions the Scheme side never had to make.
Symbols fold to uppercase at read time.
Keywords are their own datum kind, not a symbol with a colon glued on.
<code>;</code> starts a comment that has to work anywhere intertoken space works, including mid-form.
And a token that starts with a digit is not automatically a number &#x2014; <code>1+</code> is a perfectly good Common Lisp symbol, and getting that wrong is how I found DIV-0003.
This post covers Steps L4 through L6: the character predicates, the atom reader, and the datum tree that adds keywords and sharpsign-quote to the shapes a reader can produce.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 15 - Why Not call/cc: From Scheme to Common Lisp ←](phase-15-why-common-lisp.md)

</nav>


# Same reader shape, different readtable

Phase 3 built a reader that turned source text into a tree of datums without asking what any of it meant &#x2014; homoiconicity's whole point (McCarthy, John, 1960). That architecture survives the pivot unchanged: an arena-backed, fixpoint-recursive tree, atoms and lists and quote forms allocated as handles into a shared arena instead of pointers, exactly the shape `smd::smdscheme::reader::datum_type` already has. What changes is everything upstream of the tree shape &#x2014; the rules for what counts as a token and how a token becomes a datum. Common Lisp's reader algorithm is the standard's own multi-page state machine (Steele, Guy L., 1990); `smdlisp` implements a deliberately small subset of it, but even the subset has more judgment calls than Scheme's reader ever needed.


# Case folding at read time

Common Lisp's standard readtable reads unescaped symbols and folds them to uppercase before anything else sees them. Type `foo`, get the symbol `FOO`. There is no `|escaped|`-symbol support in this reader and no readtable-case configuration &#x2014; decision D2 keeps this permanent and one-directional, recorded in DIV-0001 alongside the single-package decision. The fold itself is one function, applied character by character:

```cpp
/// Case-folds a single character to uppercase, ASCII only.
///
/// Per docs/cl-pivot-plan.md decision D2, @c smdlisp folds unescaped symbol
/// text to uppercase at read time, matching the ANSI default readtable; this
/// is the single-character primitive that step L5's symbol reader folds
/// with.
constexpr auto to_upper_char(char c) -> char {
    if (c >= 'a' && c <= 'z') {
        return static_cast<char>(c - 'a' + 'A');
    }
    return c;
}
```

Folding has to happen before the token becomes a datum, and the folded spelling has to live somewhere that is not the source buffer, because the whole point of folding is that the stored characters may differ from what the source said. That is why the atom reader's `folded_name` carries its own fixed-size storage instead of a `string_view` &#x2014; the same load-bearing distinction Phase 3's `datum_symbol` never had to make, because the Scheme reader never rewrites a character.


# Keywords are a kind, not a convention

`:foo` reads as a keyword, and per decision D7 a keyword is its own datum kind &#x2014; `datum_keyword`, sitting next to `datum_symbol` in the variant, not a symbol whose name happens to start with a colon. That distinction is checkable at compile time rather than by inspecting a spelling, which is exactly the property a Lisp-2 elaborator will lean on later when it decides that keywords self-evaluate and symbols do not. The reader-level version of that decision shows up directly in the datum tree's shape:

```cpp
/// A Common Lisp symbol datum, folded to uppercase at read time per decision
/// D2 (e.g. source @c foo becomes the datum @c FOO).
///
/// Storage is the same @ref folded_name the atom reader already produces
/// (own fixed storage, not a view into source) — the datum layer does not
/// re-derive or re-fold anything.
struct datum_symbol {
    folded_name name{}; ///< The folded symbol spelling.
};

/// A Common Lisp keyword datum (@c :foo), folded to uppercase per decision
/// D2. Distinct from @ref datum_symbol per decision D7: keyword-ness is a
/// separate datum kind, not a naming convention layered on top of a symbol.
struct datum_keyword {
    folded_name name{}; ///< The folded keyword spelling, leading colon
                        ///< stripped.
};
```

Both alternatives hold the same `folded_name` the atom reader already built; the datum layer does not re-derive or re-fold anything, it just wraps what `atom_p` already classified.


# Comments have to work inside forms

Scheme's reader subset in this project never had comments at all. Common Lisp's `;` runs to end of line, and it has to be skippable **anywhere** intertoken space is skippable &#x2014; between a list's open paren and its first element, between arguments, between a symbol and the close paren that ends it. Bolting comment-skipping onto whitespace-skipping as an afterthought would mean re-deriving every call site that currently calls the whitespace skipper; instead, comments are folded directly into the one function that already owns intertoken space:

```cpp
/// Advances @p cur past a @c ; line comment, consuming through (but not
/// including) the next newline, or through end of input if none remains.
/// @pre !cur.empty() && cur.peek() == ';'
constexpr auto skip_line_comment(smdscheme::parser::cursor cur)
    -> smdscheme::parser::cursor {
    while (!cur.empty() && cur.peek() != '\n') {
        cur = cur.bump();
    }
    return cur;
}
```

The datum reader's list loop and its top-level entry point both call this instead of a bare whitespace skip, so `(1 ; a comment\n 2)` reads as the two-element list it looks like, comment and all, with no special-casing at either call site.

One naming wrinkle from Step L4 is worth repeating here because it will bite again: this function is `skip_cl_intertoken_space`, not `skip_intertoken_space`. `smd::smdscheme::parser::skip_intertoken_space` already exists and takes the same `cursor` type, so an identically-named `smdlisp` overload would be pulled into every unqualified call site by argument-dependent lookup on the cursor's namespace &#x2014; permanently ambiguous, not a one-time collision. Any future `smdlisp` function taking a `smdscheme::parser::cursor` by value needs a name that does not collide this way.


# The 1+ bug: why integers need whole-token classification

Here is the one that actually broke a build. The Scheme reader recognizes a number by greedily consuming a leading run of digit characters, and that is safe there because Scheme symbols are syntactically barred from starting with a digit &#x2014; a digit-led token is unambiguously a number. Common Lisp has no such rule. `1+` is a completely ordinary symbol, conventionally the name of an increment function, and it starts with a digit. Porting the Scheme integer parser unchanged reads `1+` as the integer `1` followed by a stray, unconsumed `+` &#x2014; wrong, and wrong in a way that only shows up once something tries to read a symbol that happens to look number-shaped at the front.

The fix, and the reason it matters beyond one function, is to stop trying to decide "number or symbol" one character at a time. Read the whole token first &#x2014; a maximal run of constituent characters, the reader's own maximal-munch move &#x2014; and classify the complete token afterward:

```cpp
/// Returns a parser for a Common Lisp integer literal.
///
/// Reads a whole token (@ref detail::atom_token_p) and accepts it only if
/// it is entirely an optional leading @c - sign followed by one or more
/// decimal digits and nothing else; otherwise the parser fails without
/// misreading a digit-led symbol's numeric prefix (see @ref
/// detail::atom_token_p for why the whole token must be checked).
[[nodiscard]] constexpr auto integer_p() {
    return smdscheme::parser::parser{
        [](smdscheme::parser::cursor cur)
            -> smdscheme::parser::parse_result<atom_integer> {
            auto tok = detail::atom_token_p()(cur);
            if (!tok.has_value()) {
                return smdscheme::parser::parse_result<atom_integer>{
                    tok.error()};
            }
            auto text = tok.value().value;
            bool negative = !text.empty() && text[0] == '-';
            std::size_t digits_start = negative ? 1 : 0;
            if (digits_start >= text.size()) {
                return smdscheme::foundation::parse_error{cur.position(),
                                                          "integer"};
            }
            int n = 0;
            for (std::size_t i = digits_start; i < text.size(); ++i) {
                if (text[i] < '0' || text[i] > '9') {
                    return smdscheme::foundation::parse_error{cur.position(),
                                                              "integer"};
                }
                n = n * 10 + (text[i] - '0');
            }
            return smdscheme::parser::parse_result<atom_integer>{
                smdscheme::parser::parse_state<atom_integer>{
                    atom_integer{negative ? -n : n}, tok.value().rest}};
        }};
}
```

An integer, now, is a token that is **entirely** an optional leading `-` followed by one or more digits and nothing else. `1+` fails that test as a whole and falls through to the symbol reader, which accepts any constituent-character token unconditionally. A lone `-` fails the "one or more digits" half and reads as the symbol `-` &#x2014; correct ANSI behavior, since `-` is a symbol name in its own right, not a truncated number. This is DIV-0003, and it is marked `accepted-permanent`: whole-token classification is simply the correct behavior for a CL-flavored reader, discovered by a failing `static_assert` on `read_atom("1+")` during `make test`, not by reading the standard closely enough in advance. It is also the reason Step L6's datum reader does not reimplement any of this scanning itself; it calls `atom_p()` and trusts the classification that already happened.


# #' as a reader-level node, not a list in disguise

The last new shape in Step L6 is sharpsign-quote. `#'f` means "the function named `f`," and the textbook desugaring sends it to `(function f)` &#x2014; a three-element list with the symbol `FUNCTION` in head position, same as `'x` desugars to `(quote x)`. This reader does not do that desugaring, for the same reason it does not desugar `'x` to a `quote` list either: the source text `#'f` never contained a list, and the reader's job stops at recording what the source actually said. `'x` already gets a dedicated `datum_quote` node instead of a synthesized `(quote x)` list; `#'x` gets the same treatment, one level over:

```cpp
/// A Common Lisp sharpsign-quote form, e.g. @c #'f, stored as a handle to
/// the referenced datum.
///
/// Per docs/cl-pivot-plan.md step L6, @c #'x lowers to this dedicated datum
/// kind rather than to a @c (function x) list: the reader preserves source
/// reality (there is no list token in @c #'f), mirroring how @c 'x is
/// represented as a @ref datum_quote node today rather than expanded to
/// @c (quote x) at read time. Downstream passes (the elaborator) decide
/// what @c datum_function means; the reader only records that the source
/// spelled a sharpsign-quote.
///
/// @tparam R        The recursive element type.
/// @tparam MaxNodes Arena capacity.
template <typename R, int MaxNodes>
struct datum_function {
    smdscheme::foundation::arena_box<R, MaxNodes>
        target{}; ///< Handle to the referenced datum.
};
```

Parsing it is the same shape as parsing `'x`, just gated on the second character after `#`:

```cpp
if (c == '\'') {
        smdscheme::parser::cursor after = cur.bump();
        auto inner = read_datum_node<MaxNodes, MaxList>(after, arena);
        if (!inner.has_value())
            return inner;
        datum d{datum_f{
            datum_quote<datum, MaxNodes>{smdscheme::foundation::make_arena_box(
                arena, inner.value().value)}}};
        return smdscheme::parser::parse_state<datum>{d, inner.value().rest};
}

if (c == '#') {
        smdscheme::parser::cursor after = cur.bump();
        if (after.empty() || after.peek() != '\'') {
            return smdscheme::foundation::parse_error{cur.position(),
                                                      "expected #'"};
        }
        smdscheme::parser::cursor after_quote = after.bump();
        auto inner = read_datum_node<MaxNodes, MaxList>(after_quote, arena);
        if (!inner.has_value())
            return inner;
        datum d{datum_f{datum_function<datum, MaxNodes>{
            smdscheme::foundation::make_arena_box(arena,
                                                  inner.value().value)}}};
        return smdscheme::parser::parse_state<datum>{d, inner.value().rest};
}
```

Whether `#'x` **means** the same thing as `(function x)` downstream is an elaborator question for a later step, not a reader question. Keeping the two questions separate is the entire homoiconicity argument from Phase 3, applied one grammar rule further: the reader's job is to preserve source reality, and deciding what a shape means belongs to whatever reads the tree next.


# The merge test: one defun, four token kinds

The step's acceptance test is a single `defun` that exercises every reader decision in this post at once &#x2014; case folding, list nesting, and the symbol/integer boundary DIV-0003 exists to get right:

```lisp
(defun f (x) (if x 1 2))
```

reads as a four-element list &#x2014; `DEFUN`, `F`, the one-element parameter list `(X)`, and the body `(IF X 1 2)` &#x2014; every symbol folded, every integer an integer, no list mistaken for an atom or an atom mistaken for a list. Two failure shapes round out the step: an unterminated list (`(defun f (x` with no closing parens) has to fail cleanly rather than read a partial tree, and a stray `)` at the top has to fail immediately rather than being silently accepted as some other kind of atom. Decision D6 keeps this reader's lists proper: there is no dotted-pair syntax yet, so `(a . b)` is not part of the grammar this step implements, and that stays a divergence to revisit only when a later step actually needs it.

Case folding, keyword-as-a-kind, comments, and `#'` are the last purely lexical differences between the two readers. Everything from here forward &#x2014; `nil` and `t`'s meaning, the Lisp-2 split between variable and function namespaces, `setq` and `defun` as more than list shapes &#x2014; is the elaborator's problem, and the elaborator is next.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 15 - Why Not call/cc: From Scheme to Common Lisp](phase-15-why-common-lisp.md)

</nav>


# References

McCarthy, John (1960). *Recursive Functions of Symbolic Expressions and Their Computation by Machine, Part I*, Communications of the ACM.

Steele, Guy L. (1990). *Common Lisp the Language*, Digital Press.
