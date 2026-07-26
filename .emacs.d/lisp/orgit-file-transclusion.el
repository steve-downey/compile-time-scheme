;;; orgit-file-transclusion.el --- transclude UUID-anchored regions at a git rev  -*- lexical-binding: t; -*-
;; SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

;; Provenance: docs/epistolary-pinning-plan.md (section 3), shared across
;; compile-time-scheme sibling repositories by copy, not by load-path coupling.
;;
;; Link form: [[orgit-file:REPO::REV::PATH::UUID]]
;; Used with: :lines 2- :src LANG :end "UUID end"   (unchanged conventions)
;;
;; Where the sibling `orgit:' adapter in init.el resolves PATH against the
;; worktree -- showing the file as of now -- this one resolves it against REV,
;; so an epistolary post keeps showing the code its prose was written about.
;; REV is normally an annotated `blog/phase-NN' tag; see docs/blog/pins.md.
;;
;; No magit dependency: the blob comes from a `git show' subprocess, so batch
;; export stays light. The interactive `orgit-file' package (gggion/orgit-file)
;; uses the same link syntax, so installing it makes these links followable in
;; a live Emacs, but nothing here requires it.

;;; Commentary:
;;
;; Implementation note -- why this extracts a blob to a file instead of
;; slicing the region itself:
;;
;; `org-transclusion-add-src-lines' already implements the exact semantics the
;; posts depend on, and they are fiddlier than they look.  `:end' is EXCLUSIVE
;; of the line holding the end marker; `:lines 2-' counts forward from the
;; anchor line found by the search option, not from the top of the file; and
;; `:src cpp' wraps the result in a fenced block.  Reimplementing that slicing
;; would duplicate three behaviours that must not drift.
;;
;; So this adapter mirrors `org-transclusion-add-orgit': it materialises the
;; pinned blob, rewrites the link in place into a plain `file:' link with the
;; same `::UUID' search option, and returns nil so the standard src-lines
;; handler does the work.  Post attributes keep meaning exactly what they
;; meant under worktree resolution -- by construction, not by imitation.
;;
;; The blob is written under a path that preserves the original repo-relative
;; path, so `find-file-noselect' picks the same major mode it would have for
;; the real file.  `org-link-search', which resolves the `::UUID' anchor, is
;; mode-sensitive, so this is what keeps anchor resolution identical.

;;; Code:

(require 'org-transclusion)
(require 'subr-x)

(defgroup orgit-file-transclusion nil
  "Transclude UUID-anchored source regions pinned to a git revision."
  :group 'org-transclusion)

(defcustom orgit-file-transclusion-cache-dir
  (expand-file-name "orgit-file-transclusion" temporary-file-directory)
  "Directory holding blobs extracted from git.
Contents are disposable: each blob is cached under its resolved commit
SHA, so a stale entry cannot be served for a different revision."
  :type 'directory
  :group 'orgit-file-transclusion)

(defcustom orgit-file-transclusion-base-url
  "https://github.com/steve-downey/compile-time-scheme/blob/"
  "Base URL for exporting `orgit-file' links, with a trailing slash.
The pinned REV is appended, so an exported link is a permalink to the
revision the post was written against rather than to a moving branch."
  :type 'string
  :group 'orgit-file-transclusion)

(defun orgit-file-transclusion--parse (raw)
  "Split RAW into (REPO REV PATH UUID); signal on any other shape."
  (let ((parts (split-string raw "::")))
    (unless (= 4 (length parts))
      (error "orgit-file link needs REPO::REV::PATH::UUID, got: %s" raw))
    parts))

(defun orgit-file-transclusion--rev-parse (repo rev)
  "Resolve REV to a full commit SHA in REPO."
  (with-temp-buffer
    (unless (zerop (call-process "git" nil t nil
                                 "-C" (expand-file-name repo)
                                 "rev-parse" (concat rev "^{commit}")))
      (error "orgit-file: cannot resolve rev %s in %s (fetch tags?): %s"
             rev repo (string-trim (buffer-string))))
    (string-trim (buffer-string))))

(defun orgit-file-transclusion--blob-file (repo rev path)
  "Materialise PATH at REV in REPO as a local file; return its name.
The returned path ends in PATH, so the buffer gets the major mode it
would have had for the real file."
  (let* ((sha (orgit-file-transclusion--rev-parse repo rev))
         (cached (expand-file-name
                  path (expand-file-name sha orgit-file-transclusion-cache-dir))))
    (unless (file-exists-p cached)
      (make-directory (file-name-directory cached) t)
      ;; Write via a temp name and rename, so a failed `git show' can never
      ;; leave a truncated blob behind for a later run to trust.
      (let ((tmp (concat cached ".partial")))
        (with-temp-buffer
          (unless (zerop (call-process "git" nil t nil
                                       "-C" (expand-file-name repo)
                                       "show" (format "%s:%s" sha path)))
            (error "orgit-file: git show %s:%s failed in %s: %s"
                   rev path repo (string-trim (buffer-string))))
          (let ((coding-system-for-write 'no-conversion))
            (write-region (point-min) (point-max) tmp nil 'quiet)))
        (rename-file tmp cached t)))
    cached))

(defun orgit-file-transclusion-add (link _plist)
  "Resolve an `orgit-file' LINK into a `file' link on the pinned blob.
Mutates LINK in place and returns nil, so `org-transclusion-add-src-lines'
applies the post's :lines/:src/:end attributes exactly as it does for a
worktree-resolved link."
  (when (string= "orgit-file" (org-element-property :type link))
    (pcase-let* ((`(,repo ,rev ,path ,uuid)
                  (orgit-file-transclusion--parse
                   (org-element-property :path link)))
                 (blob (orgit-file-transclusion--blob-file repo rev path)))
      ;; Mutate in place so downstream handlers see the resolved file path.
      (org-element-put-property link :type "file")
      (org-element-put-property link :path blob)
      (org-element-put-property link :raw-link (concat "file:" blob "::" uuid))
      (org-element-put-property link :search-option uuid)))
  ;; Always nil: let the next add-function build the payload.
  nil)

(add-hook 'org-transclusion-add-functions #'orgit-file-transclusion-add)

;; Export `orgit-file' links as forge permalinks at the pinned revision.
;; Posts currently carry these links only on `#+transclude:' lines, which
;; org-transclusion consumes before export, so this is for future inline use.
(org-link-set-parameters
 "orgit-file"
 :export
 (lambda (path desc backend)
   (let* ((parts (split-string path "::"))
          (rev (nth 1 parts))
          (filepath (or (nth 2 parts) path))
          (url (concat orgit-file-transclusion-base-url rev "/" filepath)))
     (cond
      ((memq backend '(md gfm)) (format "[`%s`](%s)" (or desc filepath) url))
      ((eq backend 'html)
       (format "<a href=\"%s\"><code>%s</code></a>" url (or desc filepath)))
      (t url)))))

(provide 'orgit-file-transclusion)
;;; orgit-file-transclusion.el ends here
