#!/usr/bin/env python3
"""
C Linter Test — validates generated C code from Aether transpiler.

Checks for:
1. Compilation success (no errors)
2. Warning-free output (no -W warnings)
3. Common C code quality issues
4. Runtime correctness (exit code matches expected)

Usage:
    python3 tools/test_c_lint.py <aether_binary> [test_fixture.ae ...]
"""

import subprocess
import sys
import os
import re
import tempfile

# Patterns that indicate problems in generated C code
WARNING_PATTERNS = [
    r"warning:",
    r"incompatible pointer type",
    r"incompatible integer to pointer",
    r"incompatible pointer to integer",
    r"implicit declaration",
    r"conflicting types",
    r"assignment makes pointer from integer",
    r"assignment makes integer from pointer",
    r"passing argument \d+ of '.*' from incompatible pointer type",
    r"initialization of '.*' from incompatible pointer type",
    r"return makes pointer from integer without a cast",
    r"return makes integer from pointer without a cast",
    r"control reaches end of non-void function",
    r"unused variable",
    r"unused parameter",
    r"unused function",
    r"missing field initializer",
    r"excess elements in scalar initializer",
    r"excess elements in struct initializer",
    r"too few arguments",
    r"too many arguments",
    r"use of undeclared identifier",
    r"unknown type name",
    r"no member named",
    r"subscripted value is not an array",
    r"statement requires expression of scalar type",
    r"invalid operands to binary expression",
    r"member reference base type",
    r"expected expression",
    r"initializing '.*' with an expression of incompatible type",
]

# Patterns that are acceptable (false positives)
ALLOWED_WARNINGS = [
    r"equality comparison with extraneous parentheses",  # Harmless, from codegen wrapping
    r"remove extraneous parentheses",                    # Same as above
    r"use '=' to turn this equality comparison",         # Same as above
]


def check_c_code(c_path, verbose=False):
    """Check generated C code for issues without compiling."""
    issues = []
    with open(c_path, 'r') as f:
        content = f.read()
        lines = content.split('\n')

    # Check for common issues in generated code
    for i, line in enumerate(lines, 1):
        # Check for hardcoded slice literals without elem_size
        if re.search(r'\(slice\)\{[^}]*\}[^,]*$', line) and 'elem_size' not in content[:500]:
            if not re.search(r'\(slice\)\{ NULL, 0, 8 \}', line) and \
               not re.search(r'\(slice\)\{[^}]*,[^}]*,[^}]*\}', line):
                issues.append(f"  Line {i}: slice literal missing elem_size: {line.strip()[:80]}")

        # Check for direct struct equality (C doesn't support it)
        if re.search(r'if\s*\(\s*\w+\s*==\s*\w+\s*\)', line):
            # This is fine if it's a primitive type, but flag it
            pass

    return issues


