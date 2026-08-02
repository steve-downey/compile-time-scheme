**DRAFT &#x2014; pending author revision**

<div class="abstract" id="orga4f36e3">
<p>
Step R2 built <code>src/smd/cl/symbol</code>.
A symbol is an entry in a table, named by a stable <code>symbol_id</code>, carrying a name and independently writable value, function, and macro slots.
Identity is id comparison, which is what <code>eq</code> means on symbols, and the table owns its name characters, so nothing downstream holds a view into the reader's storage.
The old tree's <code>closure::symbol</code> is a <code>std::string_view</code> wrapper, and seven of the recorded divergences turn out to be that one absence billed seven times.
The headline is DIV-0009: a recursive <code>defun</code> fails with <code>undefined function</code>, which is not a thing a Lisp gets to do.
R2 also settled a question the plan had left open for it: the symbol table survives into the runtime program.
</p>

</div>

{{TEASER\_END}}

<nav style="margin-bottom: 2em; border-bottom: 1px solid #ccc; padding-bottom: 1em">

[↑ Series Index](index.md) | [Phase 24 - Two Copies and a Fold That Stops ←](phase-24-substrate.md)

</nav>


# The bug that named the step

```lisp
(defun len (l) (if (null l) 0 (+ 1 (len (cdr l)))))
```

That doesn't work in `smdlisp`. Call `len` and you get `undefined function`. Phase 22 gave the mechanism: a function's closure captures a copy of the environment made before `defun` binds the function's own name into it, so the self-call is looking for a name that copy never had. It's recorded as DIV-0009, classified a defect, and step L14's recursive `block` tests were replaced by lexically nested ones, so two live activations of the same block name could be witnessed without a self-call.

The rebuild plan's §3 puts it in a table with six others. `eq` and `eql` are the same comparison (DIV-0002). `defmacro` is scoped to one top-level form (DIV-0010). There is no `gensym` at all: `or`, `cond` and `case` bind fixed reserved names (`%OR-TEMP`, `%COND-TMP`, `%CASE-TMP`) where ANSI's expansions use a fresh symbol (DIV-0006). Dynamically binding a special that has no value yet is an error (DIV-0014). The datum arena has to outlive the compiled program (DIV-0007). There is no package system (DIV-0001), and that one D12 only partly answers: a package is a symbol table, and R2 builds exactly one.

Six of them were filed because each broke on its own, over nine days, in different parts of the pipeline; DIV-0001 was a plan-time scope decision and predates all of them. Written down next to each other they're all one thing. A `std::string_view` wrapper has nowhere to keep anything, so `defun` has to put the function in the environment, and the environment is the thing that gets copied. Both `eq` and `eql` end up as string comparison because comparison is all a name supports. There's no way to make a symbol that a later mention of the same spelling won't find, so `gensym` becomes a promise about names nobody else will write. And a program's symbol names are views into the reader's storage, so the reader's storage can never go away.

D12 is the plan's answer, and it is short. A symbol is an entry in a symbol table, referred to by a stable id, carrying a name, a value slot, a function slot, and a macro slot. Identity is id comparison, not string comparison. R2 is the step where I find out what that costs to build.


# An entry, and two vectors

```cpp
struct entry {
        int name_offset{};
        int name_length{};
        bool interned{};
        std::optional<ValueSlot> value{};
        std::optional<FunctionSlot> function{};
        std::optional<MacroSlot> macro{};
};

[[nodiscard]] constexpr auto name_of(entry const &e) const
        -> std::string_view;
constexpr auto append_entry(std::string_view name, bool interned)
        -> symbol_id;
[[nodiscard]] constexpr auto entry_at(symbol_id id) -> entry &;
[[nodiscard]] constexpr auto entry_at(symbol_id id) const -> entry const &;

foundation::static_vector<entry, MaxSymbols> entries_{};
foundation::static_vector<char, MaxNameChars> chars_{};
```

An entry is an offset and a length into a pooled block of characters, a flag saying whether name lookup can reach it, and three slots. The storage is two of R1's `static_vector` containers: the entries, and the character pool the names live in.

