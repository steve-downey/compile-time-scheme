// src/smd/smdscheme/sender/comp_tree.hpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef SRC_SMD_SMDSCHEME_SENDER_COMP_TREE_HPP
#define SRC_SMD_SMDSCHEME_SENDER_COMP_TREE_HPP

#include <smd/fixpoint/box.hpp>
#include <smd/fixpoint/fix.hpp>
#include <smd/fixpoint/overloaded.hpp>

#include <smd/smdscheme/elaborator/elaborated_core.hpp>
#include <smd/smdscheme/foundation/arena_box.hpp>
#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/foundation/static_vector.hpp>

#include <functional>
#include <string_view>
#include <variant>

namespace smd::smdscheme::fixpoint_eval {

// 31435b33-ce4b-4e8e-ae10-abc64aa13501
/// A computation node holding a literal atom (integer, boolean, or symbol).
/// Stores atoms only — no full values or closures — to avoid circular type
/// dependencies with closure::value<Comp>.
struct comp_pure {
    std::variant<int, bool, std::string_view> atom;
};

/// A computation node that looks up a variable name in the environment.
struct comp_lookup {
    std::string_view name;
};

/// A computation node for conditional branching: condition, consequent,
/// alternative, each as a recursive child computation.
///
/// @tparam A The type of recursive children (will be instantiated as Comp).
template <typename A>
struct comp_if {
    smd::fixpoint::Box<A> cond;
    smd::fixpoint::Box<A> cons;
    smd::fixpoint::Box<A> alt;
};

/// A computation node for lambda (function) abstraction.
///
/// @tparam A        The type of recursive children.
/// @tparam MaxList  Maximum number of formal parameters.
template <typename A, int MaxList>
struct comp_lambda {
    foundation::static_vector<std::string_view, MaxList> params;
    smd::fixpoint::Box<A> body;
};

/// A computation node for function application.
///
/// @tparam A        The type of recursive children.
/// @tparam MaxList  Maximum number of arguments.
template <typename A, int MaxList>
struct comp_apply {
    smd::fixpoint::Box<A> func;
    foundation::static_vector<smd::fixpoint::Box<A>, MaxList> args;
};
// 31435b33-ce4b-4e8e-ae10-abc64aa13501 end

/// A computation node for @c set! assignment: mutates the binding for @c name
/// to the value produced by the child computation.
///
/// @tparam A The type of the recursive child.
template <typename A>
struct comp_set {
    std::string_view name;
    smd::fixpoint::Box<A> value;
};

/// A computation node for @c begin: evaluates each child in order and yields
/// the value of the last.
///
/// @tparam A        The type of recursive children.
/// @tparam MaxList  Maximum number of sequenced expressions.
template <typename A, int MaxList>
struct comp_begin {
    foundation::static_vector<smd::fixpoint::Box<A>, MaxList> exprs;
};

// 950ee4f0-367a-47bb-9335-30191f783119
/// Factory template producing the open-recursive variant layer for CompF.
/// Each structural node (comp_if, comp_lambda, comp_apply) is parameterized
/// by A, the type of recursive children. Leaf nodes (comp_pure, comp_lookup)
/// have no children and do not use A.
///
/// @tparam MaxList  Maximum list/argument length.
template <int MaxList>
struct comp_f_factory {
    /// One variant layer; @p A is the recursive self-reference.
    template <typename A>
    using type = std::variant<comp_pure, comp_lookup, comp_if<A>,
                              comp_lambda<A, MaxList>, comp_apply<A, MaxList>,
                              comp_set<A>, comp_begin<A, MaxList>>;
};

/// The concrete recursive computation tree type.
/// Fix<F> ties the knot: Comp = F<Comp> where F = comp_f_factory<MaxList>.
/// Children are stored as Box<Comp> (constexpr owning pointer) rather than
/// arena handles.
///
/// @tparam MaxList  Maximum list/argument length.
template <int MaxList>
using Comp = smd::fixpoint::Fix<comp_f_factory<MaxList>::template type>;
// 950ee4f0-367a-47bb-9335-30191f783119 end

/// Maps a function over the children of a CompF layer.
/// This is the functor fmap operation for CompF, enabling fold_fix and
/// refold operations over Comp trees in the future.
///
/// @tparam MaxList  The maximum argument length (embedded in the functor type).
/// @tparam F        The mapping function (A → B for some A, B types).
/// @param  f        The function to apply to each recursive child.
/// @param  layer    The CompF layer to map over.
/// @return The mapped layer, with children type transformed from A to B.
// 731f3ab0-d9ea-4254-aab4-8b38a30bb28e
template <int MaxList, typename F, typename A>
constexpr auto
fmap_comp(F &&f,
          typename comp_f_factory<MaxList>::template type<A> const &layer) {
    using B = std::invoke_result_t<F, A const &>;
    using ResultF = typename comp_f_factory<MaxList>::template type<B>;

    return std::visit(
        smd::fixpoint::overloaded{
            [](comp_pure const &p) -> ResultF { return p; },
            [](comp_lookup const &l) -> ResultF { return l; },
            [&f](comp_if<A> const &ci) -> ResultF {
                return comp_if<B>{smd::fixpoint::make_box<B>(f(*ci.cond)),
                                  smd::fixpoint::make_box<B>(f(*ci.cons)),
                                  smd::fixpoint::make_box<B>(f(*ci.alt))};
            },
            [&f](comp_lambda<A, MaxList> const &lam) -> ResultF {
                return comp_lambda<B, MaxList>{
                    lam.params, smd::fixpoint::make_box<B>(f(*lam.body))};
            },
            [&f](comp_apply<A, MaxList> const &app) -> ResultF {
                foundation::static_vector<smd::fixpoint::Box<B>, MaxList>
                    args_result;
                for (auto const &arg : app.args) {
                    args_result.push_back(
                        smd::fixpoint::make_box<B>(std::invoke(f, *arg)));
                }
                return comp_apply<B, MaxList>{
                    smd::fixpoint::make_box<B>(std::invoke(f, *app.func)),
                    std::move(args_result)};
            },
            [&f](comp_set<A> const &s) -> ResultF {
                return comp_set<B>{s.name, smd::fixpoint::make_box<B>(
                                               std::invoke(f, *s.value))};
            },
            [&f](comp_begin<A, MaxList> const &b) -> ResultF {
                foundation::static_vector<smd::fixpoint::Box<B>, MaxList>
                    exprs_result;
                for (auto const &e : b.exprs) {
                    exprs_result.push_back(
                        smd::fixpoint::make_box<B>(std::invoke(f, *e)));
                }
                return comp_begin<B, MaxList>{std::move(exprs_result)};
            }},
        layer);
}
// 731f3ab0-d9ea-4254-aab4-8b38a30bb28e end

/// Converts an elaborated core AST (arena-based, using arena_box handles)
/// into a computation tree (heap-pointer-based, using Box indirection).
///
/// This is conceptually an anamorphism (unfold), but uses manual recursion
/// to support error propagation when encountering invalid nodes like
/// core_define in expression context.
///
/// @tparam MaxNodes Arena capacity of the source core tree.
/// @tparam MaxList  Maximum list/argument length.
/// @param  node     The core node to convert.
/// @param  arena    The arena containing all core sub-nodes.
/// @return A Comp tree on success, or a parse_error on invalid input.
template <int MaxNodes, int MaxList>
[[nodiscard]] constexpr auto
core_to_comp(elaborator::core_type<MaxNodes, MaxList> const &node,
             foundation::tree_arena<elaborator::core_type<MaxNodes, MaxList>,
                                    MaxNodes> const &arena)
    -> foundation::result<Comp<MaxList>> {

    using Core = elaborator::core_type<MaxNodes, MaxList>;
    using CompT = Comp<MaxList>;
    using CompLayer = comp_f_factory<MaxList>::template type<CompT>;
    using Res = foundation::result<CompT>;

    return std::visit(
        smd::fixpoint::overloaded{
            [](elaborator::core_integer const &ci) -> Res {
                return smd::fixpoint::wrap_fix<
                    comp_f_factory<MaxList>::template type>(
                    CompLayer{comp_pure{{ci.value}}});
            },
            [](elaborator::core_boolean const &cb) -> Res {
                return smd::fixpoint::wrap_fix<
                    comp_f_factory<MaxList>::template type>(
                    CompLayer{comp_pure{{cb.value}}});
            },
            [](elaborator::core_symbol const &cs) -> Res {
                return smd::fixpoint::wrap_fix<
                    comp_f_factory<MaxList>::template type>(
                    CompLayer{comp_lookup{cs.name}});
            },
            [](elaborator::core_quote const &cq) -> Res {
                return smd::fixpoint::wrap_fix<
                    comp_f_factory<MaxList>::template type>(
                    CompLayer{comp_pure{cq.atom}});
            },
            [&arena](elaborator::core_if<Core, MaxNodes> const &cif) -> Res {
                auto cond_r = core_to_comp<MaxNodes, MaxList>(
                    arena.get(cif.condition), arena);
                if (!cond_r.has_value())
                    return cond_r.error();

                auto cons_r = core_to_comp<MaxNodes, MaxList>(
                    arena.get(cif.consequent), arena);
                if (!cons_r.has_value())
                    return cons_r.error();

                auto alt_r = core_to_comp<MaxNodes, MaxList>(
                    arena.get(cif.alternative), arena);
                if (!alt_r.has_value())
                    return alt_r.error();

                return smd::fixpoint::wrap_fix<
                    comp_f_factory<MaxList>::template type>(
                    CompLayer{comp_if<CompT>{
                        smd::fixpoint::make_box<CompT>(cond_r.value()),
                        smd::fixpoint::make_box<CompT>(cons_r.value()),
                        smd::fixpoint::make_box<CompT>(alt_r.value())}});
            },
            [&arena](
                elaborator::core_lambda<Core, MaxNodes, MaxList> const &lam)
                -> Res {
                auto body_r =
                    core_to_comp<MaxNodes, MaxList>(arena.get(lam.body), arena);
                if (!body_r.has_value())
                    return body_r.error();

                return smd::fixpoint::wrap_fix<
                    comp_f_factory<MaxList>::template type>(
                    CompLayer{comp_lambda<CompT, MaxList>{
                        lam.params,
                        smd::fixpoint::make_box<CompT>(body_r.value())}});
            },
            [&arena](elaborator::core_application<Core, MaxNodes, MaxList> const
                         &app) -> Res {
                auto func_r =
                    core_to_comp<MaxNodes, MaxList>(arena.get(app.func), arena);
                if (!func_r.has_value())
                    return func_r.error();

                foundation::static_vector<smd::fixpoint::Box<CompT>, MaxList>
                    arg_boxes;
                for (auto const &arg_box : app.args) {
                    auto arg_r = core_to_comp<MaxNodes, MaxList>(
                        arena.get(arg_box), arena);
                    if (!arg_r.has_value())
                        return arg_r.error();
                    arg_boxes.push_back(
                        smd::fixpoint::make_box<CompT>(arg_r.value()));
                }

                return smd::fixpoint::wrap_fix<
                    comp_f_factory<MaxList>::template type>(
                    CompLayer{comp_apply<CompT, MaxList>{
                        smd::fixpoint::make_box<CompT>(func_r.value()),
                        std::move(arg_boxes)}});
            },
            [](elaborator::core_define<Core, MaxNodes> const &) -> Res {
                return foundation::parse_error{
                    {},
                    "comp_tree: define not supported in expression context"};
            },
            [&arena](elaborator::core_set<Core, MaxNodes> const &cs) -> Res {
                auto val_r =
                    core_to_comp<MaxNodes, MaxList>(arena.get(cs.value), arena);
                if (!val_r.has_value())
                    return val_r.error();

                return smd::fixpoint::wrap_fix<
                    comp_f_factory<MaxList>::template type>(CompLayer{
                    comp_set<CompT>{cs.name, smd::fixpoint::make_box<CompT>(
                                                 val_r.value())}});
            },
            [&arena](elaborator::core_begin<Core, MaxNodes, MaxList> const &cb)
                -> Res {
                foundation::static_vector<smd::fixpoint::Box<CompT>, MaxList>
                    expr_boxes;
                for (auto const &expr_box : cb.exprs) {
                    auto expr_r = core_to_comp<MaxNodes, MaxList>(
                        arena.get(expr_box), arena);
                    if (!expr_r.has_value())
                        return expr_r.error();
                    expr_boxes.push_back(
                        smd::fixpoint::make_box<CompT>(expr_r.value()));
                }

                return smd::fixpoint::wrap_fix<
                    comp_f_factory<MaxList>::template type>(CompLayer{
                    comp_begin<CompT, MaxList>{std::move(expr_boxes)}});
            }},
        node.inner);
}

} // namespace smd::smdscheme::fixpoint_eval

#endif
