#!/bin/bash
# test_host.sh — Aether Host-Native Test Runner
#
# For each .ae fixture:
#   1. Compile with `aether --target host`
#   2. If @test is on func main → run binary, check exit code
#   3. If @test is on other functions (no main) → run binary with each func name as arg,
#      check exit code against @test(expect=N)
#
# Usage: ./tools/test_host.sh <aether_binary> <fixture1.ae> [fixture2.ae ...]

AETHER_BIN="$1"
shift

total=0
pass=0

for fixture in "$@"; do
    name=$(basename "$fixture" .ae)

    # Compile
    "$AETHER_BIN" --target host "$fixture" 2>/dev/null >/dev/null
    if [ $? -ne 0 ]; then
        total=$((total + 1))
        printf "  TEST: %s ... FAIL (compile)\n" "$name"
        continue
    fi

    # Check if @test is on func main
    main_expect=$(grep -B1 'func main' "$fixture" | grep -o '@test(expect=[0-9]*)' | grep -o '[0-9]*' | head -1)
    if [ -n "$main_expect" ]; then
        # Single-run mode: @test on main
        total=$((total + 1))
        printf "  TEST: %s ... " "$name"
        /tmp/kernel/$name 2>/dev/null >/dev/null
        got=$?
        if [ "$got" = "$main_expect" ]; then
            printf "PASS (exit %d)\n" "$got"
            pass=$((pass + 1))
        else
            printf "FAIL (expected %d, got %d)\n" "$main_expect" "$got"
        fi
        continue
    fi

    # Dispatcher mode: @test on non-main functions
    # Extract pairs of (@test expect value, function name)
    test_count=$(grep -c '@test(expect=' "$fixture" 2>/dev/null || echo 0)
    if [ "$test_count" = "0" ]; then
        total=$((total + 1))
        printf "  TEST: %s ... FAIL (no @test annotation)\n" "$name"
        continue
    fi

    # Parse @test annotations and the func line below each
    # Use awk to extract: expect_value, func_name pairs
    eval "$(awk '
        /@test\(expect=/ {
            match($0, /expect=([0-9]+)/, arr)
            expect = arr[1]
            getline
            if ($0 ~ /func /) {
                match($0, /func[[:space:]]+([a-zA-Z_][a-zA-Z0-9_]*)/, arr2)
                funcname = arr2[1]
                if (funcname != "main" && funcname != "") {
                    printf "test_pairs=\"%s%s %s%s %s%s\" ", test_pairs, expect, "|", funcname, "|", ""
                }
            }
        }
    ' "$fixture")"

    # If no pairs found, try the old approach (maybe @test is on main only but grep missed it)
    if [ -z "$test_pairs" ]; then
        total=$((total + 1))
        printf "  TEST: %s ... FAIL (no @test pairs found)\n" "$name"
        continue
    fi

    # Split test_pairs into individual tests
    IFS='|' read -ra pairs <<< "$test_pairs"
    for pair in "${pairs[@]}"; do
        pair=$(echo "$pair" | xargs)  # trim whitespace
        [ -z "$pair" ] && continue
        expect=$(echo "$pair" | awk '{print $1}')
        funcname=$(echo "$pair" | awk '{print $2}')
        [ -z "$funcname" ] && continue

        total=$((total + 1))
        printf "  TEST: %s.%s ... " "$name" "$funcname"
        /tmp/kernel/$name "$funcname" 2>/dev/null >/dev/null
        got=$?
        if [ "$got" = "$expect" ]; then
            printf "PASS (exit %d)\n" "$got"
            pass=$((pass + 1))
        else
            printf "FAIL (expected %d, got %d)\n" "$expect" "$got"
        fi
    done
done

echo ""
echo "=== Results: $pass/$total passed, $((total - pass)) failed ==="
[ $pass -eq $total ]