#!/usr/bin/env python3
"""test_host.py — Aether Host-Native Test Runner

For each .ae fixture:
  1. Compile with `aether --target host`
  2. If @test is on func main → run binary, check exit code
  3. If @test is on other functions (no main) → run binary with each
     func name as argv[1], check exit code against @test(expect=N)

Usage: python3 tools/test_host.py <aether_binary> <fixture1.ae> [fixture2.ae ...]
"""

import subprocess
import os
import re
import sys

def extract_tests(filepath):
    """Extract @test(expect=N) annotations paired with their func names."""
    with open(filepath, 'r') as f:
        lines = f.readlines()

    tests = []
    for i, line in enumerate(lines):
        m = re.search(r'@test\(expect=(\d+)\)', line)
        if not m:
            continue
        expect = int(m.group(1))
        # Look for the func line (could be same line or next non-empty line)
        func_name = None
        # Check if func is on the same line
        fm = re.search(r'func\s+([a-zA-Z_][a-zA-Z0-9_]*)', line)
        if fm:
            func_name = fm.group(1)
        else:
            # Look at next few lines for func
            for j in range(i + 1, min(i + 3, len(lines))):
                fm = re.search(r'func\s+([a-zA-Z_][a-zA-Z0-9_]*)', lines[j])
                if fm:
                    func_name = fm.group(1)
                    break
        if func_name:
            tests.append((func_name, expect))
    return tests

def main():
    if len(sys.argv) < 3:
        print("Usage: test_host.py <aether_binary> <fixture1.ae> [fixture2.ae ...]")
        sys.exit(1)

    aether_bin = sys.argv[1]
    fixtures = sys.argv[2:]

    total = 0
    passed = 0

    for fixture in fixtures:
        name = os.path.basename(fixture).replace('.ae', '')
        binary = f"/tmp/kernel/{name}"

        # Compile
        result = subprocess.run(
            [aether_bin, '--target', 'host', fixture],
            capture_output=True, text=True
        )
        if result.returncode != 0:
            total += 1
            print(f"  TEST: {name} ... FAIL (compile)")
            continue

        tests = extract_tests(fixture)
        if not tests:
            total += 1
            print(f"  TEST: {name} ... FAIL (no @test annotation)")
            continue

        # Check if any test is on main
        main_tests = [(fn, exp) for fn, exp in tests if fn == 'main']
        if main_tests:
            # Single-run mode: @test on main
            func_name, expect = main_tests[0]
            total += 1
            print(f"  TEST: {name} ... ", end="")
            proc = subprocess.run([binary], capture_output=True)
            got = proc.returncode
            if got == expect:
                print(f"PASS (exit {got})")
                passed += 1
            else:
                print(f"FAIL (expected {expect}, got {got})")
        else:
            # Dispatcher mode: @test on non-main functions
            for func_name, expect in tests:
                total += 1
                print(f"  TEST: {name}.{func_name} ... ", end="")
                proc = subprocess.run([binary, func_name], capture_output=True)
                got = proc.returncode
                if got == expect:
                    print(f"PASS (exit {got})")
                    passed += 1
                else:
                    print(f"FAIL (expected {expect}, got {got})")

    print()
    failed = total - passed
    print(f"=== Results: {passed}/{total} passed, {failed} failed ===")
    sys.exit(0 if passed == total else 1)

if __name__ == '__main__':
    main()