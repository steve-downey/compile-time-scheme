# Epistolary Transclusion Pinning — Agent Execution Plan

A repository-generic plan for retrofitting an epistolary blog series so that
each post's transcluded code is frozen at the revision the post was written
against, instead of rolling forward with the worktree. Applies to any
repository following the pattern: org posts with UUID-anchored
`#+transclude:` directives resolved against a live worktree, a `make
blog-md` batch-emacs export, directory-per-branch worktrees, `--no-ff`
step/phase merges on `main`, and an editorial contract that posts are
real-time and non-omniscient.

The defect being fixed: a worktree-resolved transclusion shows the file *as
of now*, so a later refactor silently rewrites the code inside an earlier
diary entry while its prose stays frozen — the future leaks into the past.
The fix: pin each post's transcludes to a per-phase git tag, and keep
worktree resolution only for living documents (architecture docs), which
are *supposed* to roll forward.

This plan is executed once per repository, parameterized by the block in
section 1. It is a docs/tooling change only: **no source code is touched,
and no post prose is edited.** Restoring a post's transcluded code to its
historical version is a restoration the epistolary contract requires, not a
rewrite it forbids.

---

# 1. Per-repository parameters

Fill in before dispatch; everything below refers to these names.

Bound for `compile-time-scheme` (2026-07-25):

```txt
REPO        ~/src/compile-time-scheme/main
POSTS       docs/blog/phase-*.org        (9 of 19 carry #+transclude:)
LIVING      docs/compiler_architecture.org
TAGPREFIX   blog/
INIT        .emacs.d/init.el
LISPDIR     .emacs.d/lisp/
CONTRACT    docs/cl-pivot-plan.md section 8 (blog authoring); orchestrator
            duties in section 2 and AGENTS.md
```

Repository-specific notes that qualify the generic plan:

- **Tag names are `blog/phase-NN`, not `part-NN`.** The series numbers itself
  by phase (`phase-16-reading-common-lisp.org`, `#+TITLE: ... Phase 16 - ...`),
  and the number is not zero-padded anywhere in the repository. `blog/phase-16`
  keeps the tag readable as the name the post already uses. Sorting is
  therefore lexical, not numeric; `git tag -n --list 'blog/*'` is an index, not
  a chronology.
- **`LIVING` is already distinguished by link type.** `docs/compiler_architecture.org`
  transcludes with plain `file:` links relative to `docs/`; posts use
  `orgit:`. P1's two categories thus need no new declaration mechanism — the
  link type already carries it, and P3's rewrite touches only `orgit:`.
- **Two authorship eras** (see section 6). Posts 16–18 were written alongside
  their step and pin to that step's `--no-ff` merge. Posts 5–12 were written
  retrospectively about code that already existed, so P4 method 1 has no
  per-phase merge to find; their pin is derived from the post's own authorship
  history instead.
- **`#+DATE` is unreliable for posts 0–13** and must not be used as P4 method 2
  for them: eleven of those posts carry `2026-06-19`, a backfill date from the
  batch that created them, and three carry dates (`2026-05-29`, `2026-06-01`)
  earlier than commits they describe. Method 2 is admissible only for posts
  14–18, whose dates were written the day the step landed.
- **`LISPDIR` is `.emacs.d/lisp/`**, not `etc/`. `INIT` is `.emacs.d/init.el`,
  loaded by the `make blog-md` recipe as `--init-directory=.emacs.d/ --load
  .emacs.d/init.el`; a sibling `lisp/` directory is on the natural load-path
  relative to that init directory and is versioned with it. `etc/` in this
  repository holds CMake toolchain files only.

---

# 2. Decisions

**P1 — Two documents, two policies.**
Epistolary posts pin; living docs roll forward. A transclusion in LIVING
keeps its current worktree link form and its existing verification (anchors
present in the worktree). A transclusion in POSTS carries a revision and is
verified against that revision. No third category: a document is one or the
other, declared in CONTRACT.

