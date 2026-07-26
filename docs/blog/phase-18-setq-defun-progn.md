**DRAFT &#x2014; pending author revision**

<div class="abstract" id="org2a7139d">
<p>
Step L12 gave <code>smdlisp</code> a way to mutate a variable, define a function, and declare a special one &#x2014; <code>setq</code>, <code>defun</code>, <code>defvar</code>, <code>defparameter</code> &#x2014; on the direct evaluator, by adapting the store from Phase 14 almost unchanged.
Step L13, which this post actually documents (the docs lag the code by one step, as usual), rebuilt the same fifteen core forms as a continuation-passing evaluator and proved the two agree on every program that matters.
Along the way, building a one-argument <code>compile_to_closure</code> &#x2014; the same shape the Scheme backend already has &#x2014; turned up the same dangling-pointer knife-edge Phase 14 warned about, one layer further up the stack.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 17 - nil, t, and Living in a Lisp-2 ←](phase-17-nil-t-lisp2.md)

</nav>


# setq returns a value; defun returns a name

Scheme's `set!` has nothing useful to hand back. There is no value to return, so the Scheme evaluator invented one: an `unspecified` alternative, added to the value variant in Phase 14 for exactly this purpose. ANSI Common Lisp disagrees with that design on both counts it touches.

`setq` returns the value it just assigned, and it takes any number of name/value pairs, assigned left to right, yielding the value of the last one. There is no `unspecified` kind anywhere in `smdlisp`'s value variant &#x2014; adding one only to satisfy a form that does not need it would have been inventing a problem to match the old solution. `defun`, `defvar`, and `defparameter` go the other way: all three return the **name** they just bound, never the value. `(defun f (x) x)` evaluates to the symbol `F`, not to a closure.

Both rules come straight from the spec, and both meant the store machinery from Phase 14 &#x2014; built for a language where mutation has no interesting return value &#x2014; had to be adapted, not reused as-is.


# The store, again

The mechanism underneath `setq` is the one Phase 14 already built: a flat array of mutable cells addressed by a stable integer, shared by pointer across every copy of the environment. `smdlisp`'s Lisp-2 environment (Phase 17) only ever needs this for the **variable** namespace &#x2014; redefining a function is already ordinary shadowing, no store required &#x2014; so only `values_` gets the store-backed treatment:

```cpp
template <typename Core, int MaxBindings>
constexpr auto env<Core, MaxBindings>::set_value(symbol name,
                                                 value<Core> val) const
    -> smd::smdscheme::foundation::result<value<Core>> {
    if (store_ == nullptr)
        return smd::smdscheme::foundation::parse_error{
            {}, "setq: environment has no mutable store"};
    for (int i = values_.size() - 1; i >= 0; --i) {
        if (values_[i].name == name) {
            store_->set(values_[i].loc, val);
            return val;
        }
    }
    return smd::smdscheme::foundation::parse_error{{},
                                                   "setq: unbound variable"};
}
```

`set_value` is `const`, same as Phase 14's `assign`: it never touches the environment's own binding list, only the cell the store pointer names, so an evaluator holding an environment by reference can still mutate through it. An unbound name is a diagnosed error, not an implicit definition &#x2014; ANSI CL's rule, and the same one Scheme already enforced.


# Everything routes through one mutable reference

The store answers "how does a write survive a copy of the environment." It does not answer a second, related question: how does `(progn (defun f (x) x) (f 1))` see the `defun` from inside the same `progn`?

The answer this step settled on is that the environment is threaded as a mutable reference through the whole evaluation, not copied at each step. `defun` mutates it in place; every later sibling in the same `progn`, or the same lambda body, evaluates against that same object and sees the mutation. Nothing here is specific to the direct evaluator &#x2014; it has to hold under CPS too, since `core_progn`'s continuation-passing form evaluates each expression but the last purely for effect, discards the value, and only tail-passes the continuation on the final one:

```cpp
[&](elaborator::core_progn<Core, MaxNodes, MaxList> const &cp)
        -> Res {
        int const n = cp.exprs.size();
        if (n == 0)
            return parse_error{{}, "progn: empty"};
        for (int i = 0; i < n - 1; ++i) {
            auto r =
                cps_dispatch<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                    arena.get(cp.exprs[i]), arena, environment, envs,
                    identity_k<Core>{}, identity_k<Core>{});
            if (!r.has_value())
                return r;
        }
        return cps_dispatch<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
            arena.get(cp.exprs[n - 1]), arena, environment, envs, cont,
            k);
},
```

This is exactly Scheme's `begin` from Phase 14, ported over unchanged in shape. The environment argument to every recursive `cps_dispatch` call in that loop is the same reference, never a fresh copy, which is the entire reason a `defun` on line one of a `progn` is visible on line two regardless of which evaluator is running the program.


# setq under continuation-passing style

The direct evaluator can just `return last` after its assignment loop. A continuation-passing evaluator cannot &#x2013; there is no `return` that means anything outside the current continuation, so the assigned value has to be handed to `cont` and `k` like every other node's result:

```cpp
[&](elaborator::core_setq<Core, MaxNodes, MaxList> const &sq)
        -> Res {
        // ANSI CL: assign each name/value pair left to right; the
        // *continuation* receives the value of the LAST assignment
        // -- never Scheme's `unspecified` (see eval_direct.hpp's
        // identical doc note on the return-type rationale).
        Res last{parse_error{{}, "setq: no assignments"}};
        for (int i = 0; i < sq.names.size(); ++i) {
            auto val_r =
                cps_dispatch<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                    arena.get(sq.exprs[i]), arena, environment, envs,
                    identity_k<Core>{}, identity_k<Core>{});
            if (!val_r.has_value())
                return val_r;
            auto set_r = environment.set_value(symbol{sq.names[i]},
                                               val_r.value());
            if (!set_r.has_value())
                return set_r;
            last = set_r;
        }
        if (!last.has_value())
            return last;
        auto r = cont(last.value());
        if (!r.has_value())
            return r;
        return k(r.value());
},
```

