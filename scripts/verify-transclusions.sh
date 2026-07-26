#!/usr/bin/env bash
# scripts/verify-transclusions.sh
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Verify that every org-transclusion directive in the docs still resolves.
#
# Two document categories, two policies (docs/epistolary-pinning-plan.md):
#
#   posts   docs/blog/phase-*.org transclude via `orgit-file:' links pinned to
#           a `blog/phase-NN' tag. Checked against that tag's tree, NOT the
#           worktree -- a post's code is frozen at the revision its prose was
#           written against, so later refactors may freely move or delete
#           anchors without owing the published past anything.
#
#   living  docs/compiler_architecture.org transcludes via `file:' links and is
#           SUPPOSED to roll forward. Checked against the worktree, so a rename
#           that orphans an anchor is caught before the doc becomes dead
#           narrative.
#
# Exits non-zero on any unresolvable transclusion. A UUID occurring a number of
# times other than two is reported as a discrepancy but is not fatal: see the
# recorded duplicate in docs/blog/pins.md.

set -o nounset
set -o errexit
trap 'echo "Aborting due to errexit on line $LINENO. Exit code: $?" >&2' ERR
set -o errtrace
set -o pipefail
IFS=$'\n\t'

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${repo_root}"

# Usage: verify-transclusions.sh [POSTS_GLOB [LIVING_GLOB [MD_GLOB]]]
#
# The globs exist so the failure paths can be exercised against throwaway
# fixtures without editing a real post; revisions and paths are still resolved
# against this repository. Defaults are the real documents.
posts_glob="${1:-docs/blog/phase-*.org}"
living_glob="${2:-docs/compiler_architecture.org}"
if [[ -n "${3:-}" ]]; then
    md_globs=("$3")
else
    md_globs=('docs/blog/phase-*.md' 'docs/blog/index.md')
fi

failures=0
discrepancies=0
checked=0

fail() {
    printf 'FAIL  %s\n' "$*" >&2
    failures=$((failures + 1))
}

warn() {
    printf 'WARN  %s\n' "$*" >&2
    discrepancies=$((discrepancies + 1))
}

# Count occurrences of a UUID in a blob, and confirm the anchor pair resolves:
# an opening anchor line (the UUID without " end") must precede a line holding
# "<uuid> end". That is what org-transclusion's search-option plus :end does.
check_region() {
    local label="$1" content="$2" uuid="$3" count begin_line end_line
    count="$(printf '%s' "${content}" | grep -c -- "${uuid}" || true)"
    if [[ "${count}" -eq 0 ]]; then
        fail "${label}: uuid ${uuid} absent"
        return
    fi
    begin_line="$(printf '%s' "${content}" | grep -n -- "${uuid}" \
        | grep -v -- "${uuid} end" | head -1 | cut -d: -f1 || true)"
    end_line="$(printf '%s' "${content}" | grep -n -- "${uuid} end" \
        | head -1 | cut -d: -f1 || true)"
    if [[ -z "${begin_line}" ]]; then
        fail "${label}: uuid ${uuid} has no opening anchor (only an end marker)"
        return
    fi
    if [[ -z "${end_line}" ]]; then
        fail "${label}: uuid ${uuid} has no \"${uuid} end\" marker"
        return
    fi
    if [[ "${begin_line}" -ge "${end_line}" ]]; then
        fail "${label}: uuid ${uuid} anchor at line ${begin_line} is not before its end marker at ${end_line}"
        return
    fi
    if [[ "${count}" -ne 2 ]]; then
        warn "${label}: uuid ${uuid} occurs ${count} times, expected 2 (region still resolves; see docs/blog/pins.md)"
    fi
    checked=$((checked + 1))
}

echo "== posts: pinned transclusions (resolved against their blog/phase-NN tag) =="
for post in ${posts_glob}; do
    [[ -e "${post}" ]] || { fail "no posts matched ${posts_glob}"; break; }
    # Reject any worktree-form link left in a post: it would roll forward.
    if grep -q '\[\[orgit:' "${post}"; then
        fail "${post}: worktree-form [[orgit:...]] link in a post; posts must pin via orgit-file:"
    fi
    while IFS= read -r link; do
        # link is orgit-file:REPO:REV:PATH:UUID once "::" is squashed to ":"
        rev="$(cut -d: -f3 <<<"${link}")"
        path="$(cut -d: -f4 <<<"${link}")"
        uuid="$(cut -d: -f5 <<<"${link}")"
        if ! git rev-parse --verify --quiet "${rev}^{commit}" >/dev/null; then
            fail "${post}: rev ${rev} does not resolve (missing tag? fetch tags)"
            continue
        fi
        if ! blob="$(git show "${rev}:${path}" 2>/dev/null)"; then
            fail "${post}: ${path} absent at ${rev}"
            continue
        fi
        check_region "${post} @ ${rev} ${path}" "${blob}" "${uuid}"
    done < <(grep -o '\[\[orgit-file:[^]]*\]\]' "${post}" 2>/dev/null \
        | sed -e 's/^\[\[//' -e 's/\]\]$//' -e 's/::/:/g' || true)
done

echo "== living docs: worktree transclusions (expected to roll forward) =="
for doc in ${living_glob}; do
    [[ -e "${doc}" ]] || { fail "${doc}: missing"; continue; }
    if grep -q '\[\[orgit-file:' "${doc}"; then
        fail "${doc}: pinned orgit-file: link in a living doc; living docs resolve against the worktree"
    fi
    doc_dir="$(dirname "${doc}")"
    while IFS= read -r link; do
        path="$(cut -d: -f1 <<<"${link}")"
        uuid="$(cut -d: -f2 <<<"${link}")"
        target="${doc_dir}/${path}"
        if [[ ! -f "${target}" ]]; then
            fail "${doc}: ${path} does not exist in the worktree"
            continue
        fi
        check_region "${doc} -> ${path}" "$(cat "${target}")" "${uuid}"
    done < <(grep -o '\[\[file:[^]]*\]\]' "${doc}" 2>/dev/null \
        | sed -e 's/^\[\[file://' -e 's/\]\]$//' -e 's/::/:/g' || true)
done

echo "== generated markdown: no unresolved directives =="
# The entries are glob patterns, not filenames, and must undergo pathname
# expansion here -- so the expansion is deliberately unquoted. IFS is \n\t, and
# the -e test below skips a pattern that matched nothing.
# shellcheck disable=SC2048
for md in ${md_globs[*]}; do
    [[ -e "${md}" ]] || continue
    if grep -q '^#+transclude' "${md}"; then
        fail "${md}: contains an unexpanded #+transclude directive"
    fi
done

printf '\n%d transclusions resolved, %d discrepancies, %d failures\n' \
    "${checked}" "${discrepancies}" "${failures}"

if [[ "${failures}" -gt 0 ]]; then
    exit 1
fi
