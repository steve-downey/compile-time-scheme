# BL-0014: the published posts cite internal work-planning identifiers

- **Status:** open — scheduled for after the Phase B merge, on the owner's direction
- **Date:** 2026-09-16
- **Origin:** raised while commissioning the phase 39–46 posts. The rule is the owner's standing one for public writing; the series has never followed it.
- **Frozen-tree impact:** none. Prose only.

## What

Public text has to stand alone: a post must not make a reader open a second
document to follow a sentence. Step names, decision-record numbers, divergence
numbers and plan filenames are all internal work-planning identifiers, and the
series cites them throughout its running text.

Go back over the published posts and say the reason or the effect instead, or
say nothing. Real file paths in the tree, tool and compiler versions, and
anything transcluded all stay — the rule is about identifiers that only mean
something to someone holding the plan.

## Scope, and one thing to settle before starting

**The reading this entry was filed under:** "redoing the history" means the
back catalogue of posts, `docs/blog/phase-*.org` and their rendered `.md`.
Phases 39 onward are being written under the new rule as they are drafted, so
the work is the posts before that point.

**It could instead have meant `docs/history/`** — `architecture-iterations.org`,
`blog-backfill-plan.md`, `handoff-archive.md`. Those are internal documents and
the rule does not reach them; internal docs are supposed to cross-reference
freely, and preferably as real links. If that was the intent, this entry is
aimed at the wrong target and should be rewritten rather than executed.

**Either way, do no prose work in `docs/history/` without asking first.** The
owner said on 2026-09-16 that the directory may eventually be dropped from
`main`, or purged outright as misleading, since its prose describes two front
ends that no longer exist on trunk and can be mistaken for a description of the
live tree. The code is preserved by the `iteration/smdscheme-final` and
`iteration/smdlisp-final` tags regardless. A cleanup pass over a directory
that may be deleted is the worst of both outcomes.

## Why it is not done

It is a prose pass over roughly twenty posts and wants doing in one sitting by
someone applying one standard, not incrementally by whoever touches a post
next. It also wants the question above answered first.

## Evidence

Every post from the third era onward does it. A representative line from
`docs/blog/index.org`: "Decision D17's second backend over the rebuild's core
tree" — a reader without the decision log learns nothing from "D17" that
"the second backend" does not already tell them. The same entry goes on to cite
`DIV-0016` for a cost that the sentence could simply state.

The posts written in this run inherited the pattern deliberately: the house
style was already established, and following it looked more correct than
diverging mid-series. Phases 33 through 38 are merged and cite step names
throughout; 39 and 40 are on the reader-combinator branch.

Every post carries `DRAFT --- pending author revision`, so none of this is
published in a form the author has signed off, which is why this is a cleanup
rather than a correction.

## Open questions

- Answer the scope question above first.
- Do decision records get an exception when the post's subject *is* the decision? A post about why an oracle was retired may need to name the record, or may be better for describing the argument and leaving the number out.
- Do the `index.org` summary entries count as running text? They read as prose and are the most identifier-dense part of the series.
- What replaces a citation that was doing real work — "the rule that a primitive lands with its first consumer" is longer than a number but says the thing; is there a case where no such paraphrase exists?
- Does `pins.md` change? It is bookkeeping rather than running text, and its tag names are real git refs, so probably not.

## Cost and risk

A prose pass over about twenty `.org` files plus `make blog-md` to re-render.
No code, no verification beyond `make lint` and the transclusion check.

The risk is over-applying it: stripping a paraphrase that was carrying weight,
or reaching into internal documents that are supposed to cross-reference
freely. The second is what the scope question guards against.

## Decision criteria

Do it after the Phase B merge, once the last of the phase 39–46 posts has
landed and the whole series can be passed over at once. Close it as declined
only if the owner decides the series' established convention outranks the
general rule — in which case say so in `docs/blog/`, so the next drafter is not
left to infer it.
