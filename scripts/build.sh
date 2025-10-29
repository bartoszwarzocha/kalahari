#!/bin/bash
# =============================================================================
# Kalahari Universal Build Script
# =============================================================================
# Cross-platform build wrapper that auto-detects OS and calls appropriate
# platform-specific build script.
#
# Supports: Linux, macOS, Windows (via WSL/Git Bash)
#
# Usage:
#   ./scripts/build.sh              # Debug build
#   ./scripts/build.sh --release    # Release build
#   ./scripts/build.sh --clean      # Clean + rebuild
#   ./scripts/build.sh --test       # Build + run tests
#   ./scripts/build.sh --help       # Show help
#
# =============================================================================

set -e  # Exit on error

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'  # No Color

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# =============================================================================
# Function: Detect operating system
# =============================================================================
detect_os() {
    local OS_TYPE=""

    case "$(uname -s)" in
        Linux*)
            OS_TYPE="Linux"
            ;;
        Darwin*)
            OS_TYPE="macOS"
            ;;
        CYGWIN*|MINGW*|MSYS*)
            OS_TYPE="Windows"
            ;;
        *)
            echo -e "${RED}[ERROR]${NC} Unsupported operating system: $(uname -s)"
            echo "Supported: Linux, macOS, Windows (WSL/Git Bash)"
            exit 1
            ;;
    esac

    echo "$OS_TYPE"
}

# =============================================================================
# Function: Show help
# =============================================================================
show_help() {
    cat << EOF
Kalahari Universal Build Script

This script auto-detects your operating system and calls the appropriate
platform-specific build script:
  - Linux:   build_linux.sh
  - macOS:   build_macos.sh
  - Windows: (use build_windows.bat directly)

Usage: $0 [OPTIONS]

Options:
  --release, -r     Build in Release mode (default: Debug)
  --clean, -c       Clean build directory before building
  --test, -t        Run tests after building
  --verbose, -v     Enable verbose build output
  --help, -h        Show this help message

Platform-Specific Options:
  macOS:
    --universal, -u   Build Universal Binary (Intel + Apple Silicon)

Examples:
  $0                      # Debug build
  $0 --release            # Release build
  $0 --clean --release    # Clean + Release build
  $0 --test               # Debug build + run tests
  $0 -r -t                # Release build + tests (short flags)

For more information, see BUILDING.md
EOF
}

# =============================================================================
# Main execution
# =============================================================================
main() {
    # Check for help flag first
    if [[ "$1" == "--help" || "$1" == "-h" ]]; then
        show_help
        exit 0
    fi

    # Detect OS
    OS="$(detect_os)"

    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}Kalahari Universal Build Script${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    echo -e "Detected OS: ${GREEN}$OS${NC}"
    echo ""

    # Call platform-specific script
    case "$OS" in
        Linux)
            echo -e "${BLUE}[INFO]${NC} Calling build_linux.sh..."
            exec "$SCRIPT_DIR/build_linux.sh" "$@"
            ;;
        macOS)
            echo -e "${BLUE}[INFO]${NC} Calling build_macos.sh..."
            exec "$SCRIPT_DIR/build_macos.sh" "$@"
            ;;
        Windows)
            echo -e "${YELLOW}[WARNING]${NC} Windows detected (WSL/Git Bash)"
            echo ""
            echo "For best results on Windows, use:"
            echo "  scripts\\build_windows.bat"
            echo ""
            echo "Or run WSL build with:"
            echo "  $SCRIPT_DIR/build_linux.sh $*"
            echo ""
            read -p "Continue with Linux build in WSL? (y/n) " -n 1 -r
            echo
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                exec "$SCRIPT_DIR/build_linux.sh" "$@"
            else
                echo "Aborted. Use scripts\\build_windows.bat instead."
                exit 1
            fi
            ;;
        *)
            echo -e "${RED}[ERROR]${NC} Unknown OS: $OS"
            exit 1
            ;;
    esac
}

main "$@"
