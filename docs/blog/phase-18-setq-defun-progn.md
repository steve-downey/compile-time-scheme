**DRAFT &#x2014; pending author revision**

<div class="abstract" id="orgaac793e">
<p>
The direct evaluator (step L11) proved the Common Lisp semantics ran.
The CPS/closure backend (step L13) has to prove they run <i>twice</i> &#x2014; once as a tree-walking interpreter, once as continuation-passing code &#x2014; and agree.
Most of that was a port: read the Scheme original's <code>cps_dispatch</code>, add four cases for the four kinds <code>eval_direct</code> already knew, done.
<code>setq</code> and <code>defun</code> were not a port.
The frozen Scheme CPS backend I was adapting from does not support <code>define</code> in expression position at all &#x2014; it is a diagnosed error, not a missing feature I overlooked.
So the reserve-then-patch trick that makes a recursive <code>defun</code> find itself had to be worked out again, this time for code built out of continuations instead of C++ call frames.
It came out the same shape as the direct evaluator's, which is either a coincidence or a sign the shape was right the first time.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 17 - nil, t, and Living in a Lisp-2 ←](phase-17-nil-t-lisp2.md)

</nav>


# What a CPS backend is for here

`eval_direct` is a tree-walking interpreter: it recurses, and C++'s own call stack is the continuation. That is fine until you need to talk about the rest of the computation as a value &#x2014; capture it, save it, invoke it later, skip over frames on the way out. Nothing in step L13 needs that yet. `block` / `return-from` (step L14) will, and that is the entire reason this backend exists before it is strictly necessary: get the plumbing in place now, on the four forms that don't need an escape, so the escape machinery has somewhere real to attach.

So `cps_code` wraps a callable that takes an environment, the closure-capture arena, and a continuation, and every node in the core AST is compiled to one of these instead of evaluated directly:

```cpp
template <class F>
struct cps_code {
    F f;

    /// Evaluates the CPS expression under @p environment (mutable) and
    /// @p envs (the shared closure-capture arena), invoking continuation
    /// @p k.
    template <class Env, class Envs, class K>
    constexpr auto operator()(Env &environment, Envs &envs, K k) const {
        return f(environment, envs, k);
    }
};
```

The environment parameter is mutable, and that is already a departure from the Scheme original, which takes `Env const&` throughout. Scheme's `set!` only ever mutates a value *through* a shared store cell, so the environment object itself never needs to change shape. `defun` is different: it adds a brand-new binding to the function namespace, which means the environment object itself grows. A `const&` can't do that, so the mutable reference is not a style preference, it is what makes the rest of this post possible.


# progn compiles to continuation chaining

The plan's own phrase for this, and it turned out to be exactly right: every expression in a `progn` but the last is evaluated for effect only, and the last one hands the whole rest of the outer computation &#x2014; the caller's own continuation &#x2014; straight through instead of returning up through this function.

```cpp
[&](elaborator::core_progn<Core, MaxNodes, MaxList> const &cp)
        -> Res {
        // "progn compiles to continuation chaining": every
        // expression but the last is evaluated for effect only
        // (identity/identity); the last tail-chains into the
        // CALLER's own (cont, k) rather than returning up through
        // this function -- the structure L14 (block/return-from)
        // hooks into.
        int const n = cp.exprs.size();
        for (int i = 0; i < n - 1; ++i) {
            auto r = cps_dispatch<MaxNodes, MaxList>(
                arena.get(cp.exprs[i]), arena, identity_k<Core>{},
                environment, envs, identity_k<Core>{});
            if (!r.has_value())
                return r;
        }
        return cps_dispatch<MaxNodes, MaxList>(
            arena.get(cp.exprs[n - 1]), arena, cont, environment, envs,
            k);
},
```

A closure's body is an implicit `progn` too (step L10 made that native to the node shape, no wrapping required), so calling a closure walks its body exactly the same way, tail-chaining the last expression into whatever continuation the call itself was given. This is the hook L14 needs: once `return-from` exists, unwinding out of an inner `progn` to an outer `block`'s continuation is the same mechanism, just aimed somewhere other than straight back out.


# setq and defun: not a port

Here is what I actually found when I went to read the Scheme original before writing this, per the plan's own instruction to read it first:

```cpp
[&](elaborator::core_define<Core, MaxNodes> const &) -> Res {
    return Res{foundation::parse_error{
        {},
        "cps_dispatch: define not supported in expression "
        "context"}};
},
```

That is the whole case for `define` in the frozen `smdscheme` CPS backend. It compiles. It runs. It refuses to do the one thing a top-level function definition needs to do. "Adapted from the Scheme CPS backend" was the instruction for step L13, and for `setq` / `defun` / `defvar` / `defparameter` there was nothing to adapt &#x2014; the pattern I was told to follow does not have an opinion on this, because it never had to.

So the design question was real, not a formality. The direct evaluator's answer, from step L12, is a reserve-then-patch dance: install a placeholder binding for the function's name, capture the environment (which now has a binding for the name, just pointing at a placeholder), build the real closure, then overwrite the placeholder in the shared store. Every copy of the environment that shares that store, including the one the closure itself just captured, sees the patched value on its next lookup. That is exactly how a self-recursive `defun` finds its own name inside its own body without a parent-environment chain to walk.

