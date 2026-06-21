<div class="abstract" id="org9aa6cc0">
<p>
After journeying through the reader, elaborating core forms, building fixpoint trees, and wiring Mendler evaluation, the question remains: what does it look like to use these structures inside a practical C++26 program?
This phase focuses on invoking a Scheme script from C++, passing parameters, and interacting with the physical execution environment.
</p>

</div>


# Execution Frontends

Throughout this project, the implementation has enforced a strict boundary between compile-time evaluation and runtime environment mapping. Because the compiler validates, elaborates, and lowers the abstract syntax tree entirely during `constexpr` calculation, the runtime executor takes the statically generated C++ execution pipeline and drives data through it sequentially.


## Standard Closure Evaluation

The standard methodology for embedding the Scheme compiler into a runtime process involves pre-compiling a script via ~scm::compiled\_closure~—the closure backend from Phase 6 wrapped in a convenient variable template.

```cpp
constexpr auto program =
    scm::compiled_closure<"(print-and-add current-year 10)">;
```

Here, the template string `(print-and-add current-year 10)` is fully elaborated and evaluated through the Mendler interpreter at compile-time.

The returned variable is a physical C++ generic lambda that represents the entry point to the pre-constructed AST sequence. At runtime, the caller initializes a dynamic variable environment and passes it into this entry point.

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

To bridge the C++ type system with the generic value wrappers utilized internally, the architecture utilizes a \`std::span\` structure over dynamically generated \`scm::closure::value<Core>\` abstractions—the same `value<Core>` variant type that the closure backend (Phase 6) defines.

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

The execution mechanism ensures that native C++ callbacks receive an accurate memory window (`std::span`) into the evaluated variable pool. Values are subsequently unpacked utilizing standard native C++ variants (`std::holds_alternative<int>`) before native invocation proceeds.


# Integrating the Sender Backend

While the standard closure backend relies on standard C++ stack evaluation by wrapping tail-calls, the `beman::execution` Sender backend introduces a more sophisticated integration state. By observing how the sender graph evaluates Fibonacci equations, we can look at the requisite changes.

First, standard instantiation requires `compile_to_sender` rather than the simplified `compiled_closure` variant—switching from the closure backend (Phase 6) to the sender backend (Phase 8).

```cpp
constexpr auto program =
    scm::sender::compile_to_sender<512, 16>(scheme_source).value();
```

Second, the returned functional interface cannot be executed natively as an immediate synchronous call. Senders define workflows, and they mandate evaluation through a physical receiver or a scheduler structure context.

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

Extraction happens by invoking `scm::sender_v::sync_wait(std::move(s))`, which initializes propagation across the pipeline, asynchronously awaiting resolution safely before unwrapping local evaluation values locally into `res_opt`.


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

`reified_environment<Tag>` starts as a declared-but-undefined struct template. `compile_environment<Tag>` calls `std::meta::define_aggregate` at `consteval` time, which **injects data members** into that template specialization. After `compile_environment<Tag>` runs, `reified_environment<Tag>` becomes a complete aggregate type whose fields correspond exactly to the supplied `(type, name)` pairs — ordinary members, accessible by name, fully typed.

This is distinct from the read-only introspection in Phase 9. There, reflection **read** information from existing types. Here, reflection **writes** a new type into existence.


## A `consteval {}` Block

The mechanism is invoked via a C++26 standalone `consteval` block — a block-level statement that executes unconditionally at compile time, with no enclosing function:

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

At runtime, the struct is used exactly like any other aggregate — by field name, with native C++ types:

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

The practical motivation is decoupling the shape of the runtime environment from the point in the code where the struct is written. In a more complete system, `compile_environment` could be driven by the Scheme program itself — the elaborator could extract the free variables of a Scheme expression and produce the corresponding `capture_desc` list at `consteval` time, generating a precisely-shaped C++ aggregate to hold exactly the variables that Scheme program needs. That would close the loop between compile-time Scheme evaluation and statically typed C++ data, without any runtime type erasure.


# Conclusion

The isolated compile-time Scheme compiler can merge back into conventional C++ software logic smoothly when mapped cleanly.

Execution graphs establish bindings safely and securely. You can compile scripting sequences straight into local variables and correctly pipe their state transitions out to standard C++ callbacks via explicit Sender receivers safely.


# References
