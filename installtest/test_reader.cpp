// installtest/test_reader.cpp                                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Exercises parser and reader module headers.

#include <smd/smdscheme/parser/alt.hpp>
#include <smd/smdscheme/parser/cursor.hpp>
#include <smd/smdscheme/parser/parser.hpp>
#include <smd/smdscheme/parser/parser_ops.hpp>
#include <smd/smdscheme/reader/atom.hpp>
#include <smd/smdscheme/reader/datum_type.hpp>
#include <smd/smdscheme/reader/read_datum.hpp>

#include <string_view>
#include <variant>

static_assert([] {
    using namespace smd::smdscheme;
    using namespace std::string_view_literals;

    foundation::tree_arena<reader::datum_type<32, 16>, 32> arena;
    auto r = reader::read_datum<32, 16>(parser::cursor{"42"sv}, arena);
    if (!r.has_value())
        return false;
    return std::holds_alternative<reader::datum_integer>(
               r.value().value.inner) &&
           std::get<reader::datum_integer>(r.value().value.inner).value == 42;
}());

static_assert([] {
    using namespace smd::smdscheme;
    using namespace std::string_view_literals;

    foundation::tree_arena<reader::datum_type<32, 16>, 32> arena;
    auto r = reader::read_datum<32, 16>(parser::cursor{"#t"sv}, arena);
    if (!r.has_value())
        return false;
    return std::holds_alternative<reader::datum_boolean>(
               r.value().value.inner) &&
           std::get<reader::datum_boolean>(r.value().value.inner).value;
}());

int main() { return 0; }
