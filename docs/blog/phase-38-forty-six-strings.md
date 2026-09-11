**DRAFT &mdash; pending author revision**

<div class="abstract" id="orga674985">
<p>
Step A5 broadens A4's eleven cases into forty-one source strings and a five-case error table, and closes Phase A.
Twenty of the twenty-three non-error inputs the retired <code>oracle_compare.test.cpp</code> tested come across, now checked against SBCL instead of against a sibling tree in this same repository.
The three that don't are the quasiquote family, which SBCL prints in an implementation-specific form ANSI permits.
Twenty more are syntax that comparison could never have reached, because D10 kept strings, characters, vectors and the numeric tower out of <code>smdlisp</code> entirely.
All forty-six agreed with SBCL 2.2.9.debian on the first run, one attempt, no new exclusions.
The differential SKIPs rather than fails when <code>sbcl</code> is not on <code>PATH</code>, so a green matrix is compatible with the comparison never having happened.
One spot check would have shown the difference, and it globs two directories down for a binary that is six down, where <code>2&gt;/dev/null</code> eats the error.
The capacity comment is written for pressure the corpus does not apply: the widest case in either table holds three elements.
DIV-0034 records that the file named <code>sbcl_differential.test.cpp</code> has never compared this project against SBCL, and is the second number Phase A issued out of six reserved.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 37 - A Printer, and Why It Could Not Land Alone ←](phase-37-a-printer-and-its-oracle.md)

</nav>


# Everything the retired file tested, checked against someone else

`src/smd/cl/reader/oracle_compare.test.cpp` had twenty-eight checks across six test cases when A2 deleted it. Twenty-three were inputs both readers had to read the same way. Five were inputs both readers had to reject.

A5's job is to put those inputs in front of an outside implementation instead of a sibling tree. Twenty of the twenty-three come across.

The three that don't are the quasiquote family, `` `x ``, `,x`, `,@x`. SBCL 2.2.9 prints them as `SB-INT:QUASIQUOTE` and `#S(SB-IMPL::COMMA ...)`, which ANSI permits and this printer does not imitate, so A4 had already excluded them. The step brief lists all five quote-family inputs under "cover, at minimum", and then, three paragraphs down, says to carry A4's exclusions forward and names the same three. A5 went with the exclusion, and labelled the group "the non-excluded quote family". The only reading that leaves anything to write.

```cpp
// Every input `reader/oracle_compare.test.cpp` tested, before A3 deleted it
// along with the rest of smdlisp, now checked against SBCL instead.
constexpr std::array<reader_case, 21> inherited_cases{{
    {"Fixnums: bare", "42"},
    {"Fixnums: negative", "-7"},
    {"Fixnums: zero", "0"},
    {"Fixnums: leading whitespace", "  42"},
    {"Fixnums: leading comment", "; comment\n42"},
    {"SymbolsFoldAlike: lowercase", "foo"},
    {"SymbolsFoldAlike: mixed case", "cAr"},
    {"SymbolsFoldAlike: t", "t"},
    {"SymbolsFoldAlike: nil", "nil"},
    {"SymbolsFoldAlike: plus", "+"},
    {"SymbolsFoldAlike: whole-token 1+", "1+"},
    {"SymbolsFoldAlike: whole-token 2buffer", "2buffer"},
    {"Keywords: foo", ":foo"},
    {"Keywords: mixed case", ":Bar"},
    {"Lists: empty", "()"},
    {"Lists: flat", "(1 2 3)"},
    {"Lists: extra intertoken space", "( 1  2 )"},
    {"Lists: comment between elements", "(1 ; two\n 2)"},
    {"Lists: nested", "(1 (2 3) 4)"},
    {"QuoteFamily: quote", "'x"},
    {"QuoteFamily: function", "#'car"},
}};

// New coverage: syntax the retired oracle_compare.test.cpp never exercised,
// because smdlisp never had it (D10 put strings, characters, vectors and
// the numeric tower out of that iteration's scope).
constexpr std::array<reader_case, 20> new_coverage_cases{{
    {"Strings: empty", R"("")"},
    {"Strings: plain", R"("hello")"},
    {"Strings: embedded quote escape", R"("a\"b")"},
    {"Strings: embedded backslash escape", R"("a\\b")"},
    {"Strings: embedded space", R"("multi word")"},
    {"Strings: semicolon is not a comment inside a string",
     R"("; not a comment")"},
    {"Characters: lowercase letter", R"(#\a)"},
    {"Characters: uppercase letter", R"(#\A)"},
    {"Characters: digit", R"(#\1)"},
    {"Characters: open paren", R"(#\()"},
    {"Characters: semicolon", R"(#\;)"},
    {"Vectors: flat", "#(1 2 3)"},
    {"Vectors: empty", "#()"},
    {"Vectors: nested list element", "#(1 (2 3) 3)"},
    {"Vectors: keyword elements", "#(:a :b)"},
    {"DecimalTower: float", "1.5"},
    {"DecimalTower: negative float", "-0.5"},
    {"DecimalTower: trailing-zero float", "3.0"},
    {"DecimalTower: ratio", "1/2"},
    {"DecimalTower: negative ratio", "-3/4"},
}};
```

