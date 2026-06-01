<div class="abstract" id="orgb03c617">
<p>
After journeying through the lexer, elaborating core forms, establishing CPS, and mapping DOT pipelines, the question remains: what does it look like to use these structures inside a practical C++26 program?
This phase focuses on invoking a Scheme script from C++, passing parameters, and interacting with the physical execution environment.
</p>

</div>


# Execution Frontends

Throughout this project, the implementation has enforced a strict boundary between compile-time evaluation and runtime environment mapping. Because the compiler validates, elaborates, and lowers the abstract syntax tree entirely during `constexpr` calculation, the runtime executor takes the statically generated C++ execution pipeline and drives data through it sequentially.


## Standard Closure Evaluation

The standard methodology for embedding the Scheme compiler into a runtime process involves pre-compiling a script via `scm::compiled_closure`.

```cpp
constexpr auto program =
    scm::compiled_closure<"(print-and-add current-year 10)">;
```

Here, the template string `(print-and-add current-year 10)` is fully elaborated and safely evaluated down to the lowest CPS core at compile-time.

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

To bridge the C++ type system with the generic value wrappers utilized internally, the architecture utilizes a \`std::span\` structure over dynamically generated \`scm::closure::value<Core>\` abstractions.

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

First, standard instantiation requires `compile_to_sender` rather than the simplified `compiled_closure` variant.

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
auto s = program(env);
auto res_opt = scm::sender_v::sync_wait(std::move(s));

scm::reflection::reified_environment<RuntimeStateTag> state{};
```

Extraction happens by invoking `scm::sender_v::sync_wait(std::move(s))`, which initializes propagation across the pipeline, asynchronously awaiting resolution safely before unwrapping local evaluation values locally into `res_opt`.


# Conclusion

The isolated compile-time Scheme compiler can merge back into conventional C++ software logic smoothly when mapped cleanly.

Execution graphs establish bindings safely and securely. You can compile scripting sequences straight into local variables and correctly pipe their state transitions out to standard C++ callbacks via explicit Sender receivers safely.


# References
