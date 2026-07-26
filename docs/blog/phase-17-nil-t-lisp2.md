**DRAFT &#x2014; pending author revision**

<div class="abstract" id="org76d653f">
<p>
Steps L4 through L10 built a reader and an elaborator; neither one runs anything.
Step L11 is the first <code>smdlisp</code> code that actually executes a program, and executing a program is where the semantic decisions from <code>docs/cl-pivot-plan.md</code> stop being paperwork and start being code you have to get right on the first try.
This post covers the direct evaluator: one truthiness function instead of Scheme's per-site <code>#f</code> encoding, a Lisp-2 environment with two namespaces that genuinely do not know about each other, <code>funcall</code> and <code>#'</code> as real call semantics rather than another two-argument builtin, and the closure-capture ownership question step L9 left open, finally resolved by an arena instead of an owning pointer.
Three programs run end to end at compile time by the end of this post: <code>(if nil 1 2)</code>, a lambda that walks a quoted list, and <code>(funcall #'cons 1 nil)</code>.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 16 - Reading Common Lisp: Case, Keywords, and #' ←](phase-16-reading-common-lisp.md)

</nav>


# One truthiness function

Scheme's evaluator in this project never had a single place that decided whether a value was true. It had one at every `if`: check whether the condition happens to be the boolean `#f`, and treat everything else, including `0` and the empty list, as true. That works, but it means "what counts as false" is a fact about the `if`-handling code, not a fact you can point at.

Common Lisp only has one false value, and I wanted the evaluator to say so in one place rather than imply it at every branch point. `nil` is it &#x2014; the sole false value, the empty list, and the symbol `NIL`, simultaneously, per decision D3. Everything else is true, including `0` and every keyword. There is exactly one function that gets to answer the question:

```cpp
template <typename Core>
[[nodiscard]] constexpr auto is_true(value<Core> const &v) -> bool {
    return !std::holds_alternative<nil_t>(v);
}
```

And the evaluator's only branching form calls it, and nothing else in the evaluator is allowed to reinvent it:

```cpp
[&](elaborator::core_if<Core, MaxNodes> const &cif) -> Res {
        auto cond_r =
            eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                arena.get(cif.condition), arena, environment, envs);
        if (!cond_r.has_value())
            return cond_r.error();
        if (is_true(cond_r.value()))
            return eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                arena.get(cif.consequent), arena, environment, envs);
        return eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            arena.get(cif.alternative), arena, environment, envs);
},
```

`t` needed a runtime representation, which the L10 elaborator deliberately left unpicked &#x2014; the core AST only records that the source spelled the canonical true constant, and defers what value that becomes. `pairs.hpp`'s predicates (`null`, `eq`, `eql`, `atom`) already return the symbol `T` for a true result, so making `core_true` evaluate to that exact same value, `symbol{"T"}`, was the only choice that did not invent a second notion of true a step later. `(eq t 'T)` and `(if (null nil) 1 2)` now agree with each other by construction, not by coincidence.


# A Lisp-2 that actually doesn't know about itself

Common Lisp keeps functions and variables in separate namespaces. `(f x)` looks `f` up among function bindings and `x` up among variable bindings, and a program can bind both names at once without either shadowing the other. Step L9 built the environment with two independent binding lists to support this; step L11 is where the evaluator has to actually respect the split at every lookup site, not just at the ones that are obviously about function calls.

An ordinary symbol used as an expression &#x2014; the `x` in `(+ x 1)` &#x2014; always resolves through the variable namespace. An application head, a `#'name`, or a `(function name)` always resolves through the function namespace, and the elaborator already collapsed all three spellings down to one core node (`core_function`) so the evaluator only has to get this right in one place:

```cpp
[&](elaborator::core_symbol const &cs) -> Res {
        // VARIABLE namespace only, per D4 -- never lookup_function.
        return environment.lookup_value(symbol{cs.name.view()});
},
[&](elaborator::core_keyword const &ck) -> Res {
        return Val{keyword{ck.name.view()}};
},
[&](elaborator::core_nil const &) -> Res { return Val{nil_t{}}; },
[&](elaborator::core_true const &) -> Res {
        return Val{symbol{"T"}};
},
[&](elaborator::core_function<Core, MaxNodes> const &cf) -> Res {
        return std::visit(
            smd::fixpoint::overloaded{
                [&](std::string_view name) -> Res {
                    // FUNCTION namespace only, per D4 -- this is
                    // what makes an application head, `#'name`,
                    // and `(function name)` all resolve the same
                    // way, distinctly from a VARIABLE reference to
                    // the same spelling.
                    return environment.lookup_function(symbol{name});
                },
                [&](smd::smdscheme::foundation::arena_box<
                    Core, MaxNodes> const &target) -> Res {
                    // An embedded (lambda ...): evaluating it
                    // materializes the closure directly, no
                    // lookup.
                    return eval_direct<MaxNodes, MaxList, MaxBindings,
                                       MaxEnvs>(
                        arena.get(target), arena, environment, envs);
                }},
            cf.target);
},
```

Notice what is missing: there is no fallback from `lookup_function` to `lookup_value`, or the other way, anywhere in this code. A Lisp-1 evaluator ported carelessly would have one lookup path and be tempted to special-case function position; getting a Lisp-2 right means having two lookup paths and never letting them touch. `env.test.cpp` already pinned that `f` can be a variable and a function at once without collision, back in step L9; this step is where that promise has to hold up under real evaluation, not just under a `static_assert` on binding tables.


# funcall and #' at the evaluator, not just the reader

Step L6 gave `#'f` its own reader node instead of desugaring it to `(function f)`, on the theory that the reader's job is to record what the source said and nothing more. Step L11 is where that deferred decision gets cashed in: evaluating a `core_function` node &#x2014; whether it's the head of an application, the operand of a bare `#'f` expression, or an ordinary argument to `funcall` &#x2014; is the one place `#'` acquires meaning. Evaluate it, and you get a function value, full stop, regardless of which of the three source spellings produced the node.