Twenty-one entries in `inherited_cases`, not twenty. The extra is `(1 (2 3) 4)`. It came from A4's eleven, and was never in the retired file at all. That file's `Lists` case ran to four inputs: the empty list, `(1 2 3)`, one padded with extra intertoken space, and one with a comment between its elements. No nesting. So the comment above the table, which says every input that file tested, is one case short of the table it sits on. The wider version is the better table.

The same comment gets the step wrong as well. It has A3 deleting the file, and so does the header paragraph above it, while A5's own commit message has A2 deleting the file and A3 deleting `smdlisp` under it. The log agrees with the commit message. Phase 37 caught the same slip in A4's commit message a step ago, and it has now moved into the code.


# Twenty the old comparison could not have reached

`new_coverage_cases` is the other half, and it is the half that makes this more than a restoration. D10 kept strings, characters, vectors and the numeric tower out of `smdlisp` altogether, so the structural comparison A2 retired had nothing to compare there. The retired file's own header said so, and named where the authority would have to come from: "the pending SBCL differential check". Here is that check, three steps after the file that asked for it stopped existing.

Six strings. An empty one, one with an escaped quote, one with an escaped backslash, and one that opens on a semicolon inside the quotes, to prove the comment scanner stays out. Five character literals, `#\(` and `#\;` among them. Four vectors, one empty, one with a list inside it. And five numeric towers, all canonical decimal: `1.5`, `-0.5`, `3.0`, `1/2`, `-3/4`.

Not `#x1f`, `2/4` or `1.50`. D19 chose readable over executable, so this reader stores a token's spelling and the printer hands the spelling back, while SBCL's `prin1` prints the value the spelling denotes. They coincide on canonical decimal and nowhere else, which is a restriction phase 37 already wrote down and this corpus obeys.

Not `#\Space` either. SBCL 2.2.9 prints the space character as `#\` followed by a literal space instead of by name, and this printer has no character-name table to disagree with. That exclusion is pinned to a version number in a comment, and to nothing anywhere in the code. The other two bullets are pinned to less than that. The quasiquote one cites ANSI and no build; the numeric one cites nothing at all. However, none of the three is a corner cut. A differential wants agreement about the things both sides are trying to say the same way, and one build's character-name table is not among them.


# Five that have to fail

```cpp
// The retired oracle_compare.test.cpp's ErrorsAgree inputs: both readers
// must reject every one of these. sbcl_read_print reports a signalling
// read as the literal sentinel "SBCL-ERROR" (confirmed against SBCL
// 2.2.9.debian for all five), never as an absent optional -- nullopt there
// is reserved for the sbcl binary itself being unreachable.
constexpr std::array<std::string_view, 5> error_cases{"", ")", "(1 2", "'",
                                                      "("};
