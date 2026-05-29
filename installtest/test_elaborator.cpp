// installtest/test_elaborator.cpp                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Exercises elaborator module headers.

#include <smd/smdscheme/elaborator/elaborate.hpp>
#include <smd/smdscheme/elaborator/elaborated_core.hpp>
#include <smd/smdscheme/parser/cursor.hpp>
#include <smd/smdscheme/reader/read_datum.hpp>

#include <string_view>
#include <variant>

static_assert([] {
    using namespace smd::smdscheme;
    using namespace std::string_view_literals;

    foundation::tree_arena<reader::datum_type<32, 16>, 32> datum_arena;
    foundation::tree_arena<elaborator::core_type<32, 16>, 32> core_arena;

    auto dr =
        reader::read_datum<32, 16>(parser::cursor{"(+ 1 2)"sv}, datum_arena);
    if (!dr.has_value())
        return false;

    auto er =
        elaborator::elaborate<32, 16>(dr.value().value, datum_arena, core_arena);
    return er.has_value();
}());

int main() { return 0; }
