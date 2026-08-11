<!-- markdownlint-disable MD013 -->

# Third-party material in `src/smd/cl/conformance/`

## `pfdietz/ansi-test`

`src/smd/cl/conformance/corpus.hpp` adapts a small number of test cases from
[`pfdietz/ansi-test`](https://github.com/pfdietz/ansi-test), the canonical
post-2020 MIT-licensed tree (not the older GCL/Debian-derived copy, which
carries a downstream LGPL classification and was not consulted).

- **Source:** `https://github.com/pfdietz/ansi-test`
- **Pinned commit:** `6e3f70002559d56d3e4a6f0b8ddcc083d202f066`
- **License:** MIT

The material was **adapted, not copied verbatim**: each corpus entry in
`ansi_test_adapted` (`src/smd/cl/conformance/corpus.hpp`) names, in its
`origin` field, the exact `ansi-test` file and `deftest` it came from. No
`deftest` body was rewritten to fit this project's narrower operator
surface — a body using an operator outside that surface (`let`, `flet`,
`values`, `catch`, `tagbody`, and so on) was discarded rather than
restructured. What *was* changed, form by form, is documented inline next
to each entry: how the expected result is checked in C++ (this project's
driver does not expose the symbol table a returned symbol's name would
need, so a bare-symbol expectation is rewritten as an `eq` or `null` check
against a quoted literal from the same source string), one keyword literal
substituted for a quoted symbol to stay within the operator surface
(`return-from.1`), one inert `locally` wrapper stripped (`length.error.8`),
and one function renamed to avoid colliding with another corpus entry
(`defun.1`).

### License text

```text
Copyright 2004 Paul F. Dietz

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

This license is compatible with this repository's
Apache-2.0 WITH LLVM-exception license; the two apply to different files,
and this notice exists so the `ansi-test`-derived fraction of
`corpus.hpp` carries its own attribution forward.
