**DRAFT &mdash; pending author revision**

<div class="abstract" id="orgc000e56">
<p>
Every macro up to now was a C++ function: <code>when</code>, <code>unless</code>, <code>and</code>, <code>or</code>, <code>cond</code>, <code>case</code> were entries in a table, each one a hand-written transformer from datum to datum.
<code>defmacro</code> is the step where the language starts writing its own macros.
A <code>defmacro</code>-defined <code>my-when</code> is Lisp code &mdash; a lambda &mdash; that <code>smdlisp</code> compiles and runs, during the expansion pass, with the same elaborator and the same evaluator an ordinary program goes through.
This is the compiler running the language it compiles, at compile time, and the new machinery that makes it possible is a pair of functions that carry data across the datum/value boundary in both directions.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 18 - setq, defun, progn ←](phase-18-setq-defun-progn.md)

</nav>


# A macro is a function, and now it can be a Lisp function

The host macros from the last two steps are ordinary C++ functions. Each takes a call form as a datum and returns a datum, and the expander tries them by name at every list head it walks. That is fine for the six forms the compiler ships with, but it is not what a Common Lisp programmer means by a macro. A real macro is written *in* the language: `(defmacro my-when (test &body body) ...)` defines a transformer whose body is Lisp code, and that code has to run somewhere.

The somewhere is the point of this step. The expander already reads, elaborates, and evaluates &mdash; that is the whole pipeline. So a `defmacro`'s expander is compiled by the same `elaborate` every program goes through, and run by the same `eval_direct`, except that it runs *now*, during expansion, instead of being emitted for the program's own later run. There is no second interpreter, no macro mini-language. The compiler already contains an evaluator; `defmacro` just points it at the macro body.


# Two arenas, kept apart

Running the evaluator at expansion time needs the evaluator's plumbing: a pair heap for `cons`, an environment with the builtins bound, an arena for the environments a closure captures. None of that can be the real program's plumbing &mdash; the program has not started running, and its heap and environment belong to *its* evaluation, not to the macro expander's. So the expander carries its own, in a `macro_context` threaded through the recursive walk the same way the datum arena already is:

```cpp
template <int MaxNodes, int MaxList, int MaxBindings = 16,
          int MaxEnvs = closure::default_max_envs>
class macro_context {
  public:
    using Core = elaborator::core_type<MaxNodes, MaxList>;

    smdscheme::foundation::static_vector<user_macro<MaxNodes, MaxList>,
                                         max_user_macros>
        table{}; ///< Registered `defmacro` macros, in definition order.
    smdscheme::foundation::tree_arena<Core, MaxNodes>
        core_arena{}; ///< Compile-time-only core arena for macro bodies;
                      ///< never the real program's own core arena.
    closure::pair_heap<Core, closure::default_max_pairs>
        heap{}; ///< Compile-time-only pair heap (reification and macro
                ///< body `cons`/`append` evaluation both allocate here).
    closure::env_arena<Core, MaxBindings, MaxEnvs>
        envs{}; ///< Owns every environment a macro-body lambda captures.
    closure::env<Core, MaxBindings> env{
        closure::default_env<Core, MaxBindings>(heap)}; ///< The
    ///< compile-time evaluation environment (default builtins only).
};
```

The `table` is the registry of user macros; everything else is a private, compile-time-only copy of the machinery `closure/` already builds for the real program. One `macro_context` lives for exactly one top-level form: a `defmacro` is visible to the rest of that form's expansion and no further. That is narrower than ANSI CL, where a `defmacro` is visible to every later form in the file; the pipeline is one datum in, one core tree out, with no session to hang a global macro environment on. Recorded as DIV-0010, along with the rest of this step's deliberate simplifications.


# Reification, both directions

The macro body is Lisp code that runs against Lisp *values* &mdash; `pair_ref`, `symbol`, `int`, `nil_t` &mdash; but a macro call's arguments arrive as unevaluated *data*, and its result has to go back into the datum tree to be expanded again. The value model and the datum model are different types. Crossing between them is the actual new machinery of this step, and it is a matched pair.

`datum_to_value` turns an argument datum into a value the macro's compiled closure can be bound to: an integer datum becomes an `int` value, a symbol becomes a `symbol` (with `NIL` mapped to the one true `nil_t`, exactly as quoted data is), a list becomes a chain of pairs on the context's private heap. `value_to_datum` runs the other way over the closure's return value, writing new nodes back into the datum arena so the result is indistinguishable from code the reader produced. A pair chain that does not end in `nil` is an error going back &mdash; the datum layer has no dotted list &mdash; and a stray backquote datum in argument position is an error coming in. Both restrictions match limits that already exist one layer down.


# Compiling and running the expander

