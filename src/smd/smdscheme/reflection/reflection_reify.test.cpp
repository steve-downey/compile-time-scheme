// src/smd/schemepoc/reflection_reify.test.cpp       -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdscheme/reflection/reflection_reify.hpp>

#include <catch2/catch_test_macros.hpp>

namespace smd::smdscheme::reflection {

struct MyEnvTag {};

consteval {
    compile_environment<MyEnvTag>(
        {{^^int, "count"}, {^^float, "ratio"}, {^^bool, "flag"}});
}

TEST_CASE("ReflectionReify - GeneratesAggregate", "[reflection]") {
    reified_environment<MyEnvTag> env;
    env.count = 42;
    env.ratio = 3.14f;
    env.flag = true;

    REQUIRE(env.count == 42);
    REQUIRE(env.ratio == 3.14f);
    REQUIRE(env.flag == true);
}

} // namespace smd::smdscheme::reflection
