#!/usr/bin/env python3
"""
Amalgamates C++ source by recursively inlining `#include <smd/schemepoc/...>` directives.
Usage: scripts/amalgamate.py src/examples/advanced_reflection_ffi.cpp > godbolt.cpp
"""
import re
import os
import sys

def amalgamate(main_file):
    visited = set()
    out = []

    # Catch `#include <smd/schemepoc/...>` or `#include "smd/schemepoc/..."`
    include_re = re.compile(r'^\s*#\s*include\s*[<"](smd/schemepoc/[^>"]+)[>"]', re.MULTILINE)

    def process_file(filepath, is_main=False):
        if filepath in visited:
            return
        visited.add(filepath)

        try:
            with open(filepath, 'r') as f:
                content = f.read()
        except Exception as e:
            print(f"Warning: could not find {filepath}", file=sys.stderr)
            return

        lines = content.splitlines()

        if not is_main:
            # Strip prolog: copyright, include guards
            while lines and (
                lines[0].startswith('// src/') or \
                lines[0].startswith('// SPDX-') or \
                lines[0].startswith('#ifndef ') or \
                lines[0].startswith('#define ') or \
                not lines[0].strip()
            ):
                lines.pop(0)

            # Strip trailing empty lines
            while lines and not lines[-1].strip():
                lines.pop()

            # Strip epilog: the final #endif
            if lines and lines[-1].startswith('#endif'):
                lines.pop()

        for line in lines:
            m = include_re.match(line)
            if m:
                inc_path = m.group(1)
                local_path = os.path.join('src', inc_path)
                if os.path.exists(local_path):
                    # add a comment so we know where it came from
                    out.append(f"// --- Begin inlined: {inc_path} ---")
                    process_file(local_path, is_main=False)
                    out.append(f"// --- End inlined: {inc_path} ---\n")
                else:
                    out.append(line)
            else:
                out.append(line)

    process_file(main_file, is_main=True)
    return '\n'.join(out)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: {} <source_file>".format(sys.argv[0]))
        sys.exit(1)
    print(amalgamate(sys.argv[1]))