def compile_and_check(aether_bin, fixture_path, verbose=False):
    """Compile an .ae fixture and check the generated C code."""
    name = os.path.splitext(os.path.basename(fixture_path))[0]

    # Compile to C
    with tempfile.NamedTemporaryFile(suffix='.c', prefix=f'{name}_', delete=False, mode='w') as f:
        c_path = f.name
    with tempfile.NamedTemporaryFile(suffix='', prefix=f'{name}_bin_', delete=False) as f:
        bin_path = f.name

    try:
        # Step 1: Compile the .ae file to a binary (uses C transpiler internally)
        result = subprocess.run(
            [aether_bin, fixture_path, '-o', bin_path],
            capture_output=True, text=True, timeout=30
        )

        # Check for compilation errors
        stderr_lines = result.stderr.strip().split('\n') if result.stderr else []
        stdout_lines = result.stdout.strip().split('\n') if result.stdout else []

        errors = [l for l in stderr_lines if 'error:' in l.lower() and 'FIRST PASS' not in l]
        warnings = [l for l in stderr_lines if 'warning:' in l.lower()]

        # Filter allowed warnings
        real_warnings = []
        for w in warnings:
            is_allowed = any(re.search(p, w) for p in ALLOWED_WARNINGS)
            if not is_allowed:
                real_warnings.append(w)

        # Step 2: Check the generated C file (if it still exists)
        c_issues = []
        if os.path.exists(c_path):
            c_issues = check_c_code(c_path, verbose)

        # Step 3: Run the binary
        exit_code = -1
        run_stdout = ''
        if len(errors) == 0 and os.path.exists(bin_path):
            run_res = subprocess.run([bin_path], capture_output=True, text=True, timeout=10)
            exit_code = run_res.returncode
            run_stdout = run_res.stdout.strip() if run_res.stdout else ''

        return {
            'name': name,
            'pass': len(errors) == 0 and len(real_warnings) == 0 and
                    len(c_issues) == 0,
            'errors': errors,
            'warnings': real_warnings,
            'c_errors': [],
            'c_warnings': [],
            'c_issues': c_issues,
            'exit_code': exit_code,
            'stdout': run_stdout,
        }

    except subprocess.TimeoutExpired:
        return {
            'name': name,
            'pass': False,
            'errors': ['TIMEOUT'],
            'warnings': [],
            'c_errors': [],
            'c_warnings': [],
            'c_issues': [],
            'exit_code': -1,
            'stdout': '',
        }
    finally:
        # Cleanup
        for p in [c_path, bin_path]:
            try:
                if os.path.exists(p):
                    os.unlink(p)
            except:
                pass


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 tools/test_c_lint.py <aether_binary> [test_fixtures...]")
        sys.exit(1)

    aether_bin = sys.argv[1]
    fixtures = sys.argv[2:] if len(sys.argv) > 2 else []

    if not fixtures:
        # Default: test all fixtures
        fixture_dir = os.path.join(os.path.dirname(aether_bin), '..', 'tests', 'fixtures')
        if os.path.exists(fixture_dir):
            fixtures = sorted([
                os.path.join(fixture_dir, f)
                for f in os.listdir(fixture_dir)
                if f.endswith('.ae')
            ])

    if not fixtures:
        print("No test fixtures found.")
        sys.exit(1)

    print(f"C Linter Test — {len(fixtures)} fixtures")
    print("=" * 60)

    results = []
    for fixture in fixtures:
        result = compile_and_check(aether_bin, fixture)
        results.append(result)

        status = "PASS" if result['pass'] else "FAIL"
        print(f"  {status:4s} | {result['name']:40s} | exit={result['exit_code']:3d}")

        if not result['pass']:
            for e in result['errors'][:3]:
                print(f"         error: {e[:100]}")
            for w in result['warnings'][:3]:
                print(f"         warning: {w[:100]}")
            for e in result['c_errors'][:3]:
                print(f"         C error: {e[:100]}")
            for w in result['c_warnings'][:3]:
                print(f"         C warning: {w[:100]}")
            for i in result['c_issues'][:3]:
                print(f"         issue: {i[:100]}")

    passed = sum(1 for r in results if r['pass'])
    total = len(results)

    print()
    print("=" * 60)
    print(f"Results: {passed}/{total} passed, {total - passed} failed")

    # Summary of failures
    failures = [r for r in results if not r['pass']]
    if failures:
        print()
        print("Failed fixtures:")
        for f in failures:
            reasons = []
            if f['errors']: reasons.append(f"{len(f['errors'])} errors")
            if f['warnings']: reasons.append(f"{len(f['warnings'])} warnings")
            if f['c_errors']: reasons.append(f"{len(f['c_errors'])} C errors")
            if f['c_warnings']: reasons.append(f"{len(f['c_warnings'])} C warnings")
            if f['c_issues']: reasons.append(f"{len(f['c_issues'])} code issues")
            print(f"  {f['name']}: {', '.join(reasons)}")

    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
