#!/usr/bin/env python3
"""test_host.py — Aether Host-Native Test Runner

For each .ae fixture:
  1. Compile with `aether --target host`
  2. Run the binary
  3. Exit code 0 = pass, > 0 = failure count

@test functions return void. The auto-generated dispatcher calls all
@test functions and returns __test_failures as the exit code.

Usage: python3 tools/test_host.py <aether_binary> <fixture1.ae> [fixture2.ae ...]
"""

import subprocess
import os
import sys


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

        # Run the binary
        total += 1
        print(f"  TEST: {name} ... ", end="")
        proc = subprocess.run([binary], capture_output=True)
        got = proc.returncode
        if got == 0:
            print("PASS")
            passed += 1
        else:
            print(f"FAIL (exit {got})")

    print()
    failed = total - passed
    print(f"=== Results: {passed}/{total} passed, {failed} failed ===")
    sys.exit(0 if passed == total else 1)


if __name__ == '__main__':
    main()
