<div class="abstract" id="org3be8218">
<p>
The lexer, elaborator, CPS pass, and DOT output all work now. What does it look like to actually call this from a C++26 program?
This phase focuses on invoking a Scheme script from C++, passing parameters, and running it.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 10 - Constexpr Pipeline ←](phase-10-constexpr.md)

</nav>


# Execution Frontends

I've kept a strict boundary between compile-time evaluation and the runtime environment. Because the compiler validates, elaborates, and lowers the abstract syntax tree entirely during `constexpr` calculation, the runtime executor takes the statically generated C++ execution pipeline and drives data through it sequentially.


## Standard Closure Evaluation

To embed the compiler in a runtime process, pre-compile a script with `scm::compiled_closure`.

```cpp
constexpr auto program =
    scm::compiled_closure<"(print-and-add current-year 10)">;
```

Here, the template string `(print-and-add current-year 10)` is fully elaborated and safely evaluated down to the lowest CPS core at compile-time.

The returned variable is a callable program object &#x2014; a `closure::closure_program` functor &#x2014; that represents the entry point to the pre-constructed AST sequence. At runtime, the caller initializes a dynamic variable environment and passes it into this entry point.

```cpp
int main() {
    auto env = scm::closure::default_env<Core, 16>();

    // Inject variables and native FFI functions into the environment
    env.define("current-year", scm::closure::value<Core>{2026});
    env.define("print-and-add",
               scm::closure::value<Core>{
                   scm::closure::foreign_function<Core>{ffi_print_and_add}});

    auto result = program(env);
```

The execution requires registering variables natively. A `current-year` variable is mapped, an FFI function is registered, and the statically checked AST is invoked via `program(env)`. No textual parsing logic occurs locally at runtime.


# FFI Abstractions

I bridge the C++ type system to the value wrappers with a `std::span` over `scm::closure::value<Core>` &#x2014; the same `value<Core>` variant type that `cps_dispatch` (Phase 12) produces and the closure backend (Phase 6) defines.

```cpp
constexpr auto
ffi_print_and_add(std::span<scm::closure::value<Core> const> args)
    -> scm::foundation::result<scm::closure::value<Core>> {
    if (args.size() != 2)
        return scm::foundation::parse_error{{}, "ffi arity mismatch"};
    if (!std::holds_alternative<int>(args[0]) ||
        !std::holds_alternative<int>(args[1]))
        return scm::foundation::parse_error{{}, "ffi type error"};

    int a = std::get<int>(args[0]);
    int b = std::get<int>(args[1]);

    std::cout << "FFI Called with: " << a << " and " << b << '\n';

    return scm::closure::value<Core>{a + b};
}
```

Native callbacks get a `std::span` into the evaluated variable pool. The callback then unpacks values with `std::holds_alternative<int>` before calling through.


# Integrating the Sender Backend

While the standard closure backend relies on standard C++ stack evaluation by wrapping tail-calls, the `beman::execution` sender backend needs a bit more setup. Here's what changes.

First, standard instantiation requires `compile_to_sender` rather than the simplified `compiled_closure` variant &#x2014; switching from the closure backend (Phase 6) to the sender backend (Phase 8).

```cpp
constexpr auto program =
    scm::sender::compile_to_sender<512, 16>(scheme_source).value();
```

Second, the returned program object is still invoked synchronously as `program(env)`, exactly like the closure backend. The difference is internal: the sender backend describes evaluation as a graph of senders and drives it to completion with `sync_wait` inside `operator()`, rather than walking the tree directly. The asynchronous machinery is hidden behind the same synchronous call interface.

```cpp
auto env = scm::closure::default_env<Core, 16>();
env.define("eq?", scm::closure::value<Core>{
                          scm::closure::foreign_function<Core>{ffi_eq}});
env.define("print-and-return",
               scm::closure::value<Core>{
                   scm::closure::foreign_function<Core>{ffi_print_and_return}});

std::println("Running pre-compiled Scheme application via Senders...");
auto result = program(env);

scm::reflection::reified_environment<RuntimeStateTag> state{};
```

The call returns a `foundation::result<closure::value<Core>>` &#x2014; the same result type the closure backend yields. The caller checks `result.has_value()` and extracts the payload with `std::get<int>(result.value())` (reporting `result.error().message` on failure); the `sync_wait` that drives the sender graph runs internally and never appears at the call site.


# Reflection-Reified Environments