Three slots, because D12 says three. Value and function are the Lisp-2 split Phase 17 was about, and the macro slot is where `defmacro` will be able to put something that survives the top-level form it appeared in. Each is a `std::optional`, and empty means unbound, which is a genuinely different state from bound to `nil`; that distinction is all DIV-0014 ever wanted.

The slot types are template parameters. At R2 nothing in the new tree knows what a value is. There's no reader, no core AST, no evaluator; what a binding **is** belongs to stages that haven't been written. So the table takes three types and never looks inside any of them, and the test instantiates it with three distinct one-field structs so that a write into the wrong slot fails to compile instead of quietly passing.

The two capacities are there for the same reason in reverse. D14 says no runtime value type may be parameterised on a container's capacity, and the standing example is `static_assert(MaxBindings == 16, ...)` appearing three times in `closure/cps_code.hpp`, which is what happens when a `value` contains a `closure` contains an `env<Core, MaxBindings>`. A table is storage. Capacities are properties of storage, so they belong here and only here.


# The id is an index and nothing else

```cpp
class symbol_id {
  public:
    /// Constructs the invalid id.
    constexpr symbol_id() = default;

    /// Constructs an id naming the entry at @p index in its owning table.
    /// @pre index >= 0
    constexpr explicit symbol_id(int index);

    /// Returns the entry index, or -1 for the invalid id.
    [[nodiscard]] constexpr auto index() const -> int;

    /// Returns true unless this is the invalid (default-constructed) id.
    [[nodiscard]] constexpr auto valid() const -> bool;

    // HIDDEN FRIEND
    friend constexpr auto operator==(symbol_id, symbol_id) -> bool = default;

  private:
    int index_{-1};
};
```

An `int` and a defaulted `operator==`. That is the entire runtime representation of the keystone decision, and the defaulted comparison is `eq` on symbols. It's the half of DIV-0002 that was blocked on symbols being strings. `eql` only comes apart from `eq` once there are characters and a numeric tower for it to come apart over, and there aren't.

Two things about it needed deciding.

It carries no capacity, which is what lets a program traffic in ids freely. Recovering a name or a slot requires the owning table, and that's the trade being made: identity gets cheap and portable, and everything else needs a second argument.

And it's default-constructible, to an id that names no entry, with an `index()` of -1. That's R1's finding coming back. `static_vector<T, N>` holds a `std::array`, so its elements have to be default-constructible; R1 discovered this when `static_vector<result<T>, N>` refused to instantiate, because `result<T>` isn't. Containers of ids need to work, so an id has to have a default state, and the honest default is one that's invalid. `valid()` tells them apart, and a default id reaching a table accessor trips an assert.


# `intern`, and the entry nobody can find

```cpp
template <class ValueSlot, class FunctionSlot, class MacroSlot, int MaxSymbols,
          int MaxNameChars>
constexpr auto symbol_table<ValueSlot, FunctionSlot, MacroSlot, MaxSymbols,
                            MaxNameChars>::intern(std::string_view name)
    -> symbol_id {
    if (auto const existing = find(name)) {
        return *existing;
    }
    return append_entry(name, true);
}
```

Find it or append it. Appending copies the name's characters into the pool and pushes an entry whose `interned` flag is true.

```cpp
template <class ValueSlot, class FunctionSlot, class MacroSlot, int MaxSymbols,
          int MaxNameChars>
constexpr auto symbol_table<ValueSlot, FunctionSlot, MacroSlot, MaxSymbols,
                            MaxNameChars>::find(std::string_view name) const
    -> std::optional<symbol_id> {
    auto const interned_with_name = [&](entry const &e) {
        return e.interned && name_of(e) == name;
    };
    auto const it = std::ranges::find_if(entries_, interned_with_name);
    if (it == entries_.end()) {
        return std::nullopt;
    }
    return symbol_id{static_cast<int>(it - entries_.begin())};
}
```

