// installtest/test_closure.cpp                                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Exercises closure module headers.

#include <smd/smdscheme/closure/closure_program.hpp>
#include <smd/smdscheme/closure/cps_code.hpp>
#include <smd/smdscheme/closure/eval_direct.hpp>
#include <smd/smdscheme/closure/value.hpp>

#include <string_view>
#include <variant>

static_assert([] {
    using namespace smd::smdscheme;
    using namespace std::string_view_literals;

    auto r = closure::compile_to_closure("(+ 1 (* 2 3))"sv);
    if (!r.has_value())
        return false;
    auto env = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto vr = r.value()(env);
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 7;
}());

static_assert([] {
    using namespace smd::smdscheme;
    using namespace std::string_view_literals;

    auto r = closure::compile_to_closure("((lambda (x) (* x x)) 7)"sv);
    if (!r.has_value())
        return false;
    auto env = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto vr = r.value()(env);
    return vr.has_value() && std::holds_alternative<int>(vr.value()) &&
           std::get<int>(vr.value()) == 49;
}());

int main() { return 0; }
