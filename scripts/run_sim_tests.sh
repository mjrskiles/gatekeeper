#!/bin/bash
#
# Run all simulator integration tests (.gks scripts)
#
# Usage:
#   ./scripts/run_sim_tests.sh [options]
#
# Options:
#   -v, --verbose   Show full test output
#   -h, --help      Show this help message
#
# Exit code: 0 if all tests pass, 1 if any test fails
#
# This script is designed for CI integration and can be run after
# the simulator is built with: cmake --preset sim && cmake --build --preset sim

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SIM_BIN="$PROJECT_ROOT/build/sim/sim/gatekeeper-sim"
SCRIPTS_DIR="$PROJECT_ROOT/sim/scripts"

VERBOSE=0

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -h|--help)
            head -16 "$0" | tail -14
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Check if simulator exists
if [[ ! -x "$SIM_BIN" ]]; then
    echo "ERROR: Simulator not found at $SIM_BIN"
    echo "Build it with: cmake --preset sim && cmake --build --preset sim"
    exit 1
fi

# Get list of test scripts
mapfile -t SCRIPTS < <(find "$SCRIPTS_DIR" -name "*.gks" -type f | sort)
SCRIPT_COUNT=${#SCRIPTS[@]}

if [[ "$SCRIPT_COUNT" -eq 0 ]]; then
    echo "No test scripts found in $SCRIPTS_DIR"
    exit 1
fi

echo "========================================"
echo "Gatekeeper Simulator Integration Tests"
echo "========================================"
echo ""
echo "Found $SCRIPT_COUNT test script(s)"
echo ""

PASSED=0
FAILED=0
FAILED_TESTS=()

for script in "${SCRIPTS[@]}"; do
    name=$(basename "$script" .gks)
    output=$("$SIM_BIN" --script "$script" --batch 2>&1)

    if echo "$output" | grep -q "Script completed successfully"; then
        printf "Running %-35s ... PASS\n" "$name"
        ((PASSED++))
    else
        printf "Running %-35s ... FAIL\n" "$name"
        ((FAILED++))
        FAILED_TESTS+=("$name")
        if [[ "$VERBOSE" -eq 1 ]]; then
            echo "--- Output ---"
            echo "$output"
            echo "--------------"
        fi
    fi
done

echo ""
echo "========================================"
echo "Results: $PASSED passed, $FAILED failed"
echo "========================================"

if [[ "$FAILED" -gt 0 ]]; then
    echo ""
    echo "Failed tests:"
    for name in "${FAILED_TESTS[@]}"; do
        echo "  - $name"
    done
    echo ""
    echo "Run with -v for verbose output"
    exit 1
fi

echo ""
echo "All tests passed!"
exit 0