**P2 — Tags, not raw SHAs, in links.**
Each post pins to an annotated tag `TAGPREFIX + part-NN` (zero-padded to
the series' width) placed on the merge commit of the **last** phase the
post covers (a post covering an arc of several steps pins to the arc's
final merge). Annotated, because the tag message carries the post title and
the phase identifier, making `git tag -n` a readable index of the series;
signed if the repository signs tags. Tags are pushed to origin. Raw SHAs
never appear in post links — the tag is the human-readable name the link
is allowed to use.

**P3 — Link form.**
The pinned form inserts one `::REV` segment into the existing convention
and changes the link type name; everything else — the UUID anchor pair,
`:lines 2-`, `:src <lang>`, `:end "<uuid> end"` — is unchanged:

```org
#+transclude: [[orgit-file:REPO::TAG::path/to/file.hpp::UUID]] :lines 2- :src cpp :end "UUID end"
```

The name `orgit-file` matches the existing package of that name (gggion/
orgit-file, `orgit-file:REPO::REV::PATH...`), so interactively following a
pinned link works by installing that package, and its forge-URL export
(GitHub/GitLab/Codeberg permalinks with line fragments) becomes available
for free if the repository is ever public. The batch transclusion module
(P5) parses the link itself and does **not** depend on the package, so the
export environment needs neither orgit-file nor magit.

**P4 — Recovering "the code as of the phase."**
The pin for a post is the merge commit its prose was written against.
Recovery methods, in order of preference:

1. **Commit-message convention:** first-parent walk of `main`
   (`git log --first-parent --merges`) matching the phase/step identifier
   the merge message carries.
2. **Post date:** the post's `#+DATE` equals the step's commit date by the
   blog contract; the merge commit on or immediately preceding that date.
3. **Anchor test (always run, as verification even when 1–2 agree):** a
   candidate REV is admissible for a post only if, for every transclusion
   in the post, `git show REV:PATH` exists and contains the UUID exactly
   twice (anchor and end marker). If several consecutive commits are
   admissible, take the one identified by methods 1–2; if none is, the
   post transcludes an anchor added later than its phase (a pre-existing
   leak in the other direction) — pin to the earliest admissible commit
   and record the discrepancy in the fixup commit message.

The output of this analysis is a table `post → phase-id → SHA → tag`,
committed alongside the fixup (a comment block in CONTRACT or a small
`docs/blog/pins.md`), so the mapping is reviewable before tags are pushed.

**P5 — One small elisp module, loaded by INIT.**
A single-file org-transclusion extension, `orgit-file-transclusion.el`,
placed in LISPDIR per repository (copy with a provenance line, in the
import-by-copy style — no cross-repo load-path coupling). It resolves
`orgit-file:` links by shelling out to `git show REV:PATH` — subprocess,
not magit, so batch export stays light and deterministic — and yields the
anchor-sliced region through the same payload contract the existing
worktree resolver uses. Worktree (`orgit:`) resolution stays installed for
LIVING docs; both adapters coexist on the same hook.

**P6 — Regeneration is expected to change published output.**
Re-running `make blog-md` after pinning will alter the generated markdown
wherever drift had already occurred — that diff is the *measured leak*, and
it flows downstream to the rendered blog. This is intentional restoration:
the post now shows the code its prose was describing. The fixup commit
message summarizes which posts changed and why. Prose is never edited;
only link plumbing in the `.org` and the regenerated `.md` change.

**P7 — Export environment constraints.**
The export environment must have full history: shallow clones cannot
resolve old blobs, so any CI job running `make blog-md` fetches with
`--filter=none` depth-unlimited and fetches tags. Tag reachability is
guaranteed by placing tags on first-parent merge commits of `main`.

---

# 3. The module

`LISPDIR/orgit-file-transclusion.el` — the shape; adapt the payload glue
to match the repository's existing worktree adapter (same hook, same
attribute handling).

```elisp
;;; orgit-file-transclusion.el --- transclude UUID-anchored regions at a git rev
;; Provenance: docs/epistolary-pinning-plan.md (this plan), shared across
;; <project> repositories by copy.
;; Link form: [[orgit-file:REPO::REV::PATH::UUID]]
;; Used with:  :lines 2- :src LANG :end "UUID end"   (unchanged conventions)

(require 'org-transclusion)
(require 'subr-x)

(defun orgit-file-tc--parse (raw)
  "Split RAW into (REPO REV PATH UUID); error on any other shape."
  (let ((parts (split-string raw "::")))
    (unless (= 4 (length parts))
      (error "orgit-file link needs REPO::REV::PATH::UUID, got: %s" raw))
    parts))

(defun orgit-file-tc--blob (repo rev path)
  "Contents of PATH at REV in REPO, via git show (no magit dependency)."
  (with-temp-buffer
    (unless (zerop (call-process "git" nil t nil
                                 "-C" (expand-file-name repo)
                                 "show" (format "%s:%s" rev path)))
      (error "git show failed: %s %s:%s" repo rev path))
    (buffer-string)))

(defun orgit-file-tc--slice (content uuid)
  "Region of CONTENT from the line containing UUID through the line
containing \"UUID end\", inclusive.  The :lines attribute (e.g. 2-)
then drops the anchor line, exactly as with worktree transclusion."
  (let* ((lines (split-string content "\n"))
         (end-marker (concat uuid " end"))
         beg end)
    (cl-loop for l in lines and i from 0
             when (and (not beg) (string-search uuid l)
                       (not (string-search end-marker l)))
               do (setq beg i)
             when (and beg (string-search end-marker l))
               do (setq end i) and return nil)
    (unless (and beg end)
      (error "anchor pair %s not found in pinned blob" uuid))
    (string-join (seq-subseq lines beg (1+ end)) "\n")))

(defun orgit-file-transclusion-add (link plist)
  "org-transclusion add-function for orgit-file: links.
Returns the same payload shape as the repository's worktree (orgit:)
adapter, with :src-content sourced from the pinned blob."
  (when (string= "orgit-file" (org-element-property :type link))
    (pcase-let ((`(,repo ,rev ,path ,uuid)
                 (orgit-file-tc--parse (org-element-property :path link))))
      (list :tc-type "orgit-file"
            :src-content (orgit-file-tc--slice
                          (orgit-file-tc--blob repo rev path) uuid)))))
