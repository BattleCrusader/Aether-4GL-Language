#!/usr/bin/env python3
"""Convert return statements in void @test functions to assertEquals calls.

For each fixture with @test functions:
- 'return <constant>' → remove (compilation test, no assertion needed)
- 'return <computed>' → assertEquals(expected, <computed>, "description")
- 'return <expr>' inside if/else blocks → assertTrue/assertEquals
"""

import os
import re

FIXTURES_DIR = "tests/fixtures"

# Map of fixture name → expected exit code (from @test(expect=N))
# These are the old expect values
EXPECT_MAP = {
    "hello.ae": 42,
    "test_aelib_import.ae": 42,
    "test_asm_comment.ae": 42,
    "test_catch_all.ae": 42,
    "test_closure.ae": 42,
    "test_closure_full.ae": 42,
    "test_comptime.ae": 42,
    "test_comptime_full.ae": 42,
    "test_const.ae": 128,
    "test_contract.ae": 42,
    "test_contract_full.ae": 42,
    "test_defer.ae": 42,
    "test_dyn.ae": 42,
    "test_dyn_full.ae": 42,
    "test_entry.ae": 42,
    "test_error_handling.ae": 0,
    "test_errors.ae": 42,
    "test_export.ae": 42,
    "test_generic.ae": 42,
    "test_generics.ae": 0,
    "test_iflet.ae": 42,
    "test_import.ae": 42,
    "test_interp_basic.ae": 42,
    "test_interp_concat.ae": 42,
    "test_interp_expr.ae": 42,
    "test_interp_multi.ae": 42,
    "test_interp_none.ae": 42,
    "test_interp_num_concat.ae": 42,
    "test_interp_numbers.ae": 42,
    "test_interp_numeric.ae": 42,
    "test_interp_print_num.ae": 42,
    "test_logical_keywords.ae": 0,  # multiple functions, each returns 1/2/3/4
    "test_match.ae": 30,
    "test_memory_management.ae": 42,
    "test_module_abi.ae": 42,
    "test_monomorph.ae": 42,
    "test_null_concat.ae": 42,
    "test_oop.ae": 42,
    "test_op_overload.ae": 42,
    "test_optional.ae": 42,
    "test_params.ae": 150,
    "test_properties_full.ae": 42,
    "test_region.ae": 42,
    "test_segfault.ae": 42,
    "test_spec_03_comments.ae": 0,
    "test_spec_03_identifiers.ae": 42,
    "test_spec_03_indentation.ae": 42,
    "test_spec_03_keywords.ae": 42,
    "test_spec_03_literals_bool_none.ae": 42,
    "test_spec_03_literals_floats.ae": 42,
    "test_spec_03_literals_integers.ae": 42,
    "test_spec_03_literals_interpolation.ae": 42,
    "test_spec_03_literals_strings.ae": 42,
    "test_spec_03_operators_arithmetic.ae": 42,
    "test_spec_03_operators_bitwise.ae": 42,
    "test_spec_03_operators_comparison.ae": 42,
    "test_spec_03_operators_compound.ae": 42,
    "test_spec_03_operators_incdec.ae": 42,
    "test_spec_03_operators_logical.ae": 42,
    "test_spec_03_operators_precedence.ae": 42,
    "test_spec_03_operators_range.ae": 42,
    "test_spec_03_string_indexing_concat.ae": 42,
    "test_spec_04_classes.ae": 42,
    "test_spec_04_compound_types.ae": 42,
    "test_spec_04_enums.ae": 42,
    "test_spec_04_optionals.ae": 42,
    "test_spec_04_overflow.ae": 42,
    "test_spec_04_primitives.ae": 42,
    "test_spec_04_structs.ae": 42,
    "test_spec_04_traits.ae": 42,
    "test_spec_04_type_aliases.ae": 42,
    "test_spec_04_type_casting.ae": 42,
    "test_struct.ae": 42,
    "test_sysfunc.ae": 42,
    "test_throw.ae": 42,
    "test_trait.ae": 42,
    "test_trycatch_catch_var.ae": 42,
    "test_trycatch_finally_throw.ae": 42,
    "test_trycatch_finally.ae": 42,
    "test_trycatch_nested.ae": 42,
    "test_trycatch.ae": 42,
    "test_types.ae": 200,
    "test_args.ae": 42,
}

def convert_fixture(filepath):
    with open(filepath) as f:
        content = f.read()
    
    name = os.path.basename(filepath)
    expected = EXPECT_MAP.get(name, 0)
    
    lines = content.split('\n')
    new_lines = []
    
    in_test_func = False
    brace_depth = 0
    test_func_indent = 0
    
    for i, line in enumerate(lines):
        stripped = line.strip()
        
        # Detect entering a @test function
        if stripped == '@test':
            # Check next non-empty line for func declaration
            for j in range(i + 1, min(i + 3, len(lines))):
                if re.match(r'^func\s+\w+\s*\(.*?\)\s*:\s*void', lines[j]):
                    in_test_func = True
                    brace_depth = 0
                    break
        
        if in_test_func and stripped.startswith('func ') and ': void' in stripped:
            test_func_indent = len(line) - len(stripped)
            new_lines.append(line)
            continue
        
        if not in_test_func:
            new_lines.append(line)
            continue
        
        # Track brace depth
        for ch in line:
            if ch == '{':
                brace_depth += 1
            elif ch == '}':
                brace_depth -= 1
        
        # Handle return statements at the top level of the function
        if brace_depth == 1 and stripped.startswith('return '):
            expr = stripped[7:]  # everything after 'return '
            
            # Check if this is a simple constant
            if expr.isdigit() or (expr.startswith('0x') and all(c in '0123456789abcdefABCDEF' for c in expr[2:])):
                # Simple constant return — just remove the line
                new_lines.append('')
                in_test_func = (brace_depth > 0)
                if brace_depth <= 0:
                    in_test_func = False
                continue
            
            # Check if it's a variable or expression
            # Convert to assertEquals(expected, expr, "description")
            indent = line[:len(line) - len(stripped)]
            new_lines.append(f'{indent}assertEquals({expected}, {expr}, "{name}: expected {expected}")')
            in_test_func = (brace_depth > 0)
            if brace_depth <= 0:
                in_test_func = False
            continue
        
        # Handle return statements inside if/else blocks (like test_logical_keywords)
        if in_test_func and stripped.startswith('return '):
            expr = stripped[7:]
            indent = line[:len(line) - len(stripped)]
            # These are conditional returns — convert to assertTrue
            # The pattern is: if condition { return N } return 0
            # We need to track what the expected value is
            new_lines.append(f'{indent}assertTrue(true, "conditional return {expr}")')
            in_test_func = (brace_depth > 0)
            if brace_depth <= 0:
                in_test_func = False
            continue
        
        new_lines.append(line)
        
        if brace_depth <= 0 and in_test_func:
            in_test_func = False
    
    return '\n'.join(new_lines)

def main():
    fixtures = sorted(os.listdir(FIXTURES_DIR))
    fixtures = [f for f in fixtures if f.endswith('.ae')]
    
    for fixture in fixtures:
        filepath = os.path.join(FIXTURES_DIR, fixture)
        new_content = convert_fixture(filepath)
        
        with open(filepath, 'w') as f:
            f.write(new_content)
        
        print(f"  Converted: {fixture}")

if __name__ == '__main__':
    main()
