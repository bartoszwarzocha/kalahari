#!/bin/bash
# =============================================================================
# Kalahari Development Environment Setup
# =============================================================================
# Installs system dependencies required to build Kalahari
# Supports: Ubuntu/Debian, Fedora/RHEL, macOS
#
# Usage:
#   ./scripts/setup_dev_env.sh              # Auto-detect distro
#   ./scripts/setup_dev_env.sh --dry-run    # Show commands without executing
#   ./scripts/setup_dev_env.sh --help       # Show help
#
# =============================================================================

set -e  # Exit on error

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

DRY_RUN=false

# =============================================================================
# Parse arguments
# =============================================================================
while [[ $# -gt 0 ]]; do
    case $1 in
        --dry-run|-n)
            DRY_RUN=true
            shift
            ;;
        --help|-h)
            cat << EOF
Kalahari Development Environment Setup

Installs system dependencies required to build Kalahari.

Supported platforms:
  - Ubuntu 20.04+ / Debian 11+
  - Fedora 36+ / RHEL 8+
  - macOS 11+ (via Homebrew)

Usage: $0 [OPTIONS]

Options:
  --dry-run, -n      Show commands without executing
  --help, -h         Show this help message

Examples:
  $0                 # Install dependencies
  $0 --dry-run       # Preview what would be installed

Manual installation instructions: BUILDING.md
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
# Detect OS and distribution
# =============================================================================
detect_platform() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        if [ -f /etc/os-release ]; then
            . /etc/os-release
            echo "$ID"
        else
            echo "unknown-linux"
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    else
        echo "unknown"
    fi
}

# =============================================================================
# Execute or print command
# =============================================================================
run_cmd() {
    if [ "$DRY_RUN" = true ]; then
        echo -e "${YELLOW}[DRY-RUN]${NC} $*"
    else
        echo -e "${BLUE}[RUN]${NC} $*"
        eval "$@"
    fi
}

# =============================================================================
# Install Ubuntu/Debian dependencies
# =============================================================================
install_ubuntu_debian() {
    echo -e "${BLUE}[INFO]${NC} Installing dependencies for Ubuntu/Debian..."
    echo ""

    # Update package list
    run_cmd "sudo apt update"

    # Build tools
    echo -e "${BLUE}[INFO]${NC} Installing build tools..."
    run_cmd "sudo apt install -y build-essential cmake ninja-build git pkg-config"

    # wxWidgets dependencies (GTK3, X11, media libraries)
    echo -e "${BLUE}[INFO]${NC} Installing wxWidgets dependencies..."
    run_cmd "sudo apt install -y \
        libgtk-3-dev \
        libx11-dev libxext-dev libxrandr-dev libxrender-dev \
        libxi-dev libxfixes-dev libxtst-dev \
        libglu1-mesa-dev \
        libpng-dev libjpeg-dev libtiff-dev libwebp-dev \
        libcurl4-openssl-dev \
        libnotify-dev libsecret-1-dev \
        libsdl2-dev \
        liblzma-dev libbz2-dev libzip-dev zlib1g-dev"

    echo ""
    echo -e "${GREEN}[✓]${NC} Ubuntu/Debian dependencies installed"
}

# =============================================================================
# Install Fedora/RHEL dependencies
# =============================================================================
install_fedora_rhel() {
    echo -e "${BLUE}[INFO]${NC} Installing dependencies for Fedora/RHEL..."
    echo ""

    # Build tools
    echo -e "${BLUE}[INFO]${NC} Installing build tools..."
    run_cmd "sudo dnf install -y gcc-c++ cmake ninja-build git pkg-config"

    # wxWidgets dependencies
    echo -e "${BLUE}[INFO]${NC} Installing wxWidgets dependencies..."
    run_cmd "sudo dnf install -y \
        gtk3-devel \
        libX11-devel libXext-devel libXrandr-devel libXrender-devel \
        libXi-devel libXfixes-devel libXtst-devel \
        mesa-libGLU-devel \
        libpng-devel libjpeg-turbo-devel libtiff-devel libwebp-devel \
        libcurl-devel \
        libnotify-devel libsecret-devel \
        SDL2-devel \
        xz-devel bzip2-devel libzip-devel zlib-devel"

    echo ""
    echo -e "${GREEN}[✓]${NC} Fedora/RHEL dependencies installed"
}

# =============================================================================
# Install macOS dependencies
# =============================================================================
install_macos() {
    echo -e "${BLUE}[INFO]${NC} Installing dependencies for macOS..."
    echo ""

    # Check Homebrew
    if ! command -v brew &> /dev/null; then
        echo -e "${RED}[ERROR]${NC} Homebrew not found!"
        echo ""
        echo "Install Homebrew first:"
        echo "  /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
        exit 1
    fi

    # Check Xcode Command Line Tools
    if ! xcode-select -p &> /dev/null; then
        echo -e "${YELLOW}[!]${NC} Xcode Command Line Tools not installed"
        echo "Installing..."
        run_cmd "xcode-select --install"
        echo ""
        echo "Follow the GUI prompts, then re-run this script"
        exit 0
    fi

    # Install build tools
    echo -e "${BLUE}[INFO]${NC} Installing build tools via Homebrew..."
    run_cmd "brew install cmake ninja pkg-config git"

    echo ""
    echo -e "${GREEN}[✓]${NC} macOS dependencies installed"
    echo ""
    echo -e "${BLUE}[INFO]${NC} wxWidgets will be built by vcpkg (no extra packages needed)"
}

# =============================================================================
# Main execution
# =============================================================================
main() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}Kalahari Dev Environment Setup${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""

    if [ "$DRY_RUN" = true ]; then
        echo -e "${YELLOW}[DRY-RUN MODE]${NC} Commands will be shown but not executed"
        echo ""
    fi

    PLATFORM="$(detect_platform)"
    echo -e "Detected platform: ${GREEN}$PLATFORM${NC}"
    echo ""

    case "$PLATFORM" in
        ubuntu|debian|linuxmint|pop)
            install_ubuntu_debian
            ;;
        fedora|rhel|centos)
            install_fedora_rhel
            ;;
        macos)
            install_macos
            ;;
        *)
            echo -e "${RED}[ERROR]${NC} Unsupported platform: $PLATFORM"
            echo ""
            echo "Supported platforms:"
            echo "  - Ubuntu 20.04+ / Debian 11+"
            echo "  - Fedora 36+ / RHEL 8+"
            echo "  - macOS 11+"
            echo ""
            echo "See BUILDING.md for manual installation instructions"
            exit 1
            ;;
    esac

    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${GREEN}✓ Setup Complete${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    echo "Next steps:"
    echo -e "  1. Clone repository: ${BLUE}git clone https://github.com/bartoszwarzocha/kalahari.git${NC}"
    echo -e "  2. Build project:    ${BLUE}cd kalahari && ./scripts/build.sh${NC}"
    echo ""
    echo "For detailed instructions, see: BUILDING.md"
    echo ""
}

main
