#!/bin/bash
# =============================================================================
# Kalahari Clean Script
# =============================================================================
# Removes build artifacts to force clean rebuild
# Cross-platform (Linux, macOS, Windows WSL)
#
# Usage:
#   ./scripts/clean.sh              # Remove build/ directory
#   ./scripts/clean.sh --full       # Also remove vcpkg_installed/
#   ./scripts/clean.sh --help       # Show help
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

FULL_CLEAN=false

# =============================================================================
# Parse arguments
# =============================================================================
while [[ $# -gt 0 ]]; do
    case $1 in
        --full|-f)
            FULL_CLEAN=true
            shift
            ;;
        --help|-h)
            cat << EOF
Kalahari Clean Script

Removes build artifacts to force clean rebuild.

Usage: $0 [OPTIONS]

Options:
  --full, -f      Full clean (also remove vcpkg_installed/ cache)
  --help, -h      Show this help message

Examples:
  $0              # Remove all build-* directories
  $0 --full       # Remove build-* + vcpkg_installed/

Note: Full clean will require re-downloading dependencies (~15-30 min)
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
echo -e "${BLUE}Kalahari Clean Script${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Clean build directories (all platforms)
FOUND_BUILDS=false

for dir in build-windows build-linux build-macos; do
    if [ -d "$dir" ]; then
        echo -e "${BLUE}[INFO]${NC} Removing $dir/ directory..."
        rm -rf "$dir/"
        echo -e "${GREEN}[✓]${NC} $dir/ removed"
        FOUND_BUILDS=true
    fi
done

if [ "$FOUND_BUILDS" = false ]; then
    echo -e "${YELLOW}[!]${NC} No build directories found (already clean)"
fi

# Full clean (remove vcpkg cache)
if [ "$FULL_CLEAN" = true ]; then
    if [ -d "vcpkg_installed" ]; then
        echo -e "${BLUE}[INFO]${NC} Removing vcpkg_installed/ cache..."
        echo -e "${YELLOW}[!]${NC} This will require re-downloading dependencies"
        rm -rf vcpkg_installed/
        echo -e "${GREEN}[✓]${NC} vcpkg_installed/ removed"
    else
        echo -e "${YELLOW}[!]${NC} vcpkg_installed/ not found"
    fi
fi

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}✓ Clean Completed${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "To rebuild, run:"
echo -e "  ${BLUE}./scripts/build.sh${NC}"
echo ""
