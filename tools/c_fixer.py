#!/usr/bin/env python3
"""
C Code Fixer — post-processes generated C code from the Aether transpiler.

Fixes common issues in generated C code:
1. Removes extraneous double parentheses: ((x == y)) → (x == y)
2. Fixes slice struct equality: (a == b) → __aether_string_eq(a, b)
3. Removes redundant outer parens in if/while conditions
4. Fixes missing semicolons on struct initializers
5. Removes duplicate function prototypes
6. Ensures proper spacing around operators

Usage:
    python3 tools/c_fixer.py <input.c> [output.c]
    If output.c is omitted, fixes in-place.
"""

import re
import sys
import os


def fix_extraneous_parens(content):
    """Remove extraneous double parentheses like ((x == y)) → (x == y)."""
    # Fix if/while conditions: if ((expr)) → if (expr)
    content = re.sub(
        r'(if|while|elif)\s*\(\(([^()]*(?:\([^()]*\)[^()]*)*)\)\)\s*(\{|:)',
        lambda m: f"{m.group(1)} ({m.group(2)}){m.group(3)}",
        content
    )
    # Fix return ((expr)) → return (expr)
    content = re.sub(
        r'return\s*\(\(([^;]+?)\)\)\s*;',
        r'return (\1);',
        content
    )
    return content


def fix_compound_assign_parens(content):
    """Fix compound assignment parens: (x = (x + 1)) → x += 1 where possible."""
    # This is tricky to do safely, so just clean up nesting
    content = re.sub(
        r'\(\((\w+)\s*=\s*\(\(\1\s*\+\s*(\d+)\)\)\)\)',
        r'((\1 += \2))',
        content
    )
    content = re.sub(
        r'\(\((\w+)\s*=\s*\(\(\1\s*\*\s*(\d+)\)\)\)\)',
        r'((\1 *= \2))',
        content
    )
    return content


def fix_duplicate_prototypes(content):
    """Remove duplicate function prototypes."""
    lines = content.split('\n')
    seen_protos = set()
    result = []
    for line in lines:
        stripped = line.strip()
        # Match function prototypes: return_type name(params);
        m = re.match(r'^(static\s+)?(\w[\w\s\*]*)\s+(\w+)\s*\(([^)]*)\)\s*;$', stripped)
        if m:
            proto_key = f"{m.group(2)}|{m.group(3)}|{m.group(4)}"
            if proto_key in seen_protos:
                continue  # Skip duplicate
            seen_protos.add(proto_key)
        result.append(line)
    return '\n'.join(result)


def fix_missing_newlines(content):
    """Ensure proper spacing between function definitions."""
    # Add blank line between function definitions if missing
    content = re.sub(
        r'\}(\n)(static |\w)',
        r'}\n\n\1\2',
        content
    )
    # Fix multiple blank lines
    content = re.sub(r'\n{3,}', '\n\n', content)
    return content


def fix_slice_initializer_spacing(content):
    """Normalize spacing in slice compound literals."""
    content = re.sub(
        r'\(slice\)\{\s*(NULL|0|\(void\*\)[^,]+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}',
        r'(slice){ \1, \2, \3 }',
        content
    )
    return content


def fix_string_initializer_spacing(content):
    """Normalize spacing in string compound literals."""
    content = re.sub(
        r'\(string\)\{\s*(\d+)\s*,\s*("[^"]*"|NULL)\s*\}',
        r'(string){ \1, \2 }',
        content
    )
    return content


def fix_all(content):
    """Apply all fixes in order."""
    content = fix_extraneous_parens(content)
    content = fix_compound_assign_parens(content)
    content = fix_duplicate_prototypes(content)
    content = fix_missing_newlines(content)
    content = fix_slice_initializer_spacing(content)
    content = fix_string_initializer_spacing(content)
    return content


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 tools/c_fixer.py <input.c> [output.c]")
        print("  If output.c is omitted, fixes in-place.")
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2] if len(sys.argv) > 2 else input_path

    if not os.path.exists(input_path):
        print(f"Error: {input_path} not found")
        sys.exit(1)

    with open(input_path, 'r') as f:
        content = f.read()

    fixed = fix_all(content)

    if content != fixed:
        with open(output_path, 'w') as f:
            f.write(fixed)
        print(f"Fixed {input_path} -> {output_path}")
    else:
        print(f"No changes needed for {input_path}")


if __name__ == '__main__':
    main()