The two FFI examples above require hand-writing a C++ struct to hold the Scheme program's output values. The names and types of those fields are determined by the programmer at the point they write the example. C++26 Reflection offers a more ambitious alternative: let the compiler **generate** that struct's shape from a descriptor list at `consteval` time.


## The Mechanism: `compile_environment` and `define_aggregate`

`reified_environment.hpp` provides two pieces:

```cpp

/// Describes one captured variable in a compile-time environment: its type
/// (as a reflection) and its name.
struct capture_desc {
    std::meta::info type;  ///< P2996 reflection of the variable's type.
    std::string_view name; ///< The variable name to use as a struct field name.
};

/// Target template into which @ref compile_environment injects data members.
///
/// Specialize this template (implicitly, via @ref compile_environment) to
/// produce an ordinary aggregate type whose fields correspond to captured
/// variables.  A unique @p Tag per call site prevents different injections
/// from aliasing each other.
///
/// @tparam Tag A unique tag type identifying this particular environment shape.
template <typename Tag>
struct reified_environment;

/// Injects fields into @c reified_environment<Tag> based on the supplied
/// capture descriptors.
///
/// Generates an ordinary aggregate type by calling
/// @c std::meta::define_aggregate.  The result is a plain struct with one
/// data member per entry in @p captures.  This keeps environment shapes out
/// of the evaluation runtime and avoids limitations of consteval allocation.
///
/// @tparam Tag      Unique tag type for this environment shape.
/// @param  captures Ordered list of (type, name) pairs for the injected fields.
template <typename Tag>
consteval void compile_environment(std::vector<capture_desc> captures) {
    std::vector<std::meta::info> members;
    for (auto c : captures) {
        members.push_back(
            std::meta::data_member_spec(c.type, {.name = c.name}));
    }
    std::meta::define_aggregate(^^reified_environment<Tag>, members);
}
```

`reified_environment<Tag>` starts as a declared-but-undefined struct template. `compile_environment<Tag>` calls `std::meta::define_aggregate` at `consteval` time, which **injects data members** into that template specialization. After `compile_environment<Tag>` runs, `reified_environment<Tag>` becomes a complete aggregate type whose fields correspond exactly to the supplied `(type, name)` pairs &#x2014; ordinary members, accessible by name, fully typed.

This is distinct from the read-only introspection in Phase 8. There, reflection **read** information from existing types. Here, reflection **writes** a new type into existence.


## A `consteval {}` Block

The mechanism is invoked via a C++26 standalone `consteval` block &#x2014; a block-level statement that executes unconditionally at compile time, with no enclosing function:

```cpp
consteval {
    scm::reflection::compile_environment<RuntimeStateTag>(
        {{^^int, "eval_result"}, {^^bool, "successful_run"}});
}
```

After this block executes, `reified_environment<RuntimeStateTag>` is a complete struct equivalent to:

```cpp
struct reified_environment<RuntimeStateTag> {
    int eval_result;
    bool successful_run;
};
```


## Using the Reified Struct

At runtime, the struct is used exactly like any other aggregate &#x2014; by field name, with native C++ types:

```cpp
// Populate a reified C++ aggregate structure directly from reflection
// properties
scm::reflection::reified_environment<RuntimeStateTag> state{};

if (result.has_value()) {
        state.successful_run = true;
        state.eval_result = std::get<int>(result.value());
        std::println("Final Scheme Result Output: {}", state.eval_result);
} else {
        state.successful_run = false;
        std::println(stderr, "Error evaluating Scheme: {}",
                     result.error().message);
        return 1;
}
```

`state.eval_result` and `state.successful_run` are accessed as ordinary struct members. There is no `std::any`, no type-erased variant, no runtime map lookup. The struct is a plain aggregate and can be passed by value, stored in arrays, or used in `if constexpr` branches like any other C++ type.


## What This Enables

The practical motivation is decoupling the shape of the runtime environment from the point in the code where the struct is written. In a more complete system, `compile_environment` could be driven by the Scheme program itself &#x2014; the elaborator could extract the free variables of a Scheme expression and produce the corresponding `capture_desc` list at `consteval` time, generating a precisely-shaped C++ aggregate to hold exactly the variables that Scheme program needs. That would close the loop between compile-time Scheme evaluation and statically typed C++ data, without any runtime type erasure.


# Conclusion

The compile-time Scheme compiler merges cleanly back into ordinary C++: senders establish bindings, and state moves out through explicit Sender receivers with no runtime type erasure.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [Next: Phase 12 - CPS →](phase-12-cps.md)

</nav>


# References
