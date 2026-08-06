// src/smd/cl/reader/number.test.cpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/cl/reader/number.hpp>
#include <smd/cl/reader/number.hpp> // test 2nd include OK

#include <catch2/catch_test_macros.hpp>

#include <string_view>

using smd::cl::reader::classify_number;
using smd::cl::reader::digit_value;
using smd::cl::reader::number_class;

namespace {

constexpr auto fixnum_of(std::string_view text, int radix, int expected)
    -> bool {
    auto const c = classify_number(text, radix);
    return c.cls == number_class::fixnum && c.value == expected;
}

constexpr auto class_of(std::string_view text, int radix, number_class cls)
    -> bool {
    return classify_number(text, radix).cls == cls;
}

constexpr auto decimal_integers() -> bool {
    return fixnum_of("42", 10, 42) && fixnum_of("-7", 10, -7) &&
           fixnum_of("+9", 10, 9) && fixnum_of("0", 10, 0) &&
           fixnum_of("10.", 10, 10) && // trailing decimal point (ANSI 2.3.1)
           fixnum_of("-10.", 10, -10);
}

constexpr auto fixnum_range_boundaries() -> bool {
    // The fixnum representation is int; both boundaries must classify as
    // fixnum, one past each as bignum spelling.
    return fixnum_of("2147483647", 10, 2147483647) &&
           fixnum_of("-2147483648", 10, -2147483647 - 1) &&
           class_of("2147483648", 10, number_class::bignum) &&
           class_of("-2147483649", 10, number_class::bignum) &&
           class_of("123456789012345678901234567890", 10, number_class::bignum);
}

constexpr auto ratios() -> bool {
    return class_of("1/2", 10, number_class::ratio) &&
           class_of("-3/4", 10, number_class::ratio) &&
           class_of("101/11", 2, number_class::ratio) &&
           class_of("1/", 10, number_class::none) &&
           class_of("/2", 10, number_class::none) &&
           class_of("1/2/3", 10, number_class::none);
}

constexpr auto floats() -> bool {
    return class_of("1.5", 10, number_class::floating) &&
           class_of(".5", 10, number_class::floating) &&
           class_of("-.5", 10, number_class::floating) &&
           class_of("1E5", 10, number_class::floating) &&
           class_of("1.5E-3", 10, number_class::floating) &&
           class_of("1.5D0", 10, number_class::floating) &&
           class_of("1S0", 10, number_class::floating) &&
           class_of("3F2", 10, number_class::floating) &&
           class_of("2L+4", 10, number_class::floating) &&
           class_of("5.E3", 10, number_class::floating);
}

constexpr auto symbols_are_not_numbers() -> bool {
    // Whole-token classification (DIV-0003): 1+ is a symbol.
    return class_of("1+", 10, number_class::none) &&
           class_of("2BUFFER", 10, number_class::none) &&
           class_of("+", 10, number_class::none) &&
           class_of("-", 10, number_class::none) &&
           class_of(".", 10, number_class::none) &&
           class_of("", 10, number_class::none) &&
           class_of(".E3", 10, number_class::none) &&
           class_of("E5", 10, number_class::none) &&
           class_of("1.5X3", 10, number_class::none) &&
           class_of("1..2", 10, number_class::none);
}

constexpr auto radix_integers() -> bool {
    return fixnum_of("1010", 2, 10) && fixnum_of("777", 8, 511) &&
           fixnum_of("FF", 16, 255) && fixnum_of("-FF", 16, -255) &&
           fixnum_of("Z", 36, 35) &&
           class_of("2", 2, number_class::none) && // 2 is no binary digit
           class_of("G", 16, number_class::none);
}

constexpr auto no_floats_outside_decimal() -> bool {
    // E is a digit in radix 16 and float syntax is decimal-only.
    return fixnum_of("1E5", 16, 0x1E5) &&
           class_of("1.5", 16, number_class::none);
}

constexpr auto digit_values() -> bool {
    return digit_value('0') == 0 && digit_value('9') == 9 &&
           digit_value('A') == 10 && digit_value('Z') == 35 &&
           digit_value('+') == -1 && digit_value('a') == -1;
}

} // namespace

static_assert(decimal_integers());
static_assert(fixnum_range_boundaries());
static_assert(ratios());
static_assert(floats());
static_assert(symbols_are_not_numbers());
static_assert(radix_integers());
static_assert(no_floats_outside_decimal());
static_assert(digit_values());

TEST_CASE("NumberTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("NumberTest - DecimalIntegers") {
    CHECK(decimal_integers());
    CHECK(fixnum_range_boundaries());
}

TEST_CASE("NumberTest - RatiosAndFloats") {
    CHECK(ratios());
    CHECK(floats());
    CHECK(no_floats_outside_decimal());
}

TEST_CASE("NumberTest - WholeTokenClassification") {
    CHECK(symbols_are_not_numbers());
    CHECK(digit_values());
}

TEST_CASE("NumberTest - RadixIntegers") { CHECK(radix_integers()); }