With the round trip in place, `defmacro` is mechanical. Parse the lambda list, build a synthetic `(lambda (params...) body...)` out of the macro's own formals and body forms, expand *that* (so a macro body may itself use backquote or an already-defined macro), elaborate it into the context's private core arena, and evaluate it to get a closure:

```cpp
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs>
[[nodiscard]] constexpr auto
process_defmacro(datum_list<MaxNodes, MaxList> const &call,
                 datum_arena<MaxNodes, MaxList> &arena,
                 macro_context<MaxNodes, MaxList, MaxBindings, MaxEnvs> &ctx)
    -> smdscheme::foundation::result<datum<MaxNodes, MaxList>> {
    using DatumT = datum<MaxNodes, MaxList>;
    using DList = datum_list<MaxNodes, MaxList>;
    using datum_f =
        typename reader::datum_f_factory<MaxNodes,
                                         MaxList>::template type<DatumT>;

    if (call.elements.size() < 3)
        return smdscheme::foundation::parse_error{
            {},
            "defmacro: expected a name, formals, and at least one body "
            "expression"};

    auto const &name_node = arena.get(call.elements[1]);
    if (!std::holds_alternative<reader::datum_symbol>(name_node.inner))
        return smdscheme::foundation::parse_error{
            {}, "defmacro: name must be a symbol"};
    auto macro_name =
        std::get<reader::datum_symbol>(name_node.inner).name.view();

    auto const &formals_node = arena.get(call.elements[2]);
    if (!std::holds_alternative<DList>(formals_node.inner))
        return smdscheme::foundation::parse_error{
            {}, "defmacro: formals must be a list"};

    auto formals_r = parse_macro_formals<MaxNodes, MaxList>(
        std::get<DList>(formals_node.inner), arena);
    if (!formals_r.has_value())
        return formals_r.error();
    auto const &formals = formals_r.value();

    DList lambda_formals{};
    for (int i = 0; i < formals.plain_params.size(); ++i)
        lambda_formals.elements.push_back(formals.plain_params[i]);

    DList lambda_form{};
    lambda_form.elements.push_back(smdscheme::foundation::make_arena_box(
        arena, make_symbol<MaxNodes, MaxList>("LAMBDA")));
    lambda_form.elements.push_back(smdscheme::foundation::make_arena_box(
        arena, DatumT{datum_f{std::move(lambda_formals)}}));
    for (int i = 3; i < call.elements.size(); ++i)
        lambda_form.elements.push_back(call.elements[i]);

    auto lambda_datum = DatumT{datum_f{std::move(lambda_form)}};

    auto expanded_lambda_r =
        expand_datum<MaxNodes, MaxList, MaxBindings, MaxEnvs>(lambda_datum,
                                                              arena, ctx);
    if (!expanded_lambda_r.has_value())
        return expanded_lambda_r;

    auto core_r = elaborator::elaborate<MaxNodes, MaxList>(
        expanded_lambda_r.value(), arena, ctx.core_arena);
    if (!core_r.has_value())
        return core_r.error();

    if (ctx.table.size() >= max_user_macros)
        return smdscheme::foundation::parse_error{
            {}, "defmacro: too many macros registered in this expansion"};

    user_macro<MaxNodes, MaxList> um{};
    um.name = macro_name;
    um.required_count = formals.required_count;
    um.has_rest = formals.has_rest;
    um.lambda_node = core_r.value();
    ctx.table.push_back(std::move(um));
    auto &entry = ctx.table[ctx.table.size() - 1];

    auto clo_r = closure::eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
        entry.lambda_node, ctx.core_arena, ctx.env, ctx.envs);
    if (!clo_r.has_value())
        return clo_r.error();
    entry.macro_value = clo_r.value();

    return make_quote<MaxNodes, MaxList>(
        arena, make_symbol<MaxNodes, MaxList>(macro_name));
}
```

The two lines that matter are the `elaborate` and the `eval_direct` near the end. That is the milestone stated plainly: the macro's expander is compiled and run by the compiler's own front end and evaluator, during compilation. The closure is stashed in the registry, and the `defmacro` form itself &mdash; which leaves nothing for the program to run &mdash; is replaced by `(quote name)`, so it evaluates to the macro's name the way ANSI CL's `defmacro` returns it.

Then, at each call site, `invoke_user_macro` slices the raw call arguments, reifies them (the `&body` rest becomes one list value), applies the stored closure, and reifies the result back to a datum for the expander to keep working on:

