#!/usr/bin/env python3
"""Import the original Go data tables into checked-in C++ (not a build step).

Usage: python3 tools/import-go-tables.py [path/to/bamboo-core]
"""
import ast
import json
from pathlib import Path
import re
import sys

repo = Path(__file__).resolve().parents[1]
core = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else repo.parent / 'bamboo-core'
output = ['/* Copyright (C) 2018 Luong Thanh Lam. MIT; see LICENSE.core. */',
          '#include "core.h"', 'namespace bamboo {']
for filename, cpp_type, function in [('input_method_def.go', 'Definitions', 'inputMethodDefinitions'),
                                     ('charset_def.go', 'Charsets', 'charsetDefinitions')]:
    output += [f'const {cpp_type} &{function}() {{', f'static const {cpp_type} data = {{']
    entries = 0
    for line in (core / filename).read_text().splitlines():
        section = re.match(r'\s*("(?:[^"\\]|\\.)*"): \{', line)
        if section:
            output.append('{' + section[1] + ', {')
            continue
        key = r'"(?:[^"\\]|\\.)*"' if cpp_type == 'Definitions' else r"'(?:[^'\\]|\\.)*'"
        entry = re.match(r'\s*(' + key + r'):\s*("(?:[^"\\]|\\.)*"|`[^`]*`),', line)
        if entry:
            left = ast.literal_eval(entry[1])
            right = entry[2][1:-1] if entry[2].startswith('`') else ast.literal_eval(entry[2])
            cpp_key = json.dumps(left, ensure_ascii=False) if cpp_type == 'Definitions' else str(ord(left))
            output.append('{' + cpp_key + ', ' + json.dumps(right, ensure_ascii=False) + '},')
            entries += 1
        elif line == '\t},':
            output.append('}},')
    if entries == 0:
        raise RuntimeError(f'No entries imported from {filename}')
    output.append('}; return data; }')
    print(f'{filename}: {entries} entries')
output.append('} // namespace bamboo')
(repo / 'bamboo/native/data.cpp').write_text('\n'.join(output) + '\n')
