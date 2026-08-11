// src/smd/cl/conformance/driver.hpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_CL_CONFORMANCE_DRIVER_HPP
#define SRC_SMD_CL_CONFORMANCE_DRIVER_HPP

#include <smd/cl/core/ast.hpp>
#include <smd/cl/elaborator/elaborate.hpp>
#include <smd/cl/eval/builtin.hpp>
#include <smd/cl/eval/machine.hpp>
#include <smd/cl/eval/outcome.hpp>
#include <smd/cl/eval/value.hpp>
#include <smd/cl/foundation/parse_error.hpp>
#include <smd/cl/foundation/result.hpp>
#include <smd/cl/reader/cursor.hpp>
#include <smd/cl/reader/datum.hpp>
#include <smd/cl/reader/read.hpp>
#include <smd/cl/reader/readtable.hpp>
#include <smd/cl/symbol/symbol_table.hpp>

#include <string_view>

/// The conformance driver: the fourth file of R6, and the answer to R5's
/// step brief noting that driving a program is four steps — install, read,
/// elaborate, run — with no one-shot entry point yet.
///
/// `src/smd/cl/eval/machine.test.cpp` had this chained through
/// `foundation::and_then` twice already, once for a single form and once
/// for two forms sharing one arena; a conformance corpus wants the general
/// case (a source string of *any* number of top-level forms) as a real
/// component, not copied a third time.
namespace smd::cl::conformance {

/// Default datum-tree node capacity for the convenience entry points below.
inline constexpr int default_datum_nodes = 128;

/// Default per-list element capacity for the convenience entry points.
inline constexpr int default_datum_list = 16;

/// Default core-tree node capacity for the convenience entry points.
inline constexpr int default_core_nodes = 192;

/// Default per-node subexpression capacity for the convenience entry points.
inline constexpr int default_core_children = 8;

/// Default symbol-table entry capacity for the convenience entry points.
inline constexpr int default_max_symbols = 96;

/// Default symbol-table name-character capacity for the convenience entry
/// points.
inline constexpr int default_max_name_chars = 768;

/// Default machine limits for the convenience entry points: generous enough
/// for a corpus case, never load-bearing for what a case is testing.
inline constexpr eval::limits default_limits{};

namespace detail {

/// Advances past whitespace and `;` line comments between top-level forms.
///
/// This is not @ref reader::detail::skip_intertoken_space — that function is
/// reader-private, and duplicating its block-comment handling here is not
/// worth it for a driver whose corpus controls its own spelling. A `#|...|#`
/// block comment between two top-level forms is simply outside what this
/// unfold recognizes; nothing in the conformance corpus needs one there.
[[nodiscard]] constexpr auto skip_between_forms(reader::cursor cur,
                                                reader::readtable const &table)
    -> reader::cursor {
    while (true) {
        while (!cur.empty() && table.is_whitespace(cur.peek())) {
            cur = cur.bump();
        }
        if (cur.empty() ||
            table.macro_of(cur.peek()) != reader::macro_kind::semicolon) {
            return cur;
        }
        while (!cur.empty() && cur.peek() != '\n') {
            cur = cur.bump();
        }
    }
}

} // namespace detail

// 74829ac7-6ad9-4158-b990-7c59ffa5b40b
/// Reads every top-level datum in @p source, interning into @p symbols and
/// elaborating each into @p program — which may already hold earlier forms
/// — and returns each form's root index, in source order.
///
/// This is the sequence unfold R5's step brief left available but unwritten:
/// @ref reader::read_datum already returns the cursor positioned after the
/// datum it read, so reading "the rest" is repeating the call against that
/// cursor until only intertoken space remains. A program is one core arena
/// and not several, per @ref elaborator::elaborate_into's own contract, so
/// every root this returns shares @p program with every other.
///
/// @tparam DatumNodes  Datum-tree node capacity, shared by every top-level
///                     form read (a fresh tree per form, not accumulated).
/// @tparam DatumList   Datum-tree per-list capacity.
/// @tparam SymbolTable The symbol table instantiation.
/// @tparam CoreTree    The @ref core::core_tree instantiation being filled.
/// @return The root index of each top-level form, left to right, or the
///         leftmost failure — including "no forms to read" for a source
///         that is empty once whitespace and comments are removed.
template <int DatumNodes, int DatumList, class SymbolTable, class CoreTree>
[[nodiscard]] constexpr auto read_and_elaborate_all(std::string_view source,
                                                    SymbolTable &symbols,
                                                    CoreTree &program)
    -> foundation::result<typename CoreTree::child_list> {
    typename CoreTree::child_list roots;
    reader::cursor cur = detail::skip_between_forms(reader::cursor{source},
                                                    reader::standard_readtable);
    if (cur.empty()) {
        return foundation::parse_error{{}, "no forms to read"};
    }
    while (!cur.empty()) {
        auto const read_r =
            reader::read_datum<DatumNodes, DatumList>(cur, symbols);
        if (!read_r.has_value()) {
            return read_r.error();
        }
        auto const root_r =
            elaborator::elaborate_into(read_r.value().value, symbols, program);
        if (!root_r.has_value()) {
            return root_r.error();
        }
        if (roots.size() >= roots.capacity()) {
            return foundation::parse_error{{}, "too many top-level forms"};
        }
        roots.push_back(root_r.value());
        cur = detail::skip_between_forms(read_r.value().rest,
                                         reader::standard_readtable);
    }
    return roots;
}
// 74829ac7-6ad9-4158-b990-7c59ffa5b40b end

namespace detail {

/// Returns @p roots as a single core index: the root itself when there is
/// only one, otherwise a fresh @ref core::core_progn wrapping all of them —
/// "this form, then that one" is what a program of several top-level forms
/// means, the same reading @c machine.test.cpp's `evaluate_both` gave it by
/// hand.
template <class CoreTree>
[[nodiscard]] constexpr auto
combine_roots(CoreTree &program, typename CoreTree::child_list const &roots)
    -> foundation::result<int> {
    if (roots.size() == 1) {
        return roots[0];
    }
    if (program.size() >= program.capacity()) {
        return foundation::parse_error{{}, "core tree full"};
    }
    return program.add_branch(core::core_tag{core::core_progn{}}, roots);
}

} // namespace detail

/// What one call to @ref evaluate_program found: how the program completed,
/// the resolved `NIL`/`T` ids to compare a symbol result against, and —
/// when a witness name was asked for — whether that symbol acquired a
/// function definition.
///
/// The witness field generalizes the pattern `machine.test.cpp` used to
/// observe `unwind-protect`'s cleanup running: a corpus case that wants to
/// know "did this side effect happen" cannot ask the outcome alone, because
/// a cleanup's own value is discarded by design (@c detail::k_protect keeps
/// only the protected form's outcome). Asking whether a chosen symbol's
/// function slot became defined is the same probe @c machine.test.cpp
/// reaches for, formalized so a corpus case does not hand-roll it.
struct program_report {
    eval::outcome how;            ///< The program's completion.
    eval::standard_symbols known; ///< Resolved `NIL` and `T`.
    bool witness_defined = false; ///< Whether the witness symbol, if any,
                                  ///< acquired a function definition.
};

// e0cc2579-ecbe-41f7-973e-5649f0ba5bc6
/// Runs @p source — one or more top-level forms — start to finish: installs
/// the builtins, reads and elaborates every form into one arena, wraps more
/// than one in a `progn`, and evaluates the result.
///
/// This formalizes the chain `install_builtins` -> `read` ->
/// `elaborate` -> `machine::run` that R5 left open-coded in
/// `eval/machine.test.cpp`'s local `evaluate`/`evaluate_both` helpers,
/// generalized from "one form" and "exactly two forms" to any number, which
/// is what a conformance corpus needs — its source spellings are not bounded
/// to one or two top-level forms the way a hand-written unit test's are.
///
/// A read or elaborate failure and a run-time failure both funnel into the
/// same @ref eval::outcome, exactly as `machine.test.cpp`'s helpers already
/// did: a corpus case that expects an error should not have to know which
/// stage produced it, only that the program never reached a value.
///
/// @tparam Limits         Machine storage and step budget.
/// @tparam DatumNodes     Datum-tree node capacity.
/// @tparam DatumList      Datum-tree per-list capacity.
/// @tparam CoreNodes      Core-tree node capacity.
/// @tparam CoreChildren   Core-tree per-node subexpression capacity.
/// @tparam MaxSymbols     Symbol-table entry capacity.
/// @tparam MaxNameChars   Symbol-table name-character capacity.
/// @param  source         One or more top-level forms.
/// @param  witness_name   A symbol name to check for a function definition
///                        after the run, or empty to skip the check.
template <
    eval::limits Limits = default_limits, int DatumNodes = default_datum_nodes,
    int DatumList = default_datum_list, int CoreNodes = default_core_nodes,
    int CoreChildren = default_core_children,
    int MaxSymbols = default_max_symbols,
    int MaxNameChars = default_max_name_chars>
[[nodiscard]] constexpr auto
evaluate_program(std::string_view source, std::string_view witness_name = {})
    -> program_report {
    using table_type = eval::symbol_table<MaxSymbols, MaxNameChars>;
    using program_type = core::core_tree<CoreNodes, CoreChildren>;

    table_type symbols;
    program_type program;
    eval::standard_symbols known{};
    auto const ran = foundation::and_then(
        eval::install_builtins(symbols),
        [&](eval::standard_symbols k) -> foundation::result<eval::outcome> {
            known = k;
            return foundation::and_then(
                read_and_elaborate_all<DatumNodes, DatumList>(source, symbols,
                                                              program),
                [&](typename program_type::child_list const &roots)
                    -> foundation::result<eval::outcome> {
                    return foundation::and_then(
                        detail::combine_roots(program, roots),
                        [&](int root) -> foundation::result<eval::outcome> {
                            program.set_root(root);
                            eval::machine<Limits, program_type, table_type>
                                running{program, symbols, k};
                            return running.run();
                        });
                });
        });
    bool witness_defined = false;
    if (!witness_name.empty()) {
        if (auto const found = symbols.find(witness_name)) {
            witness_defined = symbols.function(*found).has_value();
        }
    }
    return program_report{ran.has_value() ? ran.value()
                                          : eval::outcome{ran.error()},
                          known, witness_defined};
}
// e0cc2579-ecbe-41f7-973e-5649f0ba5bc6 end

} // namespace smd::cl::conformance

#endif
