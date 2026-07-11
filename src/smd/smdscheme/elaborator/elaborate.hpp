// src/smd/smdscheme/elaborator/elaborate.hpp -*-C++-*- SPDX-License-Identifier:
// Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_ELABORATOR_ELABORATE_HPP
#define SRC_SMD_SMDSCHEME_ELABORATOR_ELABORATE_HPP

#include <smd/smdscheme/elaborator/elaborated_core.hpp>
#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/reader/read_datum.hpp>

namespace smd::smdscheme::elaborator {
namespace detail {

/// Converts a quoted datum into its atom representation.
///
/// Only integer, boolean, and symbol atoms are supported; nested lists in
/// quote position return an error.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
/// @param  d      The datum to quote.
/// @param  arena  The datum arena (used only for type information here).
template <int MaxNodes, int MaxList>
constexpr auto elaborate_quote(
    reader::datum_type<MaxNodes, MaxList> const &d,
    const foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>,
                                 MaxNodes> & /*arena*/)
    -> foundation::result<std::variant<int, bool, std::string_view>> {
    if (std::holds_alternative<reader::datum_integer>(d.inner)) {
        return std::variant<int, bool, std::string_view>{
            std::get<reader::datum_integer>(d.inner).value};
    }
    if (std::holds_alternative<reader::datum_boolean>(d.inner)) {
        return std::variant<int, bool, std::string_view>{
            std::get<reader::datum_boolean>(d.inner).value};
    }
    if (std::holds_alternative<reader::datum_symbol>(d.inner)) {
        return std::variant<int, bool, std::string_view>{
            std::get<reader::datum_symbol>(d.inner).name};
    }
    return foundation::parse_error{
        {}, "quote: lists/nested quotes not yet supported"};
}

/// Forward declaration (mutual recursion with @c elaborate_list).
template <int MaxNodes, int MaxList>
constexpr auto elaborate_node(
    reader::datum_type<MaxNodes, MaxList> const &d,
    const foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>,
                                 MaxNodes> &datum_arena,
    foundation::tree_arena<core_type<MaxNodes, MaxList>, MaxNodes> &core_arena)
    -> foundation::result<core_type<MaxNodes, MaxList>>;

/// Elaborates a datum list into a core form.
///
/// Recognizes the special forms @c if, @c quote, @c define, and @c lambda
/// by inspecting the leading symbol.  Any other head is treated as a
/// general application.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
/// @param  lst         The list datum to elaborate.
/// @param  datum_arena Read-only datum arena.
/// @param  core_arena  Core arena receiving elaborated nodes.
template <int MaxNodes, int MaxList>
constexpr auto elaborate_list(
    reader::datum_list<reader::datum_type<MaxNodes, MaxList>, MaxNodes,
                       MaxList> const &lst,
    const foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>,
                                 MaxNodes> &datum_arena,
    foundation::tree_arena<core_type<MaxNodes, MaxList>, MaxNodes> &core_arena)
    -> foundation::result<core_type<MaxNodes, MaxList>> {
    using core = core_type<MaxNodes, MaxList>;
    using core_f =
        typename core_f_factory<MaxNodes, MaxList>::template type<core>;

    if (lst.elements.empty()) {
        return foundation::parse_error{{}, "empty application"};
    }

    auto const &first = datum_arena.get(lst.elements[0]);
    if (std::holds_alternative<reader::datum_symbol>(first.inner)) {
        auto name = std::get<reader::datum_symbol>(first.inner).name;

        if (name == "if") {
            // d33ff3ee-aee2-4dd8-b9af-28f4fb59584e
            if (lst.elements.size() != 4)
                return foundation::parse_error{{}, "if: expected 3 arguments"};

            auto cond_r = elaborate_node<MaxNodes, MaxList>(
                datum_arena.get(lst.elements[1]), datum_arena, core_arena);
            if (!cond_r.has_value())
                return cond_r;

            auto cons_r = elaborate_node<MaxNodes, MaxList>(
                datum_arena.get(lst.elements[2]), datum_arena, core_arena);
            if (!cons_r.has_value())
                return cons_r;

            auto alt_r = elaborate_node<MaxNodes, MaxList>(
                datum_arena.get(lst.elements[3]), datum_arena, core_arena);
            if (!alt_r.has_value())
                return alt_r;

            return core{core_f{core_if<core, MaxNodes>{
                make_arena_box(core_arena, std::move(cond_r.value())),
                make_arena_box(core_arena, std::move(cons_r.value())),
                make_arena_box(core_arena, std::move(alt_r.value()))}}};
            // d33ff3ee-aee2-4dd8-b9af-28f4fb59584e end
        }

        if (name == "quote") {
            if (lst.elements.size() != 2)
                return foundation::parse_error{{},
                                               "quote: expected 1 argument"};
            auto atom_r = elaborate_quote<MaxNodes, MaxList>(
                datum_arena.get(lst.elements[1]), datum_arena);
            if (!atom_r.has_value())
                return foundation::parse_error{{}, atom_r.error().message};
            return core{core_f{core_quote{atom_r.value()}}};
        }

        if (name == "define") {
            if (lst.elements.size() != 3)
                return foundation::parse_error{
                    {}, "define: expected name and value"};
            auto const &name_node = datum_arena.get(lst.elements[1]);
            if (!std::holds_alternative<reader::datum_symbol>(name_node.inner))
                return foundation::parse_error{{},
                                               "define: name must be a symbol"};

            auto val_r = elaborate_node<MaxNodes, MaxList>(
                datum_arena.get(lst.elements[2]), datum_arena, core_arena);
            if (!val_r.has_value())
                return val_r;

            return core{core_f{core_define<core, MaxNodes>{
                std::get<reader::datum_symbol>(name_node.inner).name,
                make_arena_box(core_arena, std::move(val_r.value()))}}};
        }

        if (name == "lambda") {
            if (lst.elements.size() != 3)
                return foundation::parse_error{
                    {},
                    "lambda: expected formals and body (only 1 "
                    "expr body supported)"};

            auto const &formals_node = datum_arena.get(lst.elements[1]);
            if (!std::holds_alternative<reader::datum_list<
                    reader::datum_type<MaxNodes, MaxList>, MaxNodes, MaxList>>(
                    formals_node.inner))
                return foundation::parse_error{
                    {}, "lambda: formals must be a list"};

            core_lambda<core, MaxNodes, MaxList> lam{};
            auto const &formals = std::get<reader::datum_list<
                reader::datum_type<MaxNodes, MaxList>, MaxNodes, MaxList>>(
                formals_node.inner);
            for (int i = 0; i < formals.elements.size(); ++i) {
                auto const &p = datum_arena.get(formals.elements[i]);
                if (!std::holds_alternative<reader::datum_symbol>(p.inner))
                    return foundation::parse_error{
                        {}, "lambda: formal must be a symbol"};
                auto p_name = std::get<reader::datum_symbol>(p.inner).name;
                for (auto const &existing : lam.params) {
                    if (existing == p_name)
                        return foundation::parse_error{{},
                                                       "duplicate parameter"};
                }
                lam.params.push_back(p_name);
            }

            auto body_r = elaborate_node<MaxNodes, MaxList>(
                datum_arena.get(lst.elements[2]), datum_arena, core_arena);
            if (!body_r.has_value())
                return body_r;
            lam.body = make_arena_box(core_arena, std::move(body_r.value()));

            return core{core_f{std::move(lam)}};
        }

        if (name == "let") {
            // (let ((name expr) ...) body) → ((lambda (name ...) body) expr
            // ...)
            if (lst.elements.size() != 3)
                return foundation::parse_error{
                    {}, "let: expected bindings and body"};

            auto const &bindings_node = datum_arena.get(lst.elements[1]);
            using DatumList =
                reader::datum_list<reader::datum_type<MaxNodes, MaxList>,
                                   MaxNodes, MaxList>;
            if (!std::holds_alternative<DatumList>(bindings_node.inner))
                return foundation::parse_error{{},
                                               "let: bindings must be a list"};

            auto const &bindings = std::get<DatumList>(bindings_node.inner);

            using DatumT = reader::datum_type<MaxNodes, MaxList>;
            using DatumHandle = foundation::arena_box<DatumT, MaxNodes>;

            core_lambda<core, MaxNodes, MaxList> lam{};
            foundation::static_vector<DatumHandle, MaxList> arg_ids;

            for (int i = 0; i < bindings.elements.size(); ++i) {
                auto const &pair_node = datum_arena.get(bindings.elements[i]);
                if (!std::holds_alternative<DatumList>(pair_node.inner))
                    return foundation::parse_error{
                        {}, "let: each binding must be a list"};
                auto const &pair = std::get<DatumList>(pair_node.inner);
                if (pair.elements.size() != 2)
                    return foundation::parse_error{
                        {}, "let: each binding must have 2 elements"};

                auto const &name_node = datum_arena.get(pair.elements[0]);
                if (!std::holds_alternative<reader::datum_symbol>(
                        name_node.inner))
                    return foundation::parse_error{
                        {}, "let: binding name must be a symbol"};
                auto p_name =
                    std::get<reader::datum_symbol>(name_node.inner).name;

                for (auto const &existing : lam.params) {
                    if (existing == p_name)
                        return foundation::parse_error{
                            {}, "let: duplicate binding name"};
                }
                lam.params.push_back(p_name);
                arg_ids.push_back(pair.elements[1]);
            }

            auto body_r = elaborate_node<MaxNodes, MaxList>(
                datum_arena.get(lst.elements[2]), datum_arena, core_arena);
            if (!body_r.has_value())
                return body_r;
            lam.body = make_arena_box(core_arena, std::move(body_r.value()));

            core_application<core, MaxNodes, MaxList> app{};
            app.func = make_arena_box(core_arena, core{core_f{std::move(lam)}});

            for (int i = 0; i < arg_ids.size(); ++i) {
                auto arg_r = elaborate_node<MaxNodes, MaxList>(
                    datum_arena.get(arg_ids[i]), datum_arena, core_arena);
                if (!arg_r.has_value())
                    return arg_r;
                app.args.push_back(
                    make_arena_box(core_arena, std::move(arg_r.value())));
            }

            return core{core_f{std::move(app)}};
        }

        if (name == "let*") {
            // (let* ((name expr) ...) body)
            // → nested lets: (let ((n1 e1)) (let ((n2 e2)) ... body))
            if (lst.elements.size() != 3)
                return foundation::parse_error{
                    {}, "let*: expected bindings and body"};

            auto const &bindings_node = datum_arena.get(lst.elements[1]);
            using DatumList =
                reader::datum_list<reader::datum_type<MaxNodes, MaxList>,
                                   MaxNodes, MaxList>;
            if (!std::holds_alternative<DatumList>(bindings_node.inner))
                return foundation::parse_error{{},
                                               "let*: bindings must be a list"};

            auto const &bindings = std::get<DatumList>(bindings_node.inner);

            if (bindings.elements.empty()) {
                return elaborate_node<MaxNodes, MaxList>(
                    datum_arena.get(lst.elements[2]), datum_arena, core_arena);
            }

            // Elaborate innermost binding outward: build nested
            // ((lambda (nK) body) eK) from the last binding, then wrap
            // each preceding binding around it.
            auto inner = elaborate_node<MaxNodes, MaxList>(
                datum_arena.get(lst.elements[2]), datum_arena, core_arena);
            if (!inner.has_value())
                return inner;

            for (int i = bindings.elements.size() - 1; i >= 0; --i) {
                auto const &pair_node = datum_arena.get(bindings.elements[i]);
                if (!std::holds_alternative<DatumList>(pair_node.inner))
                    return foundation::parse_error{
                        {}, "let*: each binding must be a list"};
                auto const &pair = std::get<DatumList>(pair_node.inner);
                if (pair.elements.size() != 2)
                    return foundation::parse_error{
                        {}, "let*: each binding must have 2 elements"};

                auto const &bname_node = datum_arena.get(pair.elements[0]);
                if (!std::holds_alternative<reader::datum_symbol>(
                        bname_node.inner))
                    return foundation::parse_error{
                        {}, "let*: binding name must be a symbol"};
                auto bname =
                    std::get<reader::datum_symbol>(bname_node.inner).name;

                auto val_r = elaborate_node<MaxNodes, MaxList>(
                    datum_arena.get(pair.elements[1]), datum_arena, core_arena);
                if (!val_r.has_value())
                    return val_r;

                core_lambda<core, MaxNodes, MaxList> lam{};
                lam.params.push_back(bname);
                lam.body = make_arena_box(core_arena, std::move(inner.value()));

                core_application<core, MaxNodes, MaxList> app{};
                app.func =
                    make_arena_box(core_arena, core{core_f{std::move(lam)}});
                app.args.push_back(
                    make_arena_box(core_arena, std::move(val_r.value())));

                inner = foundation::result<core>{core{core_f{std::move(app)}}};
            }

            return inner;
        }

        if (name == "set!") {
            if (lst.elements.size() != 3)
                return foundation::parse_error{{},
                                               "set!: expected name and value"};
            auto const &name_node = datum_arena.get(lst.elements[1]);
            if (!std::holds_alternative<reader::datum_symbol>(name_node.inner))
                return foundation::parse_error{{},
                                               "set!: name must be a symbol"};

            auto val_r = elaborate_node<MaxNodes, MaxList>(
                datum_arena.get(lst.elements[2]), datum_arena, core_arena);
            if (!val_r.has_value())
                return val_r;

            return core{core_f{core_set<core, MaxNodes>{
                std::get<reader::datum_symbol>(name_node.inner).name,
                make_arena_box(core_arena, std::move(val_r.value()))}}};
        }

        if (name == "begin") {
            if (lst.elements.size() < 2)
                return foundation::parse_error{
                    {}, "begin: expected at least one expression"};

            core_begin<core, MaxNodes, MaxList> seq{};
            for (int i = 1; i < lst.elements.size(); ++i) {
                auto expr_r = elaborate_node<MaxNodes, MaxList>(
                    datum_arena.get(lst.elements[i]), datum_arena, core_arena);
                if (!expr_r.has_value())
                    return expr_r;
                seq.exprs.push_back(
                    make_arena_box(core_arena, std::move(expr_r.value())));
            }

            return core{core_f{std::move(seq)}};
        }
    }

    // Regular application
    auto func_r =
        elaborate_node<MaxNodes, MaxList>(first, datum_arena, core_arena);
    if (!func_r.has_value())
        return func_r;

    core_application<core, MaxNodes, MaxList> app{};
    app.func = make_arena_box(core_arena, std::move(func_r.value()));

    for (int i = 1; i < lst.elements.size(); ++i) {
        auto arg_r = elaborate_node<MaxNodes, MaxList>(
            datum_arena.get(lst.elements[i]), datum_arena, core_arena);
        if (!arg_r.has_value())
            return arg_r;
        app.args.push_back(
            make_arena_box(core_arena, std::move(arg_r.value())));
    }

    return core{core_f{std::move(app)}};
}

/// Elaborates a single datum node into a core node.
///
/// Atomic datums (integer, boolean, symbol, quote) map directly.
/// List datums are dispatched to @ref elaborate_list.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list length.
/// @param  d           The datum to elaborate.
/// @param  datum_arena Read-only datum arena.
/// @param  core_arena  Core arena receiving elaborated nodes.
template <int MaxNodes, int MaxList>
constexpr auto elaborate_node(
    reader::datum_type<MaxNodes, MaxList> const &d,
    const foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>,
                                 MaxNodes> &datum_arena,
    foundation::tree_arena<core_type<MaxNodes, MaxList>, MaxNodes> &core_arena)
    -> foundation::result<core_type<MaxNodes, MaxList>> {
    using core = core_type<MaxNodes, MaxList>;
    using core_f =
        typename core_f_factory<MaxNodes, MaxList>::template type<core>;

    if (std::holds_alternative<reader::datum_integer>(d.inner)) {
        return core{core_f{
            core_integer{std::get<reader::datum_integer>(d.inner).value}}};
    }
    if (std::holds_alternative<reader::datum_boolean>(d.inner)) {
        return core{core_f{
            core_boolean{std::get<reader::datum_boolean>(d.inner).value}}};
    }
    if (std::holds_alternative<reader::datum_symbol>(d.inner)) {
        return core{
            core_f{core_symbol{std::get<reader::datum_symbol>(d.inner).name}}};
    }
    if (std::holds_alternative<reader::datum_quote<
            reader::datum_type<MaxNodes, MaxList>, MaxNodes>>(d.inner)) {
        auto const &q =
            std::get<reader::datum_quote<reader::datum_type<MaxNodes, MaxList>,
                                         MaxNodes>>(d.inner);
        auto atom_r = elaborate_quote<MaxNodes, MaxList>(
            datum_arena.get(q.quoted), datum_arena);
        if (!atom_r.has_value())
            return atom_r.error();
        return core{core_f{core_quote{atom_r.value()}}};
    }
    if (std::holds_alternative<reader::datum_list<
            reader::datum_type<MaxNodes, MaxList>, MaxNodes, MaxList>>(
            d.inner)) {
        return elaborate_list<MaxNodes, MaxList>(
            std::get<reader::datum_list<reader::datum_type<MaxNodes, MaxList>,
                                        MaxNodes, MaxList>>(d.inner),
            datum_arena, core_arena);
    }

    return foundation::parse_error{{}, "elaborator: unsupported node type"};
}

} // namespace detail

/// Elaborates a parsed datum into the core AST.
///
/// This is the public entry point for the elaboration phase.  Converts the
/// raw Scheme datum from the reader into a typed core expression, classifying
/// special forms (@c if, @c lambda, @c define, @c quote) and emitting errors
/// for malformed input.
///
/// @tparam MaxNodes Arena capacity.
/// @tparam MaxList  Maximum list/argument length.
/// @param  pd          Root datum to elaborate.
/// @param  datum_arena Read-only datum arena produced by the reader.
/// @param  core_arena  Core arena that receives elaborated nodes.
/// @return A @ref foundation::result holding the root core node, or a
///         @ref foundation::parse_error describing the first failure.
template <int MaxNodes, int MaxList>
constexpr auto elaborate(
    reader::datum_type<MaxNodes, MaxList> const &pd,
    const foundation::tree_arena<reader::datum_type<MaxNodes, MaxList>,
                                 MaxNodes> &datum_arena,
    foundation::tree_arena<core_type<MaxNodes, MaxList>, MaxNodes> &core_arena)
    -> foundation::result<core_type<MaxNodes, MaxList>> {
    auto r =
        detail::elaborate_node<MaxNodes, MaxList>(pd, datum_arena, core_arena);
    if (!r.has_value())
        return r.error();
    return r.value();
}

} // namespace smd::smdscheme::elaborator

#endif
