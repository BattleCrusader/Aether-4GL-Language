#!/usr/bin/env python3
"""Convert only the 45 actual test fixtures (from TEST_FIXTURES in Makefile).

Does NOT touch spec documentation files like test_spec_*.ae.
"""

import os
import re

# Only these files are actual test fixtures
TEST_FIXTURES = [
    "hello.ae",
    "test_math.ae", "test_params.ae", "test_types.ae",
    "test_struct.ae", "test_enum.ae", "test_match.ae",
    "test_defer.ae", "test_region.ae", "test_optional.ae",
    "test_trait.ae", "test_generic.ae", "test_iflet.ae",
    "test_trycatch.ae", "test_throw.ae", "test_errors.ae",
    "test_comptime.ae", "test_const.ae", "test_contract.ae",
    "test_closure.ae", "test_op_overload.ae", "test_monomorph.ae",
    "test_dyn.ae", "test_sysfunc.ae", "test_export.ae",
    "test_entry.ae", "test_module_abi.ae",
    "test_interp_basic.ae", "test_interp_multi.ae",
    "test_interp_expr.ae", "test_interp_none.ae",
    "test_interp_concat.ae", "test_interp_numbers.ae",
    "test_interp_numeric.ae", "test_interp_num_concat.ae",
    "test_interp_print_num.ae",
    "test_import.ae", "test_asm_comment.ae", "test_segfault.ae",
    "test_args.ae", "test_null_concat.ae",
    "test_logical_keywords.ae", "test_aelib_import.ae",
    "test_std_test.ae", "test_variadic.ae",
]

FIXTURES_DIR = "tests/fixtures"

def process_file(filepath):
    with open(filepath) as f:
        content = f.read()
    
    name = os.path.basename(filepath)
    
    # 0. Extract expected values from @test(expect=N) before removing them
    expect_map = {}
    lines = content.split('\n')
    for i, line in enumerate(lines):
        m = re.match(r'@test\(expect=(\d+)\)', line.strip())
        if m:
            expected = int(m.group(1))
            for j in range(i + 1, min(i + 3, len(lines))):
                fm = re.match(r'^func\s+(\w+)', lines[j])
                if fm:
                    expect_map[fm.group(1)] = expected
                    break
    
    # 1. Replace @test(expect=N) with @test
    content = re.sub(r'@test\(expect=\d+\)', '@test', content)
    
    # 2. Add import "std/test" if not present
    has_import_test = 'import "std/test"' in content or "import 'std/test'" in content
    has_any_import = re.search(r'^import\s+', content, re.MULTILINE)
    
    if not has_import_test:
        if has_any_import:
            content = re.sub(r'^(import[^\n]*\n)(?!import)', r'\1import "std/test"\n', content, count=1, flags=re.MULTILINE)
        else:
            lines = content.split('\n')
            insert_pos = 0
            for i, line in enumerate(lines):
                stripped = line.strip()
                if stripped == '' or stripped.startswith('//') or stripped.startswith('/*') or stripped.startswith('*'):
                    insert_pos = i + 1
                else:
                    break
            lines.insert(insert_pos, 'import "std/test"')
            lines.insert(insert_pos + 1, '')
            content = '\n'.join(lines)
    
    # 3. Process line by line, tracking @test function boundaries
    lines = content.split('\n')
    new_lines = []
    in_test_func = False
    current_func_name = None
    brace_depth = 0
    i = 0
    
    while i < len(lines):
        line = lines[i]
        
        if line.strip() == '@test':
            in_test_func = True
            new_lines.append(line)
            i += 1
            continue
        
        if in_test_func:
            m = re.match(r'^(func\s+(\w+)\s*\([^)]*\))\s*:\s*(u64|int)', line)
            if m:
                current_func_name = m.group(2)
                line = re.sub(r'(func\s+\w+\s*\([^)]*\))\s*:\s*(u64|int)', r'\1: void', line)
                brace_depth = 0
                for ch in line:
                    if ch == '{': brace_depth += 1
                    if ch == '}': brace_depth -= 1
                new_lines.append(line)
                i += 1
                continue
        
        if in_test_func:
            for ch in line:
                if ch == '{': brace_depth += 1
                if ch == '}': brace_depth -= 1
            
            m = re.match(r'^(\s*)return\s+(.*?)\s*$', line)
            if m:
                indent = m.group(1)
                expr_raw = m.group(2)
                expr = re.sub(r'\s*//.*$', '', expr_raw).strip()
                expected = expect_map.get(current_func_name, 0)
                
                if re.match(r'^\d+$', expr) or re.match(r'^0x[0-9a-fA-F]+$', expr):
                    i += 1
                    continue
                if '(' in expr or '=' in expr:
                    new_lines.append(f'{indent}{expr}')
                    i += 1
                    continue
                new_lines.append(f'{indent}assertEquals({expected}, {expr}, "{name}")')
                i += 1
                continue
            
            if brace_depth <= 0:
                in_test_func = False
                current_func_name = None
        
        new_lines.append(line)
        i += 1
    
    content = '\n'.join(new_lines)
    
    with open(filepath, 'w') as f:
        f.write(content)

def main():
    for fixture in TEST_FIXTURES:
        filepath = os.path.join(FIXTURES_DIR, fixture)
        if not os.path.exists(filepath):
            print(f"  SKIP (not found): {fixture}")
            continue
        process_file(filepath)
        print(f"  Converted: {fixture}")

if __name__ == '__main__':
    main()
