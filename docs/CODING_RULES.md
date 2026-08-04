<!-- markdownlint-disable MD013 -->

# Coding Rules

This document defines project coding rules for all contributors.
These rules apply to human and AI authors.

This file supersedes rules that previously lived under import-only trees.

## Semantic Defaults

- Default tree Applicative semantics are monad-derived.
- Zip semantics are an alternate tree Applicative, not the default.
- Range Applicative models nondeterminism.
- ZipList Applicative models positional parallel behavior.

When older slideware guidance conflicts with these defaults, this file wins.

## Project Layout

- Generate new C++ code in merged src layout.
- Place headers, implementations, and tests together under src/<namespace-path>/.
- Do not default to split include/ and tests/ trees unless you are working in an older subtree that already uses that style.
- Model each logical component as a local trio: <name>.hpp or <name>.h, <name>.cpp, and <name>.test.cpp or <name>.t.cpp.
- Prefer .hpp and .test.cpp for new standalone work.

## File Prolog and Includes

- Put a canonical repository-relative path comment and Emacs mode line on the first line of source-like files.
- Put SPDX on the first comment-capable line after the path comment.
- Make canonical include paths follow namespace paths.
- Treat canonical include spelling as authoritative.
- Never include project headers via . or .. relative paths.
- Never depend on non-canonical relative include spelling.

## CMake and Build Graph

- Use target-based CMake.
- Define a target first, then attach sources and headers with target_sources.
- Use file sets for headers and modules when relevant.
- Keep CMAKE_VERIFY_INTERFACE_HEADER_SETS enabled where practical.
- Keep each CMakeLists local to its directory.
- List only local files in each CMakeLists.
- Delegate only to immediate child directories.
- Do not reach upward with .. or sideways into peer subtrees.
- Use INTERFACE libraries only for truly header-only code.
- Use concrete library targets for compiled code.
- Export and install targets, not loose files.
- Prefer find_package for dependencies.
- Any fallback download mechanism must be opt-in and outside normal target definitions.

## C++ Structure

- Declare functions before defining them.
- Keep regular member and free-function definitions out of class bodies when practical.
- Define functions out of line and qualify with full namespace and class scope.
- Allow one ordinary exception: hidden friends for customization points.
- Keep hidden friends short, idiomatic, and clearly marked.
- Make headers self-contained.
- Include the component header first in each .cpp.
- Do not rely on transitive includes.
- Do not use using namespace in headers.
- Use classical include guards, not `#pragma once`.
- Guard names should use full repo-relative path pattern, for example `INCLUDE_SMD_TREE_FIX_TREE_HPP`.
- Multiple namespace levels are allowed.
- Namespace levels should correspond to directories.

## Language and Tooling

- C++26 is the baseline; do not add fallback paths for older standards.
- If an API can be meaningfully constexpr, make it constexpr and add compile-time tests.
- Treat formatter and lint configuration as binding contract.
- Assume clang-format, CMake formatting, spell check, and pre-commit checks are required.
- Write code that tools can normalize cleanly.

## Typeclass Design

- Typeclass interfaces are concept-map-like records of named operations.
- Lookup is via variable-template-selected typeclass objects.
- Generic algorithms may also accept explicit or NTTP-pinned instance objects.
- Datatypes and typeclass adaptations are separate concerns.
- Do not place Foldable, Applicative, or Traversable specialization logic directly into core data-structure headers unless that header is the designated adapter location.

## Foldable Rules

- `fold_map` is the semantic center.
- `length`, `to_vector`, `fold_left`, and `fold_right` should be derived where practical.
- Tree traversal order is part of the instance contract and must be documented.

## Applicative Rules

- The public surface is `invoke(f, ax, ay, ...)`.
- Implementor-facing primitives may be `pure` and `apply`.
- Tree Applicative semantics must be explicit.
- Alternate semantics that flatten, duplicate, expand, or reorder structure may exist as alternate maps, not silent replacements for the default.

## Traversable Rules

- `traverse` is the minimal operation.
- Traversal preserves shape.
- Traversal uses the documented Foldable order unless explicitly documented otherwise.
- Effect order is observable and therefore part of the contract.

## Test Rules

- New tests use Catch2.
- Add law-focused tests before performance tests.
- Keep optional Applicative test support separate from monoid test support when possible.
- Include idempotent-header checks in test translation units where practical and low-noise.

## Slide and Transclusion Rules

- Code shown in slides should come from real source via UUID anchors.
- Do not transclude include guards, duplicate includes, or physical boilerplate.
- Prefer short executable examples for transclusion.
- One UUID block should represent one slide concept.
- Prefer not to nest UUID blocks, since the inner markers show up inside the outer region's transcluded text. In this repository this is a legibility preference and not a rule: decision D21 (`docs/cl-rebuild-plan.md`) makes the anchor set the property of the step that lands it, obliging only that it parses at that step's merge.
- Do not invent illustrative code that does not compile.
- Prefer example files under `src/smd/typeclass/examples/` for slide snippets over transcluding production headers directly.

## Prose and Documentation Formatting

- Use one sentence per line in Markdown, Org, and LaTeX sources.
- Do not hard-wrap prose to a fixed column boundary.
- Keep sentence boundaries stable so diffs do not spill across unrelated text.