```cpp
template <int MaxNodes, int MaxList, int MaxBindings, int MaxEnvs>
[[nodiscard]] constexpr auto
invoke_user_macro(user_macro<MaxNodes, MaxList> const &m,
                  datum_list<MaxNodes, MaxList> const &call,
                  datum_arena<MaxNodes, MaxList> &arena,
                  macro_context<MaxNodes, MaxList, MaxBindings, MaxEnvs> &ctx)
    -> smdscheme::foundation::result<datum<MaxNodes, MaxList>> {
    using Core = elaborator::core_type<MaxNodes, MaxList>;
    using Val = closure::value<Core>;

    int nargs = call.elements.size() - 1;
    if (m.has_rest) {
        if (nargs < m.required_count)
            return smdscheme::foundation::parse_error{
                {}, "defmacro: too few arguments in macro call"};
    } else if (nargs != m.required_count) {
        return smdscheme::foundation::parse_error{
            {}, "defmacro: wrong number of arguments in macro call"};
    }

    smdscheme::foundation::static_vector<Val, MaxList> args{};
    for (int i = 0; i < m.required_count; ++i) {
        auto v_r = detail::datum_to_value<MaxNodes, MaxList>(
            arena.get(call.elements[1 + i]), arena, ctx.heap);
        if (!v_r.has_value())
            return v_r.error();
        args.push_back(v_r.value());
    }
    if (m.has_rest) {
        Val rest_val{closure::nil_t{}};
        for (int i = call.elements.size() - 1; i >= 1 + m.required_count; --i) {
            auto v_r = detail::datum_to_value<MaxNodes, MaxList>(
                arena.get(call.elements[i]), arena, ctx.heap);
            if (!v_r.has_value())
                return v_r.error();
            rest_val = Val{closure::pair_ref{ctx.heap.alloc(
                closure::pair_cell<Core>{v_r.value(), rest_val})}};
        }
        args.push_back(rest_val);
    }

    auto result_r =
        closure::apply_function_value<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            m.macro_value, std::span<Val const>(args.begin(), args.end()),
            ctx.core_arena, &ctx.heap, ctx.envs);
    if (!result_r.has_value())
        return result_r.error();

    return detail::value_to_datum<MaxNodes, MaxList>(result_r.value(), arena,
                                                     ctx.heap);
}
```

The returned datum is not trusted as final: it goes back through the ordinary recursive expansion, so a macro that expands into `if` and `progn`, or into another macro, keeps expanding until it bottoms out in special forms.


# my-when is when

The merge test is that a `defmacro`-defined `my-when` behaves identically to the host `when`. Written the direct way, with `list` and `cons`, it is the textbook expansion:

```lisp
(progn
  (defmacro my-when (test &body body)
    (list 'if test (cons 'progn body) nil))
  (my-when t 1 2))            ; => 2, same as (when t 1 2)
```

Written the idiomatic way, with a backquote template in the body &mdash; the form every Common Lisp programmer actually writes &mdash; it is the same macro:

```lisp
(defmacro my-when (test &body body)
  `(if ,test (progn ,@body) nil))
```

Both produce `(IF T (PROGN 1 2) NIL)`, the exact shape `when` produces, and both evaluate to the same answer through the same evaluator. The backquote version is worth pausing on: the template inside the macro body is lowered to `cons~/~append` code by the backquote work from the last step, then compiled and run at expansion time, with `,test` and `,@body` filled from the reified arguments. Backquote and `defmacro` did not need to know about each other; each just does its job on the tree, and the composition is the standard macro-writing idiom falling out for free.


# The budget still bites

A macro that expands into a call to itself never reaches a fixpoint. The combined host-and-user expansion loop keeps the same discipline every earlier expansion loop had: a bounded number of steps, and running out is a diagnosed error, not a hang.

```lisp
(defmacro loopy (x) (list 'loopy x))   ; (loopy 1) -> (loopy 1) -> ...
```

Registered and then driven at a small budget, `(loopy 1)` diagnoses "expansion budget exceeded" rather than spinning. It is the same guarantee as the non-terminating *host* macro test from step L17, now covering user macros too.


# What's still deferred

The macro lambda list is required parameters plus one `&rest~/~&body` name &mdash; no `&optional`, `&key`, `&aux`, or destructuring yet. A `defmacro` is scoped to one top-level form, not the whole session. Both are DIV-0010.

And `gensym` still does not exist. The three host macros that need an uncapturable temporary still use fixed reserved names (`%OR-TEMP` and friends, DIV-0006), and `my-when` happens not to need one. This step builds the thing `gensym` was waiting for &mdash; an evaluator that runs macro-body code at compile time &mdash; but the builtin itself would have to be added down in the value model and the environment, files this lane does not own, so it waits for a step that does. Nested backquote (DIV-0008) is unchanged too: a macro body may use one level of backquote, which is what the idiom needs, but a template nested inside another template is still opaque data.

The compiler now runs the language it compiles. What it cannot yet do is hand a macro a fresh name it can be sure no one else will write.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 18 - setq, defun, progn](phase-18-setq-defun-progn.md)

</nav>


# References
