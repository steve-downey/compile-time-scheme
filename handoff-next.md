# Next step: Step 20 (lambda syntax — elaborator hardening)

## Goal

Harden the elaborator's `lambda` handling with the one constraint that is
currently missing: duplicate-parameter detection.
The elaborator already parses `(lambda (params) body)` and stores
`core_lambda<MaxList>`, but it does not yet reject `(lambda (x x) body)`.
Add that check and add more thorough elaboration tests.
Do **not** touch `cps_dispatch` — lambda still returns "unsupported form"
at the CPS level; that comes in steps 21–22.

## Files expected to change

```txt
src/smd/schemepoc/elaborator.hpp      (add duplicate-param check)
src/smd/schemepoc/elaborator.test.cpp (add DuplicateParam error test,
                                        multi-param and complex-body tests)
checklist.md                           (mark step done)
handoff-next.md                        (update for step 21)
```

No other files need to change.

## Current state

`elaborator.hpp` `elaborate_list` (around line 118–141) already:
- Recognizes the `lambda` special form
- Checks that each param is a `datum_symbol`
- Stores `core_lambda<MaxList>{params, body_id}`

What it does **not** do: check for duplicate param names.

`cps.hpp` `cps_dispatch` (last branch, around line 128–129) returns an
error for `core_lambda`:
```cpp
// core_lambda, core_define, core_quote: out of scope for step 17
return result<value>{parse_error{{}, "unsupported form"}};
```
Leave that as-is.

## Required implementation

In `elaborate_list`, after building `params`, scan for duplicates:

```cpp
// Check for duplicate parameter names.
for (int i = 0; i < params.size(); ++i) {
    for (int j = i + 1; j < params.size(); ++j) {
        if (params[i] == params[j])
            return parse_error{{}, "lambda: duplicate parameter"};
    }
}
```

Insert that block before `auto body_r = elaborate_node(...)`.

## Required tests (in elaborator.test.cpp)

1. **Duplicate param → error**
   ```cpp
   // "(lambda (x x) x)" -> elaboration error (duplicate parameter)
   static_assert([] {
       auto dr = read_datum<32, 16>(cursor{"(lambda (x x) x)"sv});
       if (!dr.has_value()) return false;
       auto er = elaborate(dr.value().value);
       return !er.has_value(); // must fail
   }());
   ```

2. **Multi-param lambda → correct shape**
   ```cpp
   // "(lambda (x y) (+ x y))" -> core_lambda with 2 params
   static_assert([] {
       auto dr = read_datum<32, 16>(cursor{"(lambda (x y) (+ x y))"sv});
       if (!dr.has_value()) return false;
       auto er = elaborate(dr.value().value);
       if (!er.has_value()) return false;
       auto const &ct = er.value();
       auto const &root = ct.get(ct.size() - 1);
       if (!std::holds_alternative<core_lambda<16>>(root)) return false;
       auto const &lam = std::get<core_lambda<16>>(root);
       return lam.params.size() == 2 &&
              lam.params[0] == "x"sv &&
              lam.params[1] == "y"sv;
   }());
   ```

3. **Complex body → correct shape** (lambda whose body is a call)
   ```cpp
   // "(lambda (x) (+ x 1))" -> core_lambda at root, body is core_application
   static_assert([] {
       auto dr = read_datum<32, 16>(cursor{"(lambda (x) (+ x 1))"sv});
       if (!dr.has_value()) return false;
       auto er = elaborate(dr.value().value);
       if (!er.has_value()) return false;
       auto const &ct = er.value();
       auto const &root = ct.get(ct.size() - 1);
       if (!std::holds_alternative<core_lambda<16>>(root)) return false;
       auto const &lam = std::get<core_lambda<16>>(root);
       return std::holds_alternative<core_application<16>>(ct.get(lam.body));
   }());
   ```

Add matching `TEST_CASE` blocks (Catch2 runtime versions) for all three.

## What to confirm but NOT change

- `cps.hpp` `cps_dispatch` continues to return "unsupported form" for
  `core_lambda`.  Do not add closure values or lambda evaluation here.
- `value.hpp` `value` remains `std::variant<int, bool, builtin>`.
- `schemepoc.hpp` `compiled_closure` is unchanged.
- `closure_backend.hpp` is unchanged.

## Required commands

```bash
make compile
make test
make lint
```

## Do not do

- Do not add a closure type to `value` (step 21).
- Do not make `cps_dispatch` handle `core_lambda` (step 21+).
- Do not implement function application for user lambdas (step 22).
- Do not proceed to step 21 in the same commit.
