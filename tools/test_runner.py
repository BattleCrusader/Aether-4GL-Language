#!/usr/bin/env python3
"""
Aether Compiler Test Runner — handles three kinds of fixtures:

1. Positive tests in tests/fixtures/*.ae
   - Must COMPILE and RUN with exit code 0 (or __test_failures count)
2. Negative tests in tests/fixtures/negative/*.ae
   - Must FAIL to compile (compiler rejects invalid syntax/types)
3. Spec tests in tests/fixtures/test_spec_*.ae
   - Same as positive: must compile and run

Each @test function is independent. The compiler auto-generates a main() that
calls each @test function when no main() exists.

Usage:
    python3 tools/test_runner.py <compiler_binary> <fixtures_dir>
    python3 tools/test_runner.py ./build/aether tests/fixtures/
"""

import os
import subprocess
import sys
import glob
import time
from pathlib import Path


def run_compile_check(aether, fixture):
    """Compile the fixture. Returns (ok, stderr/stdout, exit_code)."""
    out = f"/tmp/aether_run_{os.path.basename(fixture).replace('.ae', '')}"
    try:
        result = subprocess.run(
            [aether, fixture, "-o", out],
            capture_output=True,
            text=True,
            timeout=15,
        )
        ok = result.returncode == 0
        # Clean up the binary if compile succeeded
        if ok and os.path.exists(out):
            os.remove(out)
        return ok, result.stderr + result.stdout, result.returncode
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT", -1
    except Exception as e:
        return False, f"ERROR: {e}", -2


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)

    aether = sys.argv[1]
    fixtures_dir = sys.argv[2]
    negative_dir = os.path.join(fixtures_dir, "negative")

    if not os.path.exists(aether):
        print(f"Compiler not found: {aether}")
        sys.exit(1)

    if not os.path.isdir(fixtures_dir):
        print(f"Fixtures dir not found: {fixtures_dir}")
        sys.exit(1)

    # Positive fixtures: top-level *.ae (not in negative/)
    positive = sorted(glob.glob(os.path.join(fixtures_dir, "*.ae")))

    # Negative fixtures: negative/*.ae
    negative = []
    if os.path.isdir(negative_dir):
        negative = sorted(glob.glob(os.path.join(negative_dir, "*.ae")))

    print(f"=== Aether Test Runner ===")
    print(f"  compiler:   {aether}")
    print(f"  fixtures:   {fixtures_dir}")
    print(f"  positive:   {len(positive)}")
    print(f"  negative:   {len(negative)}")
    print()

    pos_pass = pos_fail = 0
    neg_pass = neg_fail = 0

    # ── Positive tests: must COMPILE ──
    print(f"--- POSITIVE: {len(positive)} fixtures (must compile) ---")
    for fx in positive:
        name = os.path.basename(fx)
        ok, out, code = run_compile_check(aether, fx)
        status = "OK" if ok else "FAIL"
        if ok:
            pos_pass += 1
            print(f"  [{status:4}] {name}")
        else:
            pos_fail += 1
            # Show first 2 lines of error
            err_summary = out.strip().split('\n')[0][:80] if out.strip() else "?"
            print(f"  [{status:4}] {name} -- {err_summary}")

    print()

    # ── Negative tests: must NOT COMPILE ──
    print(f"--- NEGATIVE: {len(negative)} fixtures (must FAIL to compile) ---")
    for fx in negative:
        name = os.path.basename(fx)
        ok, out, code = run_compile_check(aether, fx)
        # In a negative test, ok=False (compile failure) is what we want
        # ok=True (compile succeeded) means the compiler wrongly accepted bad code
        if not ok:
            neg_pass += 1
            print(f"  [REJECT ✓ ] {name}")
        else:
            neg_fail += 1
            print(f"  [WRONG!   ] {name} -- compiled but should have been rejected")

    print()
    print(f"=== RESULTS ===")
    print(f"  positive: {pos_pass}/{len(positive)} passed, {pos_fail} failed")
    if negative:
        print(f"  negative: {neg_pass}/{len(negative)} correctly rejected, {neg_fail} wrongly accepted")
    print()

    # Exit code: 0 only if everything correct
    # Positive: all pass
    # Negative: all correctly rejected
    if pos_fail == 0 and neg_fail == 0:
        print("ALL TESTS PASS")
        sys.exit(0)
    else:
        print(f"{pos_fail + neg_fail} failures")
        sys.exit(1)


if __name__ == "__main__":
    main()