The CPS version does the identical dance, at the identical granularity, from inside continuation-passing code instead of a plain function body:

```cpp
[&](elaborator::core_setq<Core, MaxNodes> const &cs) -> Res {
        auto val_r = cps_dispatch<MaxNodes, MaxList>(
            arena.get(cs.value), arena, identity_k<Core>{}, environment,
            envs, identity_k<Core>{});
        if (!val_r.has_value())
            return val_r;
        auto assign_r =
            environment.set_value(symbol{cs.name}, val_r.value());
        if (!assign_r.has_value())
            return assign_r;
        auto r = cont(assign_r.value());
        if (!r.has_value())
            return r;
        return k(r.value());
},
[&](elaborator::core_defun<Core, MaxNodes> const &cd) -> Res {
        auto const &lam_node = arena.get(cd.lambda);
        int loc =
            environment.define_function(symbol{cd.name}, Val{nil_t{}});
        auto const *captured = envs.alloc(environment);
        Val fn_val{closure<Core>{&lam_node, captured}};
        environment.patch_function(symbol{cd.name}, loc, fn_val);
        // ANSI CL: defun returns the function name.
        Val name_val{symbol{cd.name}};
        auto r = cont(name_val);
        if (!r.has_value())
            return r;
        return k(r.value());
},
```

`setq` is simpler and closer kin to Scheme's `set!` &#x2014; it mutates a store cell, not the environment object, so continuation-passing style doesn't change anything about how it has to work, only where the mutation happens to sit in the code. `defun` is the one that actually tests whether the CPS backend's mutable-environment threading was the right call.


# A closure that finds itself, through continuations

Building the closure with a patched-in self-reference is only half the story; the other half is that calling the closure has to look the name up through the same shared store, not through whatever binding happened to be captured at closure-creation time. That happens in `cps_apply`, the CPS counterpart of `apply_function_value` &#x2014; the single place every call, whether from an ordinary application or from `funcall` / `apply`, ends up:

```cpp
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

        // Implicit progn, continuation-chained: every body
        // expression but the last is evaluated for effect only;
        // the last tail-chains into the caller's own (cont, k).
        int const n = lam.body.size();
        for (int i = 0; i < n - 1; ++i) {
            auto r = cps_dispatch<MaxNodes, MaxList>(
                arena.get(lam.body[i]), arena, identity_k<Core>{},
                new_env, envs, identity_k<Core>{});
            if (!r.has_value())
                return r;
        }
        return cps_dispatch<MaxNodes, MaxList>(
            arena.get(lam.body[n - 1]), arena, cont, new_env, envs, k);
},
```

Notice the body walk here is the same shape as `progn`'s: every expression but the last evaluated for effect, the last one tail-chained through the caller's own continuation. A called closure's implicit `progn` and an ordinary `progn` are the same kind of sequence at the core-node level, so they compile the same way &#x2014; I did not write this twice, I wrote it once and called it from two places.

```lisp
(progn
  (defun fact (n) (if (eq n 0) 1 (* n (fact (+ n -1)))))
  (fact 5))                            ; => 120, through cps_dispatch/cps_apply
```

Five recursive calls, one shared store, one patched binding, all of it running as continuation-passing code at compile time.


# The merge test, and a lifetime bug it found

The plan's own merge criterion for this step is broad on purpose: every end-to-end scenario step L11 and step L12 already proved through `eval_direct` has to also run through `compile_to_closure`, not just one new narrow test standing in for the rest. So `closure_program.test.cpp` re-runs all of it &#x2014; truthiness, the Lisp-2 lookup split, `funcall` / `apply`, and now `setq` / `defun` / `defvar` / `defparameter` &#x2014; through the compiled entry point instead of the tree-walker.

Writing that test file surfaced a lifetime bug in `compile_to_closure` itself, the same class of bug L10 and L11 each hit one layer up: something a returned value points at has to actually outlive the value. This time it was the datum arena. `smdlisp` folds symbols to uppercase at read time (decision D2), which means a symbol's spelling is owned storage sitting inside the datum tree, not a view into the original source text the way Scheme's reader does it. A lambda's parameter names are held as `string_view` values into that owned storage, and they get read every time the compiled program is later called, not just while it's being elaborated. Building the datum arena as a local inside `compile_to_closure`, the way the Scheme original does, meant that storage was gone by the time anyone tried to call the result. GCC's own constexpr evaluator caught it, flatly, before this was ever run: accessing the arena outside its lifetime, during `make compile`, not as a test failure discovered later. The fix was to make the caller own the datum arena, the same discipline every other arena in this project already follows, and file it as a permanent divergence from the Scheme original's single-argument entry point &#x2014; there wasn't a way to keep that shape and keep the case-folded spellings honest at the same time.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 17 - nil, t, and Living in a Lisp-2](phase-17-nil-t-lisp2.md)

</nav>


# References
