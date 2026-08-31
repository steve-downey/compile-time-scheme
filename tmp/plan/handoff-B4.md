# Handoff to B4

## Exact spellings, as B3 left them

`src/smd/kit/parser/parser.hpp`: `pure(T)`, `map(P, F)`, `satisfy(Pred pred, char const *expected)` (consumes one char on match, reports `expected` at the *current* position on failure/empty), `char_p(char)` (built on `satisfy`). All take the threaded-context second argument via `parse_context auto &`; none constrain to a domain-specific concept.

`src/smd/kit/parser/repeat.hpp`: `skip_many(P p)` — repeats `p`, discarding values, and **succeeds** at the cursor where `p` stopped. It stops on either of two conditions, not one: `p` failing, **or** `p` succeeding without advancing the cursor (`r.value().rest == cur`). The second arm is not a precondition note — it is checked code. Full rationale at the anchor below; do not re-narrate it, but do read it, because your bounded collecting repetition faces the identical question.

`docs/compiler_architecture.org` § "B3: the skipping layer is combinators, not two raw loops" (anchors `013cfe3b-6dfa-4f4b-b715-a59917e7715b` in `repeat.hpp`, `2ce9b1bf-8125-40fa-8496-fb96580aab21` in `skip.hpp`) has the full story, including why the guard exists: testing `skip_many(skip_many(p))` (a law your own step's law tests will want too, in some form) hit a `constexpr` evaluation-limit error, not a wrong answer, because `skip_many(p)` is itself exactly the zero-consumption-succeeding shape the precondition warned about. A documented-only precondition would not have caught this; the guard is what makes the law actually hold.

## What this means for your bounded collecting repetition

Your two repetitions are not symmetric on this axis. `skip_many` treats a zero-consumption success as a stop condition because it must never fail. Your collecting repetition fails on running out of input (`"expected ')'"`) and on capacity (`"too many elements"`), so a zero-consumption element read is a different hazard for you: it would not spin the way it would for `skip_many` (your loop's other exits — the closing delimiter, or running out of input — still fire), but decide explicitly whether your element step can ever succeed without advancing the cursor, and say in your own handoff what you found, since B5's string accumulator will ask the same question about its own "too long" shape.

## The two skippers' signatures did not change

`skip_block_comment(cursor) -> cursor` and `skip_intertoken_space(cursor, readtable const&) -> cursor`, exactly as before. Every caller (`read_delimited` included) is unaffected by B3.

## `readtable const` as a context: nothing needed adding to any concept

`readtable const` satisfies `kit::parser::parse_context` as-is (it is simply not a `cursor`) and does **not** satisfy `reader_context` (it lacks `.tree`/`.symbols`/`.child_list`) — that's expected, not a gap. Neither concept needed anything added. `read_delimited` takes a `reader_context` today; nothing about B3 changes that.

## Out-of-scope touch: `src/smd/cl/reader/read.test.cpp`

Outside B3's declared file scope but explicitly pre-authorized by the orchestrator mid-step: two new `fails_with` cases pinning the unterminated-`#|` behaviour (`fails_with("#| unterminated", "unexpected end of input")` and a nested variant), appended to the existing `reports_errors()` static-assert chain. No existing case's text changed. If you also touch reader error tests, this is the current shape of that function to build on.

## `bind` reach, unchanged

`src/smd/cl/reader/detail/read_context.hpp`'s `using smd::kit::foundation::bind;` is still the way to reach the Monad instance's `bind` unqualified; B3 also reached it directly as `smd::kit::foundation::bind(...)` inside `skip.hpp`'s own `skip_detail` namespace (a `using` there would have been fine too — no new gotcha, just noting both spellings work).