`funcall` and `apply` needed something a plain two-argument builtin can't give them: the ability to call an arbitrary function value with an arbitrary argument list computed at runtime. So they're builtins that recurse into the same call-dispatch helper ordinary application already uses, instead of a case in `pairs.hpp`'s primitive table:

```cpp
    [&](builtin const &bi) -> Res {
        switch (bi.op) {
        case builtin_op::add:
        case builtin_op::multiply: {
            // ANSI CL `+`/`*` are variadic, `(+)` => 0, `(*)` => 1.
            int acc = bi.op == builtin_op::add ? 0 : 1;
            for (auto const &a : args) {
                if (!std::holds_alternative<int>(a))
                    return parse_error{{}, "type error"};
                acc = bi.op == builtin_op::add ? acc + std::get<int>(a)
                                               : acc * std::get<int>(a);
            }
            return Val{acc};
        }
        case builtin_op::cons:
        case builtin_op::car:
        case builtin_op::cdr:
        case builtin_op::list:
        case builtin_op::null:
        case builtin_op::eq:
        case builtin_op::eql:
        case builtin_op::atom:
            return apply_prim<Core, default_max_pairs>(
                detail::to_list_op(bi.op), args, heap);
        case builtin_op::funcall: {
            // (funcall f args...): the first argument IS the
            // function to call; the rest are its call arguments.
            if (args.empty())
                return parse_error{
                    {}, "funcall: expected a function argument"};
            return apply_function_value<MaxNodes, MaxList, MaxBindings,
                                        MaxEnvs>(
                args[0], args.subspan(1), arena, heap, envs);
        }
        case builtin_op::apply: {
            // (apply f args... list): every argument but the
            // first (the function) and the last (a list) is
            // passed through as-is; the last argument is spread.
            if (args.size() < 2)
                return parse_error{
                    {},
                    "apply: expected a function and at least one "
                    "list argument"};
            smd::smdscheme::foundation::static_vector<Val, MaxList>
                spread;
            for (std::size_t i = 1; i + 1 < args.size(); ++i)
                spread.push_back(args[i]);
            Val cur = args[args.size() - 1];
            while (!std::holds_alternative<nil_t>(cur)) {
                if (!std::holds_alternative<pair_ref>(cur))
                    return parse_error{
                        {}, "apply: last argument must be a list"};
                if (heap == nullptr)
                    return parse_error{
                        {}, "apply: environment has no pair heap"};
                auto const &cell =
                    heap->get(std::get<pair_ref>(cur).loc);
                spread.push_back(cell.car);
                cur = cell.cdr;
            }
            return apply_function_value<MaxNodes, MaxList, MaxBindings,
                                        MaxEnvs>(
                args[0],
                std::span<Val const>(spread.begin(), spread.end()),
                arena, heap, envs);
        }
        }
        return parse_error{{}, "unknown builtin"};
    },
    [&](closure<Core> const &clo) -> Res {
        if (clo.node == nullptr)
            return parse_error{
                {}, "internal error: closure has no lambda node"};
        auto const &lam_node = *clo.node;
        if (!std::holds_alternative<
                elaborator::core_lambda<Core, MaxNodes, MaxList>>(
                lam_node.inner))
            return parse_error{
                {},
                "internal error: closure does not reference a "
                "lambda"};
        auto const &lam =
            std::get<elaborator::core_lambda<Core, MaxNodes, MaxList>>(
                lam_node.inner);
        if (static_cast<int>(args.size()) != lam.params.size())
            return parse_error{{}, "arity mismatch"};
        if (clo.captured == nullptr)
            return parse_error{
                {},
                "internal error: closure has no captured "
                "environment"};

        env<Core, MaxBindings> new_env = *clo.captured;
        for (int i = 0; i < lam.params.size(); ++i)
            new_env.define_value(symbol{lam.params[i]}, args[i]);

        // Implicit progn: at least one body expression is
        // guaranteed by the elaborator (elaborate_lambda).
        Res last{parse_error{{}, "lambda: empty body"}};
        for (int i = 0; i < lam.body.size(); ++i) {
            last = eval_direct<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                arena.get(lam.body[i]), arena, new_env, envs);
            if (!last.has_value())
                return last;
        }
        return last;
    },
    [&](foreign_function<Core> const &ff) -> Res {
        return ff.fn(args);
    },
    [](nil_t const &) -> Res {
        return parse_error{{}, "attempted to call non-function"};
    },
    [](int const &) -> Res {
        return parse_error{{}, "attempted to call non-function"};
    },
    [](symbol const &) -> Res {
        return parse_error{{}, "attempted to call non-function"};
    },
    [](keyword const &) -> Res {
        return parse_error{{}, "attempted to call non-function"};
    },
    [](pair_ref const &) -> Res {
        return parse_error{{}, "attempted to call non-function"};
    }},
func_val);
```

