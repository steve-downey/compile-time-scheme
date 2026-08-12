**DRAFT &mdash; pending author revision**

<div class="abstract" id="org6d6ae50">
<p>
R6 builds the corpus decision D16 promised: a place where a program's spelling, its expected outcome, and the provenance of that expectation live together.
Seventy-five cases, split between thirty-five adapted from Paul Dietz's <code>ansi-test</code> (MIT, pinned commit) and forty hand-derived from the ANSI text, because <code>ansi-test</code> leans on strings, characters and the numeric tower, and none of the three are in scope yet.
Every case states its outcome by channel (value, diagnosed error, or unwind), the same three R5 spent a whole phase splitting apart.
Only one entry pins anything, and D16 says that's the most any entry is allowed to pin.
Building the seventy-five found six new divergences, and the SBCL oracle closes a question this series has been carrying since phase 28: whether the evaluator agrees with anything other than itself.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 29 - Answering Phase 7, and One Invariant That Was Two ←](phase-29-answering-phase-7.md)

</nav>


# Four calls, written down once

`machine.test.cpp` already had the chain install-read-elaborate-run written out, twice: once for a single form, once for two forms sharing an arena, both times through `foundation::and_then` by hand. A corpus is not one form or two, it's whatever a case's author wrote, so the chain had to stop being copied and become a component.

```cpp
/// Reads every top-level datum in @p source, interning into @p symbols and
/// elaborating each into @p program — which may already hold earlier forms
/// — and returns each form's root index, in source order.
///
/// This is the sequence unfold R5's step brief left available but unwritten:
/// @ref reader::read_datum already returns the cursor positioned after the
/// datum it read, so reading "the rest" is repeating the call against that
/// cursor until only intertoken space remains. A program is one core arena
/// and not several, per @ref elaborator::elaborate_into's own contract, so
/// every root this returns shares @p program with every other.
///
/// @tparam DatumNodes  Datum-tree node capacity, shared by every top-level
///                     form read (a fresh tree per form, not accumulated).
/// @tparam DatumList   Datum-tree per-list capacity.
/// @tparam SymbolTable The symbol table instantiation.
/// @tparam CoreTree    The @ref core::core_tree instantiation being filled.
/// @return The root index of each top-level form, left to right, or the
///         leftmost failure — including "no forms to read" for a source
///         that is empty once whitespace and comments are removed.
template <int DatumNodes, int DatumList, class SymbolTable, class CoreTree>
[[nodiscard]] constexpr auto read_and_elaborate_all(std::string_view source,
                                                    SymbolTable &symbols,
                                                    CoreTree &program)
    -> foundation::result<typename CoreTree::child_list> {
    typename CoreTree::child_list roots;
    reader::cursor cur = detail::skip_between_forms(reader::cursor{source},
                                                    reader::standard_readtable);
    if (cur.empty()) {
        return foundation::parse_error{{}, "no forms to read"};
    }
    while (!cur.empty()) {
        auto const read_r =
            reader::read_datum<DatumNodes, DatumList>(cur, symbols);
        if (!read_r.has_value()) {
            return read_r.error();
        }
        auto const root_r =
            elaborator::elaborate_into(read_r.value().value, symbols, program);
        if (!root_r.has_value()) {
            return root_r.error();
        }
        if (roots.size() >= roots.capacity()) {
            return foundation::parse_error{{}, "too many top-level forms"};
        }
        roots.push_back(root_r.value());
        cur = detail::skip_between_forms(read_r.value().rest,
                                         reader::standard_readtable);
    }
    return roots;
}
```

`read_and_elaborate_all` is the sequence R5's brief left as an unfold available but unwritten: `reader::read_datum` already returns the cursor positioned after the datum it read, so reading the rest is calling it again against that cursor until nothing but whitespace and comments is left. Every root it returns shares one `program` tree, because a closure remembers the index of the `core_lambda` node its body lives at, and a function defined by one form and called by a later one needs both forms' nodes to still be there.

