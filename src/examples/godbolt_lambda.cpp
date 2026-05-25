// src/examples/godbolt_lambda.cpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/schemepoc/eval_direct.hpp>
#include <smd/schemepoc/schemepoc.hpp>
#include <iostream>

template <smd::schemepoc::source_literal Source>
struct compiled_program {
    static constexpr auto tree = [] {
        auto dr = smd::schemepoc::read_datum<64, 32>(smd::schemepoc::cursor{Source.view()});
        auto er = smd::schemepoc::elaborate(dr.value().value);
        return er.value();
    }();

    template <int MaxBindings>
    constexpr auto operator()(smd::schemepoc::env<MaxBindings> const& env) const {
        return smd::schemepoc::eval_direct(tree, tree.size() - 1, env);
    }
};

template <smd::schemepoc::source_literal Source>
inline constexpr compiled_program<Source> compiled_eval{};

int main(int argc, char**)
{
    auto env = smd::schemepoc::default_env<16>();
    env.define("argc", smd::schemepoc::value{argc});
    
    // Evaluate a lambda using argc
    auto result = compiled_eval<"((lambda (x) (+ 1 (* x x))) argc)">(env);
    
    if (result.has_value()) {
        std::cout << std::get<int>(result.value()) << '\n';
    } else {
        std::cout << result.error().message << '\n';
        return 1;
    }
    return 0;
}
