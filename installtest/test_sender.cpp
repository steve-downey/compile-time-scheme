// installtest/test_sender.cpp                                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Exercises all sender module headers.

#include <smd/smdscheme/sender/build_scheme_tree.hpp>
#include <smd/smdscheme/sender/dump_scheme_plan.hpp>
#include <smd/smdscheme/sender/format_scheme_node.hpp>
#include <smd/smdscheme/sender/scheme_node_data.hpp>
#include <smd/smdscheme/sender/scheme_tree.hpp>
#include <smd/smdscheme/sender/sender_cps_program.hpp>
#include <smd/smdscheme/sender/sender_program.hpp>
#include <smd/smdscheme/sender/sender_v.hpp>
#include <smd/smdscheme/sender/string_writer.hpp>

#include <cassert>
#include <sstream>
#include <variant>

// compile_sender_cps: constexpr compilation pipeline.
static_assert([] {
    auto r = smd::smdscheme::sender_cps::compile_sender_cps<32, 16>("(+ 3 4)");
    return r.has_value();
}());

// compile_to_sender: constexpr compilation to sender program.
static_assert([] {
    auto r = smd::smdscheme::sender::compile_to_sender<32, 16>("(+ 3 4)");
    return r.has_value();
}());

int main() {
    using namespace smd::smdscheme;
    using namespace std::string_view_literals;

    // string_writer: Writer monad for logging strings.
    {
        using sender::string_writer;
        auto w = string_writer<int>::pure(42);
        assert(w.log.empty() && w.value == 42);
    }

    // Runtime CPS evaluation via sender pipeline.
    auto program_r = sender_cps::compile_sender_cps<32, 16>("(+ 3 4)"sv);
    assert(program_r.has_value());
    auto env = closure::default_env<elaborator::core_type<32, 16>, 16>();
    auto result = program_r.value()(env);
    assert(result.has_value());
    assert(std::get<int>(result.value()) == 7);

    // dump_scheme_plan: reflect on a sender type and produce DOT output.
    using S = decltype(sender_v::then(sender_v::just(1),
                                      [](int x) { return x + 1; }));
    std::ostringstream oss;
    sender::dump_scheme_plan<S>(oss);
    assert(!oss.str().empty());

    return 0;
}