;; Hand :src-content through the same :lines/:src fencing the existing
;; adapter applies (org-transclusion-src-lines machinery or the local
;; equivalent) so post attributes keep meaning what they meant.

(add-hook 'org-transclusion-add-functions #'orgit-file-transclusion-add)

(provide 'orgit-file-transclusion)
```

INIT gains, next to the existing transclusion setup:

```elisp
(add-to-list 'load-path (expand-file-name "LISPDIR" repo-root))
(require 'orgit-file-transclusion)
```

Interactive niceties (optional, per user config rather than repo tooling):
install the orgit-file package so `org-open-at-point` on a pinned link
visits the blob, and its export alist emits forge permalinks once public.

---

# 4. Steps

Per repository; B0–B4 serial, B5 whenever after B2.

## Step B0 — Inventory and pin mapping

Extract every `#+transclude:` from POSTS (post, path, uuid triples); run
the P4 recovery producing the `post → phase-id → SHA → tag` table; run the
anchor test for every row; record discrepancies. No writes to git except
the table file.
Merge criteria: table complete, one row per post; every row passes the
anchor test or carries a recorded discrepancy with the chosen earliest
admissible commit.

## Step B1 — Tags

Create the P2 annotated tags at the mapped commits (`git tag -a TAG SHA
-m "<Part NN: title> (<phase-id>)"`); push tags. No file changes.
Merge criteria: `git tag -n --list 'TAGPREFIX*'` reads as a correct index
of the series; every tag's commit matches the table.

## Step B2 — Module and INIT wiring

Land `orgit-file-transclusion.el` in LISPDIR per section 3; wire INIT;
keep the worktree adapter installed for LIVING.
Merge criteria: a scratch org file transcluding one known region via a
B1 tag exports correctly under the batch INIT; LIVING docs still export
unchanged.

## Step B3 — Rewrite post links

Mechanical rewrite in each post: `orgit:REPO::PATH::UUID` becomes
`orgit-file:REPO::TAG::PATH::UUID` with TAG from the table; attributes
untouched. No prose edits (P6).
Merge criteria: grep shows zero worktree-form transcludes remaining in
POSTS and zero pinned-form transcludes in LIVING.

## Step B4 — Regenerate and verify

`make blog-md`; verify no leftover `#+transclude` directive in any
generated `.md`; for every post link, `git show TAG:PATH` contains the
UUID twice (the anchor test, now as a permanent check — add it to the
repository's doc-verification script alongside the existing worktree
transclusion check, keyed by document category per P1); review the `.md`
diff — changes are restorations of historical code and are enumerated in
the commit message (P6). Commit `.org`, `.md`, module, INIT, table.
Merge criteria: export clean; checks green; diff reviewed and narrated.

## Step B5 — Contract update for future posts

CONTRACT gains the pinned-link rule: the blog agent transcludes via
`orgit-file` with **this step's tag**; the orchestrator creates the tag at
the `--no-ff` merge (tagging joins the orchestrator's post-merge duties,
before blog-agent dispatch, so the tag exists when the agent runs); the
anchor-must-exist-at-the-pin rule replaces anchor-must-exist-in-worktree
for posts — meaning later refactors may move or delete anchors without
owing the published past anything.
Merge criteria: CONTRACT and the orchestrator plan's duty list both name
the tag-at-merge step; a dry-run dispatch checklist shows the tag
available at blog-agent start.

---

# 5. Consequences worth noticing

- **Deletion-proof history.** Once posts are pinned, retiring or deleting
  source files can never break the blog export — `git show TAG:PATH` is
  indifferent to the worktree. Any planned removal of transcluded files
  (pipeline retirements, large refactors) should schedule this plan's B0–B4
  for the affected repository **before** the deletion lands.
- **Determinism.** `make blog-md` becomes reproducible for the back
  catalogue; only the newest unpinned post (if the agent runs before
  tagging, which B5 forecloses) could ever vary.
- **Anchor hygiene decouples.** Living docs still require anchors in the
  worktree; posts require them only at their pin. The two checks are
  separate lines in the verification script, keyed by P1's document
  category.