```

`sbcl_read_print` wraps its read and print in `handler-case` and prints the literal text `SBCL-ERROR`, so a signalling read comes back as an optional holding a string. `nullopt` means the binary was unreachable, which is a different fact about the world. A4 chose that split and confirmed all five of these inputs travel it. A5 followed it instead of re-deciding.

The assertion is weak on one side, on purpose. `CHECK_FALSE(parsed.has_value())` for this reader, `CHECK(*theirs == "SBCL-ERROR")` for SBCL, and no comparison of message text at all. DIV-0027 already records that this project's diagnostics carry no source position and are its own. Pinning their wording to one implementation's condition text would pin the wrong thing.

The corpus tables carry a label per case, so a failure names itself in the `INFO`. The error table doesn't. It's a bare array of five source strings, and the helper puts the source itself in the `INFO`. Four of the five have something to show. The first one is the empty string.


# All forty-six, first run

A5's row in `tmp/plan/metrics.jsonl` reads `"outcome":"green"` and `"attempts":1`. The note on it says all 41 corpus cases and all 5 error cases agreed with SBCL 2.2.9.debian on the first run, with no exclusions needed beyond A4's three traps and `#\Space`. `ctest` went from 310 entries to 311, the new one being `ErrorsAgreeWithSbcl`.

Forty-one cases in a single `TEST_CASE` is a lot to hang on `REQUIRE`. `check_against_sbcl` requires that this reader parsed, that the printer rendered, and that SBCL answered at all, before it gets as far as checking the two strings against each other. A failed `REQUIRE` throws, and the throw leaves the `for_each`, so the first input this reader can not read stops the other forty from running. The labels would tell you which one it was, and nothing about the rest.

Nothing failed, so none of that came up.


# What a green run proves

The differential shells out. `find_sbcl_version()` runs `sbcl --version`, and both test cases here open by calling it, then call `SKIP` when it comes back empty. That was decided when the first of these tests was written, and is inherited here unchanged. The file is a member of the default suite. A suite that goes red on a machine with no Common Lisp installed is reporting a fact about the machine, and not a defect in the project.

Catch2's `catch_discover_tests` sets `SKIP_RETURN_CODE 4` unless told `SKIP_IS_FAILURE`, and this project doesn't tell it that. So `ctest` records a skipped case as skipped, and the leg stays at 100% passing.

So `311/311, both legs` is compatible with two different worlds. In one of them, forty-six source strings each went through an SBCL subprocess and agreed. In the other, `sbcl` was not on `PATH` and two test cases printed a skip message. The only assertion that ran in the whole file was the `REQUIRE(true)` in `HeaderIsIdempotent`. The matrix output reads the same either way.

The oracle's identity is in the run too, and just as invisible. `INFO("oracle: " << *version)` carries the version string, and Catch2 prints an `INFO` only when something under it fails. On a passing run it prints nothing. The single place SBCL 2.2.9.debian is written where a later reader can find it is a prose note in a metrics row. Nothing the build reads.


# The spot check that could not run

The step file's spot checks end with this, and running it is the whole exercise:

```sh
./.build/*/*/cl_conformance_test "*ReaderDifferential*" 2>/dev/null | tail -5
```

The test binaries are not two directories down. A configured tree puts this one at `.build/build-gcc-16/src/smd/cl/conformance/Asan/cl_conformance_test`, which is six, with the `Debug` leg beside it. `.build/*/*/` reaches `.build/build-gcc-16/src/`, and there is no such file there.

Under `bash` the unmatched pattern passes through literally, the shell reports `No such file or directory` on stderr, and `2>/dev/null` discards it. Under `zsh` the glob fails outright and the command never runs. Either way `tail -5` reads an empty stdin, prints nothing, and exits 0, so the pipeline exits 0. A blank spot check looks exactly like a quiet one.

What it would have printed, with the path right and SBCL installed, is the one number nothing else in the record carries:

```
All tests passed (180 assertions in 3 test cases)
```

and on a machine without `sbcl`:

```
test cases: 3 | 1 passed | 2 skipped
assertions: 1 | 1 passed
```

180 against 1. The one command in the step that separates a differential from a skip is the command that couldn't find its binary, and `2>/dev/null` is what kept that quiet. Its two neighbours in the block do not fill the gap. One is a `grep -c "SKIP"` that prints `3` whatever those three are, and they are two calls plus one mention in a comment. The other is a `test ! -f` on a file A2 deleted three steps ago, which the step file itself labels as not this step's work.

