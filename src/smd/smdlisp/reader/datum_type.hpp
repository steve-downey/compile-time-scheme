// src/smd/smdlisp/reader/datum_type.hpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDLISP_READER_DATUM_TYPE_HPP
#define SRC_SMD_SMDLISP_READER_DATUM_TYPE_HPP

#include <smd/smdlisp/reader/atom.hpp>
#include <smd/smdscheme/foundation/arena_box.hpp>
#include <smd/smdscheme/foundation/fix.hpp>
#include <smd/smdscheme/foundation/static_vector.hpp>

#include <variant>

namespace smd::smdlisp::reader {

/// A Common Lisp integer datum, e.g. @c 42.
struct datum_integer {
    int value{};
};

// eb76ff97-b500-454c-8ad0-9b71c6db4598
/// A Common Lisp symbol datum, folded to uppercase at read time per decision
/// D2 (e.g. source @c foo becomes the datum @c FOO).
///
/// Storage is the same @ref folded_name the atom reader already produces
/// (own fixed storage, not a view into source) — the datum layer does not
/// re-derive or re-fold anything.
struct datum_symbol {
    folded_name name{}; ///< The folded symbol spelling.
};

/// A Common Lisp keyword datum (@c :foo), folded to uppercase per decision
/// D2. Distinct from @ref datum_symbol per decision D7: keyword-ness is a
/// separate datum kind, not a naming convention layered on top of a symbol.
struct datum_keyword {
    folded_name name{}; ///< The folded keyword spelling, leading colon
                        ///< stripped.
};
// eb76ff97-b500-454c-8ad0-9b71c6db4598 end

/// A Common Lisp list datum, e.g. @c (a b c).
///
/// Elements are stored as @ref smdscheme::foundation::arena_box handles
/// into the reader arena, avoiding recursion during the parse phase. There
/// is no dotted-pair reader syntax (decision D6, deferred until a later
/// step needs it); every list is a proper list at the reader level.
///
/// @tparam R        The recursive element type (the datum fix-point).
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum number of list elements.
template <typename R, int MaxNodes, int MaxList>
struct datum_list {
    smdscheme::foundation::static_vector<
        smdscheme::foundation::arena_box<R, MaxNodes>, MaxList>
        elements{};
};

/// A Common Lisp quote form, e.g. @c 'x, stored as a handle to the quoted
/// datum.
///
/// @tparam R        The recursive element type.
/// @tparam MaxNodes Arena capacity.
template <typename R, int MaxNodes>
struct datum_quote {
    smdscheme::foundation::arena_box<R, MaxNodes>
        quoted{}; ///< Handle to the quoted datum.
};

// d0fdf4c8-643f-44a6-8587-0f0c6d90d8c8
/// A Common Lisp sharpsign-quote form, e.g. @c #'f, stored as a handle to
/// the referenced datum.
///
/// Per docs/cl-pivot-plan.md step L6, @c #'x lowers to this dedicated datum
/// kind rather than to a @c (function x) list: the reader preserves source
/// reality (there is no list token in @c #'f), mirroring how @c 'x is
/// represented as a @ref datum_quote node today rather than expanded to
/// @c (quote x) at read time. Downstream passes (the elaborator) decide
/// what @c datum_function means; the reader only records that the source
/// spelled a sharpsign-quote.
///
/// @tparam R        The recursive element type.
/// @tparam MaxNodes Arena capacity.
template <typename R, int MaxNodes>
struct datum_function {
    smdscheme::foundation::arena_box<R, MaxNodes>
        target{}; ///< Handle to the referenced datum.
};
// d0fdf4c8-643f-44a6-8587-0f0c6d90d8c8 end

/// A Common Lisp backquote template, e.g. `` `(a ,x) ``.
///
/// Per docs/cl-pivot-plan.md step L18, a backquote form lowers to this
/// dedicated datum kind holding a handle to its (unexpanded) template
/// datum, mirroring how `` 'x `` and `` #'x `` get their own dedicated
/// kinds (@ref datum_quote, @ref datum_function) rather than being
/// desugared to an ordinary list at read time: the reader preserves source
/// reality, and downstream passes decide what the template means. What a
/// backquote template expands into -- `cons`/`list`/`append` builds -- is a
/// macroexpand-layer concern (`smd::smdlisp::macroexpand`), not the
/// reader's.
///
/// @tparam R        The recursive element type.
/// @tparam MaxNodes Arena capacity.
template <typename R, int MaxNodes>
struct datum_backquote {
    smdscheme::foundation::arena_box<R, MaxNodes>
        templ{}; ///< Handle to the (unexpanded) template datum.
};

/// A Common Lisp unquote, e.g. `` ,x ``.
///
/// Well-formed only within an enclosing @ref datum_backquote template; a
/// bare unquote elsewhere is a diagnosed error at the macroexpand layer
/// (the reader itself does not track backquote nesting). Stored as a
/// handle to the unquoted (code) datum.
///
/// @tparam R        The recursive element type.
/// @tparam MaxNodes Arena capacity.
template <typename R, int MaxNodes>
struct datum_unquote {
    smdscheme::foundation::arena_box<R, MaxNodes>
        target{}; ///< Handle to the unquoted (code) datum.
};

/// A Common Lisp unquote-splicing, e.g. `` ,@xs ``.
///
/// Well-formed only as a direct list element within an enclosing
/// @ref datum_backquote template; anywhere else is a diagnosed error at the
/// macroexpand layer. Stored as a handle to the spliced (code) datum.
///
/// @tparam R        The recursive element type.
/// @tparam MaxNodes Arena capacity.
template <typename R, int MaxNodes>
struct datum_unquote_splice {
    smdscheme::foundation::arena_box<R, MaxNodes>
        target{}; ///< Handle to the spliced (code) datum.
};

/// Factory template that produces the open-recursive functor used by
/// @ref datum_type.
///
/// The @c type member template is the @c std::variant layer for one level
/// of the datum tree; @ref smdscheme::foundation::fix ties the recursion.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
template <int MaxNodes, int MaxList>
struct datum_f_factory {
    /// The one-layer variant type; @p R is the recursive self-reference.
    template <typename R>
    using type =
        std::variant<datum_integer, datum_symbol, datum_keyword,
                     datum_list<R, MaxNodes, MaxList>, datum_quote<R, MaxNodes>,
                     datum_function<R, MaxNodes>, datum_backquote<R, MaxNodes>,
                     datum_unquote<R, MaxNodes>,
                     datum_unquote_splice<R, MaxNodes>>;
};

/// The concrete recursive Common Lisp datum type, formed as the fixed point
/// of @ref datum_f_factory.
///
/// @tparam MaxNodes Maximum tree nodes (arena size).
/// @tparam MaxList  Maximum list elements per node.
template <int MaxNodes, int MaxList>
using datum_type = smdscheme::foundation::fix<
    datum_f_factory<MaxNodes, MaxList>::template type>;

} // namespace smd::smdlisp::reader

#endif
