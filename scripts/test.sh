#!/bin/bash
# =============================================================================
# Kalahari Test Script
# =============================================================================
# Convenient wrapper for running Catch2 tests via ctest
# Cross-platform (Linux, macOS, Windows WSL)
#
# Usage:
#   ./scripts/test.sh                    # Run all tests
#   ./scripts/test.sh --verbose          # Verbose output
#   ./scripts/test.sh --filter "Thread*" # Run specific tests
#   ./scripts/test.sh --help             # Show help
#
# =============================================================================

set -e  # Exit on error

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Get project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

VERBOSE=false
TEST_FILTER=""

# =============================================================================
# Parse arguments
# =============================================================================
while [[ $# -gt 0 ]]; do
    case $1 in
        --verbose|-v)
            VERBOSE=true
            shift
            ;;
        --filter|-f)
            TEST_FILTER="$2"
            shift 2
            ;;
        --help|-h)
            cat << EOF
Kalahari Test Script

Convenient wrapper for running tests via ctest.

Usage: $0 [OPTIONS]

Options:
  --verbose, -v           Show detailed test output
  --filter PATTERN, -f    Run only tests matching pattern
  --help, -h              Show this help message

Examples:
  $0                      # Run all tests
  $0 --verbose            # Run all tests with detailed output
  $0 --filter "Thread*"   # Run tests matching "Thread*"
  $0 -f "GUI*" -v         # Run GUI tests with verbose output

Test executable:
  Location: build-linux/bin/kalahari-tests

Direct ctest usage:
  cd build-linux && ctest --output-on-failure
  cd build-linux && ctest -V  # Very verbose
  cd build-linux && ctest -R "TestName"  # Filter by regex
EOF
            exit 0
            ;;
        *)
            echo -e "${RED}[ERROR]${NC} Unknown option: $1"
            echo "Run '$0 --help' for usage"
            exit 1
            ;;
    esac
done

# =============================================================================
# Main execution
# =============================================================================
cd "$PROJECT_ROOT"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Kalahari Test Runner${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check if build-linux directory exists
if [ ! -d "build-linux" ]; then
    echo -e "${RED}[ERROR]${NC} build-linux/ directory not found"
    echo "Run build-linux first: ./scripts/build-linux.sh"
    exit 1
fi

# Check if test executable exists
if [ ! -f "build-linux/bin/kalahari-tests" ]; then
    echo -e "${RED}[ERROR]${NC} Test executable not found"
    echo "Run build-linux with tests: ./scripts/build-linux.sh --test"
    exit 1
fi

cd build-linux

# Prepare ctest arguments
CTEST_ARGS=("--output-on-failure")

if [ "$VERBOSE" = true ]; then
    CTEST_ARGS+=("-V")
fi

if [ -n "$TEST_FILTER" ]; then
    echo -e "${BLUE}[INFO]${NC} Running tests matching: $TEST_FILTER"
    CTEST_ARGS+=("-R" "$TEST_FILTER")
else
    echo -e "${BLUE}[INFO]${NC} Running all tests..."
fi

echo ""

# Run tests
if ctest "${CTEST_ARGS[@]}"; then
    echo ""
    echo -e "${GREEN}[✓]${NC} All tests passed!"
    exit 0
else
    echo ""
    echo -e "${RED}[✗]${NC} Some tests failed"
    exit 1
fi
