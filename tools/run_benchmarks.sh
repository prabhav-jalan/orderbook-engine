#!/bin/bash
# tools/run_benchmarks.sh
#
# Builds and runs the benchmark suite.
#
# Usage:
#   ./tools/run_benchmarks.sh                # Default: 500K ops
#   ./tools/run_benchmarks.sh 1000000        # Custom op count

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build-bench"

NUM_OPS="${1:-500000}"

echo "=== Order Book Engine — Benchmark Runner ==="
echo ""

# Build in Release mode with benchmarks enabled
echo "Building in Release mode..."
cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF \
    -DBUILD_BENCHMARKS=ON \
    -S "$PROJECT_DIR" \
    > /dev/null 2>&1

cmake --build "$BUILD_DIR" --parallel "$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)" \
    > /dev/null 2>&1

echo "Build complete."
echo ""

BENCH_BIN="$BUILD_DIR/benchmarks/orderbook_bench"

if [ ! -f "$BENCH_BIN" ]; then
    echo "ERROR: Benchmark binary not found at $BENCH_BIN"
    exit 1
fi

# System info
echo "System info:"
echo "  OS:       $(uname -s) ($(uname -m))"
echo "  Compiler: $(c++ --version 2>/dev/null | head -1 || echo 'unknown')"
echo "  Date:     $(date -u +'%Y-%m-%d %H:%M:%S UTC')"
echo ""

# Run
echo "Running benchmarks ($NUM_OPS operations per test)..."
echo ""
"$BENCH_BIN" "$NUM_OPS"

echo ""
echo "Done. Copy the summary table above into benchmarks/results/baseline.md"