`apply`'s only real complication is CL's rule that the last argument is a list to be spread and every argument before it is passed through unchanged &#x2014; `(apply #'+ 1 (list 2 3))` has to walk the pair chain that `(list 2 3)` built and turn it back into three ordinary arguments, `1`, `2`, and `3`, before making the call.


# The closure-capture question, resolved

Step L9's handoff left one thing unbuilt on purpose: a real closure has to capture an environment, and `closure<Core>::captured` is a raw, non-owning pointer, because `value.hpp` can't include `env.hpp` back without a cycle, and so can't embed an owned `env` by value the way the Scheme original does. A pointer to a C++ stack local doesn't survive the call that created it. This is the same class of bug L10 hit with `core_symbol`'s `folded_name`, one layer further up the stack: whatever a returned value points at has to outlive the value, or the value is a landmine.

The fix is the same shape as `pair_heap`, which already solves an identical problem for cons cells: give every captured environment a stable home in caller-owned, fixed-capacity storage, and hand out a pointer into that storage instead of a pointer onto the call stack.

```cpp
/// Copies @p e into the arena and returns a stable, arena-owned
/// pointer to the copy.  The returned pointer remains valid for the
/// lifetime of this arena (never merely for the lifetime of the
/// call that produced @p e).
constexpr auto alloc(env<Core, MaxBindings> e)
        -> env<Core, MaxBindings> const * {
        envs_.push_back(std::move(e));
        return &envs_[envs_.size() - 1];
}
```

`smd::smdscheme::closure::env`'s own answer to this problem is a `constexpr_box` &#x2014; an owning box backed by `new` and `delete` whose destructor runs when the closure holding it is destroyed, relying on C++20's rule that transient constant-evaluation allocation just has to be freed before the enclosing evaluation finishes. That works, but only because Scheme's `closure` and `env` are defined together in one header, where `env` is complete everywhere `constexpr_box` needs it to be. Splitting `env` into its own header for the Lisp-2 split ruled that construction out, which forced the question instead of letting me duplicate the answer. An arena needs no `new`, no `delete`, and no argument about whether a transient allocation got freed in time &#x2014; it needs only that whoever is running the evaluation keeps the arena alive for as long as they keep using anything it produced, which is the same discipline the core tree's own arena already requires.

I found the exact failure mode this design exists to prevent while writing this step's own tests, not while writing the evaluator. A test helper that read, elaborated, and evaluated a bare keyword atom, then returned the resulting value out of the function that did all three, crashed under AddressSanitizer with a stack-use-after-return &#x2014; the elaborated root, held in a local variable, went out of scope while the returned `keyword` still held a view into its spelling. Same bug as L10's, one more layer up, caught the same way.


# The merge test: three programs, one evaluator

```lisp
(if nil 1 2)                          ; => 2
((lambda (x) (car (cdr x))) '(1 2 3)) ; => 2
(funcall #'cons 1 nil)                ; => (1)
```

All three run as compile-time `static_assert` checks: read the source, elaborate it, evaluate the result, and check the answer, all inside the C++ constant evaluator. The first exercises truthiness and the `if` branch. The second exercises a closure over quoted list data &#x2014; `'(1 2 3)` elaborates to hermetic `cons` cells built at compile time by the elaborator, and the lambda has to walk them through the ordinary `CAR` and `CDR` builtins, proving that hand-built and builtin-built pairs are the same kind of value. The third exercises `funcall`, `#'`, and the function namespace together: `#'cons` resolves `CONS` as a function value without calling it, and `funcall` is what actually calls it.

Everything that made this evaluator interesting was already implied by earlier steps' decisions &#x2014; `nil` as sole false value from L7, the two namespaces from L9, `#'` as its own reader node from L6. Step L11's job was mostly to stop deferring and make those decisions run.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 16 - Reading Common Lisp: Case, Keywords, and #'](phase-16-reading-common-lisp.md)

</nav>


# References