`defun` reuses the ordinary lambda lowering for its embedded `(lambda (params...) body...)` rather than duplicating it, and only adds the function-namespace side effect and the name-as-return-value on top:

```cpp
[&](elaborator::core_defun<Core, MaxNodes> const &cd) -> Res {
        // Reuses the lambda CPS lowering directly -- `defun`'s
        // embedded lambda node is a real core_lambda, evaluated the
        // same way an ordinary `(lambda ...)` expression is (see
        // the core_lambda case above); only the function-namespace
        // definition side effect and the name-as-return-value are
        // added here.
        auto clo_r =
            cps_dispatch<MaxNodes, MaxList, MaxBindings, MaxEnvs>(
                arena.get(cd.lambda_node), arena, environment, envs,
                identity_k<Core>{}, identity_k<Core>{});
        if (!clo_r.has_value())
            return clo_r;
        environment.define_function(symbol{cd.name}, clo_r.value());
        // ANSI CL: `defun` returns the function name, not the
        // closure value.
        auto r = cont(Val{symbol{cd.name}});
        if (!r.has_value())
            return r;
        return k(r.value());
},
```

`defvar` and `defparameter` share one node, distinguished by a flag: `defvar` initializes only if the name is not already bound, `defparameter` always does. Both mark the name special &#x2014; the mark is recorded now and does nothing yet, since dynamic-binding behavior for special variables is a later step's problem:

```cpp
[&](elaborator::core_defvar<Core, MaxNodes> const &dv) -> Res {
        environment.mark_special(symbol{dv.name});
        if (dv.has_init) {
            bool const already_bound =
                environment.lookup_value(symbol{dv.name}).has_value();
            // `defparameter` always (re)initializes; `defvar` only
            // if `name` is not already bound.
            if (dv.is_parameter || !already_bound) {
                auto init_r = cps_dispatch<MaxNodes, MaxList,
                                           MaxBindings, MaxEnvs>(
                    arena.get(dv.init), arena, environment, envs,
                    identity_k<Core>{}, identity_k<Core>{});
                if (!init_r.has_value())
                    return init_r;
                environment.define_value(symbol{dv.name},
                                         init_r.value());
            }
        }
        // ANSI CL: `defvar`/`defparameter` return the variable
        // name.
        auto r = cont(Val{symbol{dv.name}});
        if (!r.has_value())
            return r;
        return k(r.value());
}},
```


# The merge test

Four programs, run through both the direct evaluator and this step's CPS backend, with the same answer from each:

```lisp
(if nil 1 2)                          ; => 2
((lambda (x) (car (cdr x))) '(1 2 3)) ; => 2
(funcall #'cons 1 nil)                ; => (1)
(progn (defun twice (x) (+ x x)) (twice 4)) ; => 8
```

The first three carry over from Phase 17's evaluator and mostly check that CPS did not break anything already working. The fourth is the one that actually exercises this step's new material: a `defun` and a call to the function it just defined, in the same `progn`, under an evaluator that never ~return~s in the ordinary sense.


# The lifetime knife-edge, one layer up

Phase 14 ended with a warning about a pointer that only survives one evaluation, and said the day something tried to persist a closure past its own run, the design would need to change. This step did not touch closures, but it hit the same class of bug from a different direction, and it hit it at compile time instead of under AddressSanitizer.

The Scheme backend's `compile_to_closure` takes one argument, a source string, and hands back a fully self-contained, callable value. I wanted the same thing here, and wrote it that way first: read, elaborate, and compile CPS, with the datum arena a local variable discarded on return. GCC's constexpr evaluator refused to build the test that actually uses a lambda parameter, pointing straight at that local: *accessing 'arena\_dr' outside its lifetime*.

The core arena is fine to return by value &#x2014; it is index-addressed, not pointer-based, so copying it just copies integers that still mean the same thing afterward. The problem is that `smdlisp`'s elaborated core nodes &#x2013; a lambda's parameter names, a `setq`'s targets, a `defun`'s or `defvar`'s name &#x2013; hold plain `string_view~s into the *datum* arena's storage, not owned copies. Discard the datum arena and every one of those views points at nothing. It is the identical bug Phase 17 hit for a bare root symbol, one layer further from the root: that fix made ~core_symbol` and `core_keyword` own their spelling; this one still doesn't reach a name that is merely a list element rather than the whole program.

The fix was not to chase the bug into the elaborator &#x2013; out of scope for this step &#x2013; but to stop pretending the datum arena can be discarded at all. `compile_to_closure` now takes it as a caller-owned reference, the same discipline the pair heap, the store, and the closure-capture arena already use everywhere else in `smdlisp`. Recorded as DIV-0007. It is a worse API than the Scheme original's, and it is correct.


# What's still deferred

`defvar`'s special mark does nothing observable yet; step L16 owes it real dynamic-binding semantics. And the CPS backend, like the direct evaluator before it, still has no story for a program that outlives the run that compiled it &#x2013; the knife-edge is marked, not resolved, for the second time in this series.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 17 - nil, t, and Living in a Lisp-2](phase-17-nil-t-lisp2.md)

</nav>


# References