None of which makes the green wrong. `make test-matrix` ran the binary properly, on a machine that had SBCL, and the metrics note reports what that run saw. The spot check was the cheap second look at the same thing from a different angle, and it is the one that came back empty.


# Sixty-four nodes for a six-node tree

A4's file declared `test_nodes = 32` and `test_list = 8`. A5 doubles both, with a comment. A broadened corpus's vectors and nested lists push closer to the per-list element cap than a handful of cases ever did, and tripping that cap is an `add_leaf~/~add_branch` precondition and not a diagnosed error. So the margin is deliberate.

Deliberate, and aimed at nothing. The widest list or vector in either table holds three elements. The largest trees any case builds are `(1 (2 3) 4)` and its vector twin `#(1 (2 3) 3)`, at six nodes each. A4 already had the first of those under 32 and 8. The handoff wrote its warning before the corpus existed. Widen first, it said, and do not wait for the assert. Good advice about a cap this corpus comes nowhere near.

`using tree = datum_tree<test_nodes, test_list>;` sits on the line below, still never used, exactly as it was a step ago. A5 edited both of its arguments and left it alone.


# A correction filed against a file this step was not changing

The neighbour, `conformance/sbcl_differential.test.cpp`, has a `check_against_sbcl` of its own, and it compares SBCL's output against `expected_sbcl_text(c)`, a string transcribed by hand into the corpus case. It never calls `evaluate_program`, or any other evaluation entry point of this project, and it skips every case whose channel is not `expect_fixnum` or `expect_boolean`. Whatever that file is, it is not a differential between `cl` and SBCL, and it never was.

What it is instead is a check that the corpus's own transcriptions, from `pfdietz/ansi-test` and from the specification, are what a real Common Lisp produces. Worth having, under an honest description.

DIV-0034 records why the name outran the code, and the reason is the previous step. The file predates `smd::cl::printer`. There was no way to render what this project produced into anything comparable, so the name got written for a check the repository did not yet have the pieces to write. No rename. It would churn CMake and every recorded `ctest` name for nothing. The file gains a paragraph in its own header instead, signed with the step and pointing at the record, which is the append-a-note convention already in use here. The consequence is written down plainly. The reader layer has an outside authority as of this step; the evaluator layer still does not.

DIV-0034 is also the first divergence number issued since DIV-0029, which A0 wrote. A1, A2, A3 and A4 each had one reserved up front, 0030 through 0033, so concurrent lanes could not collide on the next free number. Not one of the four needed it. `docs/divergences/README.md` already carried a sentence saying there is no DIV-0005. It now carries a second, saying there are no DIV-0030 through DIV-0033 either. Two sentences about numbers that never named anything.


# Phase A, backwards

The phase ran freeze, neutralise, delete, printer, differential, which is not the order it was first drawn in. A0 wrote the steps and decision D32. A1 tagged both dead front ends and moved their prose to a pinned history document. A2 cut every consumer. A3 took out 173 files and 31,155 lines. A4 built the printer, and A5 gave it a corpus.

`ctest` went 1118, 1105, 304, 310, 311. Phase A took out 814 test entries and put back seven.

A5 also writes the gate note. Phase A does not hand off to a next step; it hands off to the owner. What the note puts in front of him is D32, which retired `smdlisp` to a tag *before* the parity phase. The ratified D22 it sits beside had made that retirement the phase's merge criterion. And what the note says D32 deferred, and did not discard, is roughly 120 evaluated behaviour cases: `LET`, `SETQ`, `CATCH`, `TAGBODY`, `DEFMACRO`, multiple values. The 75-entry corpus reaches none of them. They stay readable out of `iteration/smdlisp-final` by anything that wants source programs.

The reader can now be told it is wrong by an implementation that has never seen this repository. Forty-six times over, on a machine that has SBCL installed. On a machine without, nothing happens and nothing says so.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 37 - A Printer, and Why It Could Not Land Alone](phase-37-a-printer-and-its-oracle.md)

</nav>


# References