```cpp
/// Runs @p source — one or more top-level forms — start to finish: installs
/// the builtins, reads and elaborates every form into one arena, wraps more
/// than one in a `progn`, and evaluates the result.
///
/// This formalizes the chain `install_builtins` -> `read` ->
/// `elaborate` -> `machine::run` that R5 left open-coded in
/// `eval/machine.test.cpp`'s local `evaluate`/`evaluate_both` helpers,
/// generalized from "one form" and "exactly two forms" to any number, which
/// is what a conformance corpus needs — its source spellings are not bounded
/// to one or two top-level forms the way a hand-written unit test's are.
///
/// A read or elaborate failure and a run-time failure both funnel into the
/// same @ref eval::outcome, exactly as `machine.test.cpp`'s helpers already
/// did: a corpus case that expects an error should not have to know which
/// stage produced it, only that the program never reached a value.
///
/// @tparam Limits         Machine storage and step budget.
/// @tparam DatumNodes     Datum-tree node capacity.
/// @tparam DatumList      Datum-tree per-list capacity.
/// @tparam CoreNodes      Core-tree node capacity.
/// @tparam CoreChildren   Core-tree per-node subexpression capacity.
/// @tparam MaxSymbols     Symbol-table entry capacity.
/// @tparam MaxNameChars   Symbol-table name-character capacity.
/// @param  source         One or more top-level forms.
/// @param  witness_name   A symbol name to check for a function definition
///                        after the run, or empty to skip the check.
template <
    eval::limits Limits = default_limits, int DatumNodes = default_datum_nodes,
    int DatumList = default_datum_list, int CoreNodes = default_core_nodes,
    int CoreChildren = default_core_children,
    int MaxSymbols = default_max_symbols,
    int MaxNameChars = default_max_name_chars>
[[nodiscard]] constexpr auto
evaluate_program(std::string_view source, std::string_view witness_name = {})
    -> program_report {
    using table_type = eval::symbol_table<MaxSymbols, MaxNameChars>;
    using program_type = core::core_tree<CoreNodes, CoreChildren>;

    table_type symbols;
    program_type program;
    eval::standard_symbols known{};
    auto const ran = foundation::and_then(
        eval::install_builtins(symbols),
        [&](eval::standard_symbols k) -> foundation::result<eval::outcome> {
            known = k;
            return foundation::and_then(
                read_and_elaborate_all<DatumNodes, DatumList>(source, symbols,
                                                              program),
                [&](typename program_type::child_list const &roots)
                    -> foundation::result<eval::outcome> {
                    return foundation::and_then(
                        detail::combine_roots(program, roots),
                        [&](int root) -> foundation::result<eval::outcome> {
                            program.set_root(root);
                            eval::machine<Limits, program_type, table_type>
                                running{program, symbols, k};
                            return running.run();
                        });
                });
        });
    bool witness_defined = false;
    if (!witness_name.empty()) {
        if (auto const found = symbols.find(witness_name)) {
            witness_defined = symbols.function(*found).has_value();
        }
    }
    return program_report{ran.has_value() ? ran.value()
                                          : eval::outcome{ran.error()},
                          known, witness_defined};
}
```

`evaluate_program` wraps more than one root in a fresh `core_progn` before handing the whole thing to the machine, and a read failure, an elaborate failure and a run-time failure all funnel into the one `eval::outcome` the two test helpers already agreed on. A corpus case that expects an error should not have to know which stage produced it. The one piece that isn't a straight formalization is `program_report::witness_defined`: a case can name a symbol and ask, after the run, whether it acquired a function definition, which is the same probe `machine.test.cpp` used by hand to watch `unwind-protect`'s cleanup actually run, now written once instead of reached for.


# Two provenances, and the smaller one isn't the one I expected

D16 wants a corpus where a program, its expected outcome, and where that expectation came from all live in one place. `ansi_test_adapted` takes that literally: every entry names, in an `origin` field, the exact `ansi-test` file and `deftest` it came from, and no `deftest` body was rewritten to fit. A body using `let`, `flet`, `values`, `catch` or `tagbody` was simply dropped.

