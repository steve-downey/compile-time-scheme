;; Save any custom set variable in exordium-custom-file rather than at the end of init.el:
(setq custom-file (locate-user-emacs-file "custom.el"))

(require 'package)
(add-to-list 'package-archives '("melpa" . "https://melpa.org/packages/") t)
(setq package-user-dir
      (locate-user-emacs-file (concat "elpa-" emacs-version)))

(when (fboundp 'native-comp-available-p)
  (setq package-native-compile (native-comp-available-p)))
(package-initialize)

;; Load the packages we need if they are not installed already
(let ((package-pinned-packages (append
                                '((use-package             . "melpa")
                                  (diminish                . "melpa")
                                  (bind-key                . "melpa"))))
      (has-refreshed nil))

  (defun update-package (p  has-refreshed)
    (unless (package-installed-p p)
      (unless has-refreshed
        (message "Refreshing package database...")
        (package-refresh-contents)
        (setq has-refreshed t)
        (message "Done."))
      (package-install p)))

  (dolist (pkg package-pinned-packages)
    (let ((p (car pkg)))
      (update-package p has-refreshed))))

;; This is only needed once, near the top of the file
(eval-when-compile
  ;; Following line is not needed if use-package.el is in ~/.emacs.d
  (require 'use-package))

(require 'use-package-ensure)
(setq use-package-always-ensure t)
(setq use-package-compute-statistics t)
;;; remove a package from the builtin list so it can be upgraded
(defun wg21org-ignore-builtin (pkg)
  (assq-delete-all pkg package--builtins)
  (assq-delete-all pkg package--builtin-versions))


;;; Org mode

(use-package org
  :commands (org-mode)
  :mode (("\\.org\\'" . org-mode))
  :after (flycheck)
  :bind
  (:map org-mode-map
        ([remap org-toggle-comment] . iedit-mode))
  :custom
  (org-confirm-babel-evaluate t)
  (org-fontify-quote-and-verse-blocks t)
  (org-fontify-whole-heading-line t)
  (org-log-into-drawer t)
  (org-src-fontify-natively t)
  (org-src-preserve-indentation t)
  (org-startup-folded nil)
  (org-startup-indented t)
  (org-startup-truncated nil)
  (org-startup-with-inline-images t)
  (org-support-shift-select :always)
  (org-use-sub-superscripts "{}")

  ;; Edit settings
  (org-auto-align-tags nil)
  (org-tags-column 0)
  (org-catch-invisible-edits 'show-and-error)
  (org-special-ctrl-a/e t)
  (org-insert-heading-respect-content t)

  ;; Org styling, hide markup etc.
  (org-hide-emphasis-markers nil)
  (org-pretty-entities t)

  ;; ;;;; code blocks
  (org-confirm-babel-evaluate nil)
  (org-src-window-setup 'reorganize-frame)
  (org-edit-src-persistent-message t)
  (org-src-fontify-natively t)
  (org-src-preserve-indentation t)
  (org-src-tab-acts-natively t)
  (org-edit-src-content-indentation 0)

  ;; ;;;; export
  (org-export-with-toc t)
  (org-export-headline-levels 8)
  (org-export-dispatch-use-expert-ui nil)
  (org-html-htmlize-output-type 'css)
  (org-html-head-include-default-style t)
  (org-html-head-include-scripts t)

  ;;; visual line mode
  (visual-line-mode 1)

  :hook ((org-mode . variable-pitch-mode))

  :init
  (add-hook 'org-src-mode-hool #'(lambda ()
                                   (add-to-list 'flycheck-disabled-checkers 'emacs-lisp-checkdoc)))

  :config
  ;; Enable org-babel for perl, ruby, sh, python, emacs-lisp, C, C++, etc
  ;; TODO: add extra languages configurable by user
  (org-babel-do-load-languages
   'org-babel-load-languages
   `((perl       . t)
     (ruby       . t)
     (shell      . t)
     (python     . t)
     (emacs-lisp . t)
     (C          . t)
     (dot        . t)
     (sql        . t))))

(use-package htmlize
  :ensure t)


(use-package graphviz-dot-mode
  :config
  (setq graphviz-dot-indent-width 4))

(org-babel-do-load-languages
 'org-babel-load-languages
 (append org-babel-load-languages
         '((dot . t))))


(setq plantuml-jar-path "/usr/share/plantuml/plantuml.jar")
(setq plantuml-default-exec-mode 'jar)

(setq org-plantuml-jar-path (expand-file-name "/usr/share/plantuml/plantuml.jar"))
(add-to-list 'org-src-lang-modes '("plantuml" . plantuml))
(org-babel-do-load-languages
 'org-babel-load-languages
 (append org-babel-load-languages
         '((plantuml . t))))

(org-babel-do-load-languages
 'org-babel-load-languages
 (append org-babel-load-languages
         '((ditaa . t))))

(setq org-ditaa-jar-path "/usr/share/ditaa/ditaa.jar")

(setq org-support-shift-select 'always)


;; Reveal.js + Org mode
(use-package org-re-reveal
  :config
  (setq org-re-reveal-root "file:////home/sdowney/bld/reveal.js"))


(use-package org-transclusion
  :after org
  :bind (:map
         org-mode-map
         ("C-c C-x T" . org-transclusion-mode))
  :config
  (org-transclusion-mode 1)
  )


(use-package ox-gfm
  :after org)

(use-package with-editor)

(use-package citeproc :ensure t :after org)

;; Export orgit links as GitHub source browser URLs
(defvar orgit-base-url "https://github.com/steve-downey/compile-time-scheme/blob/main/"
  "Base URL for exporting orgit links.")

(org-link-set-parameters
 "orgit"
 :export (lambda (path desc backend)
           (let* ((parts (split-string path "::"))
                  (filepath (if (> (length parts) 1) (cadr parts) path))
                  (url (concat orgit-base-url filepath)))
             (cond
              ((or (eq 'md backend) (eq 'gfm backend))
               (format "[`%s`](%s)" (or desc filepath) url))
              ((eq 'html backend)
               (format "<a href=\"%s\"><code>%s</code></a>" url (or desc filepath)))
              (t url)))))









;; Allow org-transclusion to resolve orgit links to the local file system
(defun org-transclusion-add-orgit (link plist)
  "Resolve orgit links into file links in-place."
  (when (string= "orgit" (org-element-property :type link))
    (let* ((full-path (org-element-property :path link))
           (parts (split-string full-path "::"))
           (repo-dir (car parts))
           (inner-file (cadr parts))
           (search-uuid (if (> (length parts) 2) (caddr parts) nil))
           (actual-file (expand-file-name inner-file repo-dir))
           (raw-link (concat "file:" actual-file)))

      (when search-uuid
        (setq raw-link (concat raw-link "::" search-uuid)))

      ;; CRITICAL: Mutate the original link in-place so downstream plugins
      ;; (like org-transclusion-src-lines) see the correct file path!
      (org-element-put-property link :type "file")
      (org-element-put-property link :path actual-file)
      (org-element-put-property link :raw-link raw-link)
      (if search-uuid
          (org-element-put-property link :search-option search-uuid)
        (org-element-put-property link :search-option nil))))
  ;; ALWAYS return nil so the NEXT functions (like org-transclusion-add-src-lines) can handle it!
  nil)

(require 'org-transclusion)
(add-hook 'org-transclusion-add-functions 'org-transclusion-add-orgit)

;; Pinned transclusion for the epistolary blog posts.
;;
;; Two document categories, two policies (docs/epistolary-pinning-plan.md):
;;   - living docs (docs/compiler_architecture.org) transclude via `file:' /
;;     `orgit:' and roll forward with the worktree, which is the point of them;
;;   - blog posts transclude via `orgit-file:' pinned to a `blog/phase-NN' tag,
;;     so a later refactor cannot rewrite the code inside a published entry.
;;
;; Both adapters live on `org-transclusion-add-functions' and coexist; the link
;; type selects the policy. See docs/blog/pins.md for the post-to-tag mapping.
(add-to-list 'load-path
             (expand-file-name
              "lisp" (or (and load-file-name
                              (file-name-directory load-file-name))
                         user-emacs-directory)))
(require 'orgit-file-transclusion)
