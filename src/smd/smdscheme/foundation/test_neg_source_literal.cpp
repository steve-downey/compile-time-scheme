// src/smd/smdscheme/foundation/test_neg_source_literal.cpp     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

using namespace smd::smdscheme;
char const *text = "not a literal";
[[maybe_unused]] auto closure = compiled_closure<text>;
return 0;
}