```cpp
/// Cases adapted from `pfdietz/ansi-test` (MIT; see
/// `THIRD-PARTY-LICENSES.md` for the copyright notice and the pinned
/// commit). Every source form here is the original `deftest`'s form,
/// unrestructured — a `deftest` body using `let`, `values`, `flet`, or any
/// other operator past this project's surface was discarded rather than
/// rewritten, per the fetch-and-adapt diligence this step was given.
///
/// What *is* adapted is how the expected result is checked: this harness
/// cannot inspect a returned symbol's name (the corpus driver deliberately
/// does not expose the symbol table, so a `program_report` outlives it), so
/// a `deftest` whose expected value is a bare symbol is wrapped in `eq` or
/// `null` against a quoted literal from the *same* source string — the same
/// symbol, interned once, in the same run. That wrapping changes nothing
/// about what CL form is under test.
inline constexpr std::array<corpus_case, 35> ansi_test_adapted{{
    // cons/cons.lsp
    {"cons.error.1", "(cons)", expect_error{},
     provenance_kind::ansi_test_adapted, "cons/cons.lsp deftest cons.error.1"},
    {"cons.error.2", "(cons 'a)", expect_error{},
     provenance_kind::ansi_test_adapted, "cons/cons.lsp deftest cons.error.2"},
    {"cons.error.3", "(cons 'a 'b 'c)", expect_error{},
     provenance_kind::ansi_test_adapted, "cons/cons.lsp deftest cons.error.3"},
    {"cons-of-symbols.car", "(eq (car (cons 'a 'b)) 'a)", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "cons/cons.lsp deftest cons-of-symbols (car half)"},
    {"cons-of-symbols.cdr", "(eq (cdr (cons 'a 'b)) 'b)", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "cons/cons.lsp deftest cons-of-symbols (cdr half)"},
    {"cons-with-nil.car", "(eq (car (cons 'a nil)) 'a)", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "cons/cons.lsp deftest cons-with-nil (car half)"},
    {"cons-with-nil.cdr", "(null (cdr (cons 'a nil)))", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "cons/cons.lsp deftest cons-with-nil (cdr half)"},
    // cons/cxr.lsp — car and cdr only; every compound accessor (caar,
    // cadr, ...) is out of the operator surface and was discarded.
    {"cxr.cons.23", "(eq (car (quote (a))) 'a)", expect_boolean{true},
     provenance_kind::ansi_test_adapted, "cons/cxr.lsp deftest cons.23"},
    // cons.24, (cdr '(a . b)) => b, is NOT adapted: it needs reader syntax
    // for the dotted-pair consing dot, which the reader does not implement
    // (D6, `docs/cl-pivot-plan.md`: "deferred until a step needs it; file a
    // divergence doc when deferring bites" — DIV-0022 is that doc).
    {"cxr.cdr.1.car", "(eq (car (cdr '(a b))) 'b)", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "cons/cxr.lsp deftest cdr.1 (car-of-cdr half)"},
    {"cxr.cdr.1.cdr", "(null (cdr (cdr '(a b))))", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "cons/cxr.lsp deftest cdr.1 (cdr-of-cdr half)"},
    // data-and-control-flow/if.lsp
    {"if.1", "(if t 1 2)", expect_fixnum{1}, provenance_kind::ansi_test_adapted,
     "data-and-control-flow/if.lsp deftest if.1"},
    {"if.2", "(if nil 1 2)", expect_fixnum{2},
     provenance_kind::ansi_test_adapted,
     "data-and-control-flow/if.lsp deftest if.2"},
    {"if.4", "(null (if nil 'a))", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "data-and-control-flow/if.lsp deftest if.4"},
    // data-and-control-flow/progn.lsp
    {"progn.1", "(null (progn))", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "data-and-control-flow/progn.lsp deftest progn.1"},
    {"progn.2", "(eq (progn 'a) 'a)", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "data-and-control-flow/progn.lsp deftest progn.2"},
    {"progn.3", "(eq (progn 'b 'a) 'a)", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "data-and-control-flow/progn.lsp deftest progn.3"},
    // data-and-control-flow/block.lsp
    {"block.1", "(block foo (return-from foo 1))", expect_fixnum{1},
     provenance_kind::ansi_test_adapted,
     "data-and-control-flow/block.lsp deftest block.1"},
    {"block.4",
     "(eq (block foo (block foo (return-from foo 'bad)) 'good) 'good)",
     expect_boolean{true}, provenance_kind::ansi_test_adapted,
     "data-and-control-flow/block.lsp deftest block.4"},
    {"block.8", "(null (block foo))", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "data-and-control-flow/block.lsp deftest block.8"},
    // data-and-control-flow/return-from.lsp — the original's `:bad` is
    // spelled `'bad` here (a quoted symbol rather than a keyword); both are
    // self-evaluating data never reached once return-from fires, so the
    // substitution changes nothing about what is under test.
    {"return-from.1", "(null (block xyz (return-from xyz) 'bad))",
     expect_boolean{true}, provenance_kind::ansi_test_adapted,
     "data-and-control-flow/return-from.lsp deftest return-from.1"},
    // data-and-control-flow/nil.lsp
    {"nil.4", "(null (car nil))", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "data-and-control-flow/nil.lsp deftest nil.4"},
    {"nil.5", "(null (cdr nil))", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "data-and-control-flow/nil.lsp deftest nil.5"},
    // eqt/nil.8 pins DIV-0002 (eq and eql coincide), a `scope-decision`,
    // which D16 says a test may pin.
    {"nil.8", "(eq nil 'nil)", expect_boolean{true},
     provenance_kind::ansi_test_adapted,
     "data-and-control-flow/nil.lsp deftest nil.8"},
    // data-and-control-flow/defun.lsp — defun.1's shape (a function whose
    // body is only return-from, called after definition), renamed to avoid
    // colliding with any other corpus entry's function name.
    {"defun.1",
     "(defun conformance-defun-1 () (return-from conformance-defun-1 "
     "'good)) (eq (conformance-defun-1) 'good)",
     expect_boolean{true}, provenance_kind::ansi_test_adapted,
     "data-and-control-flow/defun.lsp deftest defun.1 (renamed)"},
    // numbers/plus.lsp, numbers/minus.lsp, numbers/times.lsp
    {"plus.1", "(+)", expect_fixnum{0}, provenance_kind::ansi_test_adapted,
     "numbers/plus.lsp deftest plus.1"},
    {"minus.error.1", "(-)", expect_error{}, provenance_kind::ansi_test_adapted,
     "numbers/minus.lsp deftest minus.error.1"},
    {"times.1", "(*)", expect_fixnum{1}, provenance_kind::ansi_test_adapted,
     "numbers/times.lsp deftest *.1"},
    // numbers/number-comparison.lsp — only the arity errors are usable
    // without `let`; every literal comparison test binds its operands with
    // `let` first, which is out of the operator surface and was discarded
    // rather than inlined (the equivalent facts are hand-derived below,
    // marked spec-derived rather than claimed as ansi-test's own forms).
    {"=.error.1", "(=)", expect_error{}, provenance_kind::ansi_test_adapted,
     "numbers/number-comparison.lsp deftest =.error.1"},
    {"<.error.1", "(<)", expect_error{}, provenance_kind::ansi_test_adapted,
     "numbers/number-comparison.lsp deftest <.error.1"},
    {">.error.1", "(>)", expect_error{}, provenance_kind::ansi_test_adapted,
     "numbers/number-comparison.lsp deftest >.error.1"},
    // sequences/length.lsp
    {"length.list.1", "(length nil)", expect_fixnum{0},
     provenance_kind::ansi_test_adapted,
     "sequences/length.lsp deftest length.list.1"},
    {"length.list.2", "(length '(a b c d e))", expect_fixnum{5},
     provenance_kind::ansi_test_adapted,
     "sequences/length.lsp deftest length.list.2"},
    {"length.error.6", "(length)", expect_error{},
     provenance_kind::ansi_test_adapted,
     "sequences/length.lsp deftest length.error.6"},
    {"length.error.7", "(length nil nil)", expect_error{},
     provenance_kind::ansi_test_adapted,
     "sequences/length.lsp deftest length.error.7"},
    // length.error.8's outer `(locally (length 'a) t)` is stripped to
    // `(length 'a)`: `locally` carries no declarations here, so it is inert
    // wrapper syntax around the one form actually under test, and `locally`
    // itself is not in this project's operator surface.
    {"length.error.8", "(length 'a)", expect_error{},
     provenance_kind::ansi_test_adapted,
     "sequences/length.lsp deftest length.error.8 (locally wrapper "
     "stripped)"},
}};
```

Thirty-five cases made it through that filter, out of a tree of `ansi-test` files that is not small. D10 put strings, characters and the numeric tower out of scope at the pivot, before any of this rebuild existed, and most of `ansi-test` turns out to be about those same three things. `cons.24` (`(cdr '(a . b)) => b`) didn't make it either, for a reason that isn't D10 at all: the reader has no syntax for a dotted-pair consing dot, so `(a . b)` reads as the three-symbol list `(a |.| b)` instead of a pair, and adapting the case would have silently tested the wrong thing. That's DIV-0022, and it's below.

So the adapted set is thirty-five and the hand-derived set is forty, and I went in assuming the ratio would run the other way. It doesn't. The honest reading is that this project's own operator surface is narrower than `ansi-test`'s assumptions; `ansi-test` itself is not thin. `quote` has no dedicated `ansi-test` file at all. Multi-argument arithmetic and all three branches of `unwind-protect` are spec-derived because every adaptable case for them binds something with `let` first; `atom`, `consp` and `symbolp`, because `ansi-test`'s own forms for them don't fit this narrow a surface. The recursive-`defun` acceptance witness from phase 28 is in there too, as `defun.recursive`, because DIV-0009 closing was itself a claim I wanted pinned as a permanent regression: a `static_assert` can quietly stop being run, and a corpus entry can not.

One more adaptation, worth stating plainly because it's the kind of thing that could be mistaken for cheating: this driver doesn't expose the symbol table, so a `program_report` can't compare a returned symbol against its name. A `deftest` whose expected value is a bare symbol gets wrapped in `eq` or `null` against a quoted literal drawn from the same source string: the same symbol, interned once, in the same run. That changes how the check is written in C++. The CL form under test is untouched, and every such rewrite is recorded next to the entry it applies to.


# The one entry allowed to pin anything

D16's rule is short and has no hedge in it: a test may pin a `scope-decision`, a test must never pin a `defect`. Out of seventy-five entries, exactly one pins anything at all.

`nil.8` asserts `(eq nil 'nil)`, which is DIV-0002: `eq` and `eql` still coincide in this tree, for a different reason than the pivot's. There it was string comparison on a name; here identity is real, and every number that exists is an immediate, so nothing yet separates the two operations. DIV-0002 is classified `scope-decision` in `docs/divergences/README.md`, so the corpus is allowed to assert the coincidence as correct. Nothing else in the corpus comes close: no entry spells `(block nil ...)`, none reaches for a lambda-list keyword, none calls anything through `funcall`, because those are the gaps that would turn a corpus case into an anti-specification. Assert the bug, and fixing it costs a test deletion instead of buying one.

Writing seventy-five cases against those boundaries is what turned up six divergences nobody had filed yet. DIV-0022 is the dotted-pair gap above, deferred since the pivot's own D6 and inherited unreopened by this rebuild's reader step; it only bit here, when a corpus needed the syntax and didn't have it. DIV-0023 is a genuine defect. ANSI has no restriction on `nil` as a block name, and `(block nil ...)` is what `loop`'s implicit block needs, but the atom pass here turns every `NIL` token into a constant before the role pass runs, so `(block nil ...)` is diagnosed the same as `(return-from nil ...)`. DIV-0024 and DIV-0025 are both D19's deliberate scope, for two different reasons that rhyme: one won't let a lambda-list keyword misread as an ordinary parameter, the other won't add a value channel with no producer to fill it. Lambda-list keywords are diagnosed; none is silently bound to a parameter literally named `&optional`. `lambda` in expression position, `#'f`, `funcall` and `apply` are all still unreachable, because nothing yet produces a function value for them to consume. DIV-0026 writes up a GCC trunk defect R3 had already worked around in `reader/number.hpp` (a `memchr`-lowered `find` misreports its offset on a view advanced by `remove_prefix`), and DIV-0027 states, now that a corpus is here to feel it, that every elaborator and evaluator diagnostic still carries no source position at all.

None of the six are news to the evaluator; R4 and R5's own step briefs had already noticed most of them in passing. What the corpus did was force each one into a doc with a classification, because "may I pin this" turns out to be a question that only gets asked once something is trying to.


# Someone else's answer

Everything above runs against this project's own evaluator, which is a way of checking that the machine agrees with the specification I read and the tests I wrote. Phase 28 said that's the weakest evidence in the project, and it stayed that way through phase 29. `sbcl_oracle.hpp` is where it stops being the only evidence.

```cpp
/// Probes for an `sbcl` binary on `PATH` and returns its version string
/// (e.g. `SBCL 2.2.9.debian`), or `nullopt` if none is reachable.
///
/// A runtime probe rather than a build-time dependency, per the
/// orchestrator's decision: the differential test must degrade to a skip in
/// an environment without SBCL, not fail to configure. The returned string
/// is what a reported divergence should be attributed to, exactly as
/// `compile-time-forth`'s `gforth_diff` pins the gforth version it ran
/// against.
[[nodiscard]] inline auto find_sbcl_version() -> std::optional<std::string> {
    auto output = detail::run_shell_capture("sbcl --version 2>/dev/null");
    if (!output.has_value()) {
        return std::nullopt;
    }
    auto trimmed = detail::trim(std::move(*output));
    if (trimmed.empty()) {
        return std::nullopt;
    }
    return trimmed;
}
```

This is a runtime probe: the differential check has to degrade to a skip when `sbcl` isn't on `PATH`, and it must never simply fail to configure. An environment without SBCL stays green. A missing oracle is a fact about the environment, not a defect here.

```cpp
/// Evaluates @p form as a single SBCL top-level form and returns what
/// `prin1` prints for its value — the readable representation, which for a
/// fixnum or the symbols `T`/`NIL` matches this project's own printed form
/// exactly (both readers upcase). Returns `nullopt` if SBCL is unreachable;
/// a form SBCL itself signals an error on prints as the literal
/// `SBCL-ERROR`, a string no well-typed corpus form's `prin1` output can
/// collide with.
///
/// This is deliberately the informal half of the check named in the R5 step
/// brief: it compares one printed representation, not a full
/// condition-system-aware equivalence, which is enough to catch a semantic
/// disagreement on the scalar-valued forms the guarded differential test
/// runs it against, without building a general SBCL-condition-to-`outcome`
/// translation this step does not need.
[[nodiscard]] inline auto sbcl_prin1(std::string_view form)
    -> std::optional<std::string> {
    std::string const script = "(handler-case (prin1 " + std::string(form) +
                               ") (error () (princ \"SBCL-ERROR\")))";
    std::string const command =
        "sbcl --noinform --non-interactive --no-userinit --no-sysinit "
        "--eval " +
        detail::shell_quote(script) + " 2>/dev/null";
    auto output = detail::run_shell_capture(command);
    if (!output.has_value()) {
        return std::nullopt;
    }
    return detail::trim(std::move(*output));
}
```

The check is deliberately informal: one form in, one `prin1` line out, compared as text against what a fixnum or `T` / `NIL` expectation would print. It's not a general SBCL-condition-to-`outcome` translation, and the test that drives it only runs the comparison for the corpus's fixnum- and boolean-valued cases: the ones scalar enough for two printers to agree on without either side inventing a shared error vocabulary first. A form SBCL itself signals an error on prints as the literal string `SBCL-ERROR`, which is not a value any well-typed corpus entry can produce, so an error there reads as a plain mismatch rather than a crash.

I'd half-expected shelling out to be the awkward part, and it wasn't; wrapping a two-form corpus entry (a `defun` and then a call) into one `--eval` argument was. The differential test wraps the source in `(progn ...)` for the same reason `evaluate_program` wraps several roots in a `core_progn`: SBCL and this driver both need one form, and a sequence of top-level forms is one form once you say "and then." Two independent pieces of code reaching for the same wrapper is a small thing, but it's the kind of small agreement that makes me trust the comparison more than I would have from either alone.

`sbcl_oracle.hpp` is deliberately not `constexpr`. Every other file in `conformance/` has a compile-time twin (`corpus.test.cpp` runs the whole corpus through `cl/eval` inside two separate `static_assert` expressions, one per array), and this is the one exception, on purpose: a subprocess has no meaning under constant evaluation, so there's nothing to make a twin of. The corpus proves the pipeline agrees with itself at compile time; the oracle test is the only place in this whole area that has to run.


# What's still owed

The corpus checks value, error and unwind shape, never a diagnostic's text against SBCL's condition system, and DIV-0027 means it couldn't check position even if it wanted to. Forty of the seventy-five cases have no `ansi-test` counterpart to compare provenance against at all. That's a fact about the coverage: a hand-derived case cites the ANSI section it encodes, and that's still a citation, just a different kind. And the differential check only ever asked SBCL about the scalar fraction; nothing here compares list structure, and nothing asks it about a program that errors or unwinds, because there's no shared vocabulary yet for what SBCL's condition system and this project's `outcome` agree to mean by "the same failure."

That's the next gap, and it's someone else's problem for now. R7 is next.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 29 - Answering Phase 7, and One Invariant That Was Two](phase-29-answering-phase-7.md)

</nav>


# References
