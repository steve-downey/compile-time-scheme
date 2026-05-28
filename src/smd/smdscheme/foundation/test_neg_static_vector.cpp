// src/smd/smdscheme/foundation/test_neg_static_vector.cpp      -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

using namespace smd::smdscheme::foundation;
constexpr auto overflow = [] {
    static_vector<int, 2> vec{};
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3); // Overflow
    return vec.size();
}();
return overflow;
}
