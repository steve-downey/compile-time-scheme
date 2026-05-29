// installtest/test_foundation.cpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Exercises every header in the foundation module.

#include <smd/smdscheme/foundation/arena_box.hpp>
#include <smd/smdscheme/foundation/fix.hpp>
#include <smd/smdscheme/foundation/functor.hpp>
#include <smd/smdscheme/foundation/parse_error.hpp>
#include <smd/smdscheme/foundation/result.hpp>
#include <smd/smdscheme/foundation/source_pos.hpp>
#include <smd/smdscheme/foundation/source_span.hpp>
#include <smd/smdscheme/foundation/static_vector.hpp>
#include <smd/smdscheme/foundation/version.hpp>

#include <cassert>
#include <variant>

static_assert(smd::smdscheme::foundation::version::major == 0);

static_assert([] {
    smd::smdscheme::foundation::static_vector<int, 8> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    return v.size() == 3 && v[0] == 1 && v[2] == 3;
}());

static_assert([] {
    using smd::smdscheme::foundation::result;
    using smd::smdscheme::foundation::parse_error;
    using smd::smdscheme::foundation::source_pos;
    result<int> ok{42};
    result<int> err{parse_error{source_pos{1}, "oops"}};
    return ok.has_value() && !err.has_value() && ok.value() == 42;
}());

int main() { return 0; }
