// src/smd/smdlisp/reader/cl_chars.test.cpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <smd/smdlisp/reader/cl_chars.hpp>
#include <smd/smdlisp/reader/cl_chars.hpp> // test 2nd include OK

#include <smd/smdscheme/parser/cursor.hpp>

#include <catch2/catch_test_macros.hpp>

using smd::smdlisp::reader::is_constituent_char;
using smd::smdlisp::reader::is_delimiter;
using smd::smdlisp::reader::is_terminating_macro_char;
using smd::smdlisp::reader::is_whitespace;
using smd::smdlisp::reader::skip_cl_intertoken_space;
using smd::smdlisp::reader::skip_line_comment;
using smd::smdlisp::reader::to_upper_char;
using smd::smdscheme::parser::cursor;

// -- Case folding (D2) -------------------------------------------------

static_assert(to_upper_char('a') == 'A');
static_assert(to_upper_char('z') == 'Z');
static_assert(to_upper_char('A') == 'A');
static_assert(to_upper_char('0') == '0');
static_assert(to_upper_char('-') == '-');
static_assert(to_upper_char('?') == '?');

// -- Terminating / delimiter set ----------------------------------------

static_assert(is_terminating_macro_char('('));
static_assert(is_terminating_macro_char(')'));
static_assert(is_terminating_macro_char('\''));
static_assert(is_terminating_macro_char(';'));
static_assert(!is_terminating_macro_char('a'));
static_assert(!is_terminating_macro_char('+'));

static_assert(is_whitespace(' '));
static_assert(is_whitespace('\t'));
static_assert(is_whitespace('\n'));
static_assert(is_whitespace('\r'));
static_assert(is_whitespace('\f'));
static_assert(!is_whitespace('a'));

static_assert(is_delimiter(' '));
static_assert(is_delimiter('('));
static_assert(is_delimiter(';'));
static_assert(!is_delimiter('a'));
static_assert(!is_delimiter('-'));

static_assert(is_constituent_char('a'));
static_assert(is_constituent_char('Z'));
static_assert(is_constituent_char('0'));
static_assert(is_constituent_char('+'));
static_assert(is_constituent_char('-'));
static_assert(is_constituent_char('*'));
static_assert(!is_constituent_char(' '));
static_assert(!is_constituent_char('('));
static_assert(!is_constituent_char(';'));

// -- Comment skipping -----------------------------------------------------

static_assert([] {
    cursor c{"; comment\nfoo"};
    return skip_line_comment(c).peek() == '\n';
}());

static_assert([] {
    cursor c{"; comment"};
    return skip_line_comment(c).empty();
}());

static_assert([] {
    cursor c{"  ; a comment\n  foo"};
    return skip_cl_intertoken_space(c).peek() == 'f';
}());

static_assert([] {
    cursor c{"; leading\n; another\nfoo"};
    return skip_cl_intertoken_space(c).peek() == 'f';
}());

static_assert([] {
    cursor c{"foo"};
    return skip_cl_intertoken_space(c).peek() == 'f';
}());

static_assert([] {
    cursor c{"   "};
    return skip_cl_intertoken_space(c).empty();
}());

TEST_CASE("ClCharsTest - HeaderIsIdempotent") { REQUIRE(true); }

TEST_CASE("ClCharsTest - ToUpperCharFoldsAsciiLowercase") {
    CHECK(to_upper_char('a') == 'A');
    CHECK(to_upper_char('m') == 'M');
    CHECK(to_upper_char('Z') == 'Z');
    CHECK(to_upper_char('1') == '1');
}

TEST_CASE("ClCharsTest - IsTerminatingMacroChar") {
    CHECK(is_terminating_macro_char('('));
    CHECK(is_terminating_macro_char(')'));
    CHECK(is_terminating_macro_char('\''));
    CHECK(is_terminating_macro_char(';'));
    CHECK_FALSE(is_terminating_macro_char('x'));
}

TEST_CASE("ClCharsTest - IsConstituentCharComplementsDelimiter") {
    for (unsigned char c = 0; c < 128; ++c) {
        auto const ch = static_cast<char>(c);
        CHECK(is_constituent_char(ch) == !is_delimiter(ch));
    }
}

TEST_CASE("ClCharsTest - SkipLineCommentStopsBeforeNewline") {
    cursor c{"; hi there\nrest"};
    cursor result = skip_line_comment(c);
    CHECK(result.peek() == '\n');
}

TEST_CASE("ClCharsTest - SkipLineCommentToEndOfInput") {
    cursor c{"; no newline here"};
    cursor result = skip_line_comment(c);
    CHECK(result.empty());
}

TEST_CASE("ClCharsTest - SkipIntertokenSpaceInterleavesWhitespaceAndComments") {
    cursor c{"  ; skip me\n\n  ; skip me too\n  token"};
    cursor result = skip_cl_intertoken_space(c);
    CHECK(result.remaining() == "token");
}

TEST_CASE("ClCharsTest - SkipIntertokenSpaceOnEmptyInput") {
    cursor c{""};
    cursor result = skip_cl_intertoken_space(c);
    CHECK(result.empty());
}

TEST_CASE("ClCharsTest - SkipIntertokenSpaceNoLeadingSpace") {
    cursor c{"token"};
    cursor result = skip_cl_intertoken_space(c);
    CHECK(result.remaining() == "token");
}