The predicate is two clauses, and the first one is the whole `gensym` design. `make_uninterned` appends an entry with that flag false. The entry is ordinary in every other way: it has a name you can print, slots you can read and write, an id that compares equal to itself. What it doesn't have is a route in from a spelling. So interning `g1` after making an uninterned `g1` produces a different symbol, pinned by a test and by its `static_assert` twin; and two uninterned symbols both named `g1` are distinct from each other, which the runtime test pins on its own. Set that next to `%OR-TEMP`, which is a reserved-name convention that works right up until somebody writes the reserved name.

One small thing, which looks like nothing at the point where you write it. `find` reports absence with `std::nullopt` rather than a null pointer, and there's a reason for that beyond taste. DIV-0013 still reproduces on the current trunk (GCC 16.0.1 20260322, r16-8246): under `-fsanitize=null`, a null comparison against the address of a subobject of a namespace-scope `constexpr` object does not fold. A symbol table is going to end up as exactly that kind of object. R2 needs no null comparisons at all on the constant-evaluation path, which is luck as much as design, and I'd rather write it down than rediscover it.


# The table owns its characters

The test I care most about in this step is short:

```cpp
std::array<char, 3> buf{'f', 'o', 'o'};
table t;
auto const id = t.intern(std::string_view{buf.data(), buf.size()});
buf[0] = 'z';
CHECK(t.name(id) == "foo");
```

Interning copies. Mutate the caller's buffer afterwards and the symbol's name doesn't move, because the characters are in the table's own pool now.

That's DIV-0007 being closed, and DIV-0007 is the one that always bothered me the most. A compiled `smdlisp` program is a value you can carry around and evaluate later, except that all of the symbol names in it point into the reader's datum arena, so it's a value you can carry around only as far as the arena goes. A program plus its table is self-contained. Nothing in it points anywhere else.


# Does the table survive into the run?

The plan's §7 listed this as an open question and assigned it to R2: is the symbol table a compile-time structure that gets consumed, or does it ride along into the running program?

It rides along. Three things say so, and none of them is close. `symbol-name` and printing have to recover a name, and an id can not do that by itself, so if the names don't travel then D12's self-containedness promise isn't kept for anything observable. The slots are runtime state: `setq` under the evaluator writes the value slot, so the table can't end at the compile/run boundary. And the macro slot is the compile-time face of the same entries the value slot is the runtime face of &#x2014; one table, both stages.

The cost is the whole of both containers: `static_vector` holds a `std::array`, so a table is `MaxSymbols` entries and `MaxNameChars` characters from the moment it's default-constructed. D14 is what keeps that cost from spreading. Programs traffic in `symbol_id`, which carries no capacity, so a table riding along puts nothing capacity-shaped into any value type's identity. Which means the representation can be made smaller later without touching `symbol_id` or anything built on it.


# What R2 does not settle

Table overflow is a precondition the caller has to meet. `append_entry` asserts that there's room for one more entry and for the name's characters, matching `static_vector::push_back`, which asserts the same way. A reader consuming input it didn't write will have to check `size() < capacity()` and the pooled-character room and produce a `parse_error` before it interns anything, because otherwise the Asan build trips the assert instead of diagnosing the program. That's the right shape for a substrate component and it's going to be somebody else's problem in about a week.

`find` is a linear scan over the entries. With eight symbols in a test that's nothing, and I have no measurement that says it will ever be anything. A lookup table with no lookup structure in it looks like an oversight. It was a choice, and it's revisable without touching the interface.

The larger caution is about what "closes" means. Six divergences are closed by construction and a seventh partly, in a component that has no callers. The plan's claim is that those seven were one absence; R2 fills the absence. Whether filling it actually makes `(defun len (l) ...)` work is something a step that has an evaluator gets to find out, and there isn't one in this tree yet. All of the behaviour above is `constexpr`, with a `static_assert` twin beside every runtime test, so the table interns at compile time. What it interns is still up to somebody else.

<nav style="margin-top: 3em; border-top: 1px solid #ccc; padding-top: 1em">

[↑ Series Index](index.md) | [← Phase 24 - Two Copies and a Fold That Stops](phase-24-substrate.md)

</nav>


# References
