// src/smd/smdscheme/parser/test_neg_parser_concept.cpp         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

using namespace smd::smdscheme::parser;
// 'int' is not a parser_like.
[[maybe_unused]] auto bad_parser = map(42, [](auto v) { return v; });
return 0;
}
