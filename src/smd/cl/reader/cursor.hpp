// src/smd/cl/reader/cursor.hpp                                    -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Forwarding shim (step B2): smd::cl::reader::cursor, parse_state and
// advance_while now name smd::kit::parser's definitions -- moved there
// because they were already free of anything Lisp-specific, and B2's
// context-threaded parser<F> needed to compose over the same cursor and
// parse_state the reader already had. The include path and the names
// below are unchanged for every existing caller in this tree; only the
// definition moved. See src/smd/cl/foundation/source_pos.hpp for the shim
// pattern this reuses.
#ifndef SRC_SMD_CL_READER_CURSOR_HPP
#define SRC_SMD_CL_READER_CURSOR_HPP

#include <smd/kit/parser/cursor.hpp>

// The original cursor.hpp included source_pos.hpp; kept here so
// smd::cl::foundation still transitively names source_pos exactly as it
// did before this file became a shim.
#include <smd/cl/foundation/source_pos.hpp>

namespace smd::cl::reader {

using smd::kit::parser::advance_while;
using smd::kit::parser::cursor;
using smd::kit::parser::parse_state;

} // namespace smd::cl::reader

#endif
