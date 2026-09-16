# BL-0012: a plan's spot-check commands ship without ever being run

- **Status:** open
- **Date:** 2026-09-15
- **Origin:** seven instances across one fourteen-step fan-out, each found by the agent executing a step rather than by the one who wrote it. Collected while drafting the phase 33–38 posts, which is the first time anyone read the step files end to end after the fact.
- **Frozen-tree impact:** none. This is about `tmp/plan/` and whatever writes the next one.

## What

Add a step zero to the plan template: before any worker is dispatched, execute
every literal path, glob and spot-check command in the plan against the tree
the plan is written for, and fix what does not run. It is mechanical, it is
cheap, and nothing in the current process does it.

This is `tmp/plan/INTEGRATION-REVIEW.md` § 9's recommendation, filed here so it
survives the directory it is written in.

## Why it is not done

The plan that produced these defects has finished. Nothing is broken today;
the cost lands on the next plan, which does not exist yet and will be written
by whoever writes it, possibly from the same template.

## Evidence

Seven, from one run. None is a typo; every one is the same mistake — a command
written by reasoning about the text the author expected rather than running it
against the text that was there.

| # | What | Where |
|---|---|---|
| 1 | A worktree path that has never existed on this machine, in **all seventeen** plan files | `tmp/plan/*.md`, corrected mid-run |
| 2 | `./.build/*/*/cl_..._test` — binaries are six levels down, not two, and `2>/dev/null` swallows the failure, so the check prints nothing and exits 0 | `step-A4.md:308`, `step-A5.md:188`, propagated to B3, B5, B6, B7 |
| 3 | `grep -c` counting lines where the check wanted occurrences; expects 2, prints 0 and 8 | `step-A1.md`, recorded in A1's own metrics row |
| 4 | Test-case arithmetic two over the real drop — two `TEST_CASE` strings live inside comments | `step-A3.md` |
| 5 | "grepping finds only `sbcl_oracle.hpp`" — it also finds `sbcl_differential.test.cpp` | `step-A4.md` |
| 6 | Spot checks forbidding any `.test.cpp` under `src/smd/cl/`, contradicting `AGENT-PROMPT.md`'s standing rule that adding a case is fine | `step-B6.md`, `step-B7.md`, corrected mid-run |
| 7 | `grep and_then` expected to reach zero repo-wide; seven calls survive legitimately, three inside a function the step does not own | `step-B7.md`, corrected mid-run |

**Number 2 is the expensive one**, and it is the argument for step zero on its
own. B1's handoff reported it. **No step file was corrected until B6**, five
steps later, so every intervening step ran a check that could not fail. One of
those checks was the only thing standing between a green matrix and an oracle
that was not running — see
[BL-0011](BL-0011-a-skipped-oracle-reads-as-a-passing-one.md).

**Numbers 3, 4 and 5 are consecutive.** A1, A3 and A4 each caught an arithmetic
error in their own plan's greps. Three steps running is not three unlucky
authors.

**Who found them matters.** Every one was found by an agent executing the step,
or by a drafter reading it afterwards — never by the plan's author, and never
by the plan's own review. A cleared worker is the first entity to run these
commands, and by then the cost of a wrong one is a halt, a workaround, or
silence.

## Open questions

- Does step zero belong in the plan template, in the `plan-fanout` skill that generates it, or both? The skill writes the files; the template is what a human edits afterwards.
- Can any of it be automated? Extracting fenced `sh` blocks and running the read-only ones is plausible; telling a read-only command from a destructive one automatically is not.
- Should a spot check be allowed to use `2>/dev/null` at all? On this evidence the answer looks like no — the flag converts "could not run" into "ran and said nothing".
- Should a check that prints nothing be a failure by convention, so that an empty result is never mistakable for a quiet pass? That is the same question [BL-0011](BL-0011-a-skipped-oracle-reads-as-a-passing-one.md) asks about a skipped test, one level up.
- Is the real defect that these are prose commands at all, rather than a script the plan ships and the worker runs?

## Cost and risk

Small, and paid once per plan. The fourteen-step run measured a ~440-second
cold baseline per step and a roughly 700-second floor per step; executing every
command in a plan's step files is minutes against that, and item 2 alone cost
five steps of false confidence.

The risk of not doing it is not a broken build. It is a plan whose verification
record says more than it checked.

## Decision criteria

Do it when the next plan is written, before its first worker is dispatched.
Close it as declined if the next plan is small enough that its author will run
every command anyway — but say so in writing, because "I would have noticed" is
what the last three authors would have said.
