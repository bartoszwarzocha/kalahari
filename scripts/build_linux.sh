#!/bin/bash
# =============================================================================
# Kalahari Build Script (Linux) - VirtualBox Auto-Handling
# =============================================================================
# Automatically handles VirtualBox shared folders by building in local storage
#
# Usage:
#   ./scripts/build_linux.sh              # Debug build
#   ./scripts/build_linux.sh --release    # Release build
#   ./scripts/build_linux.sh --clean      # Clean + rebuild
#   ./scripts/build_linux.sh --test       # Build + run tests
#
# =============================================================================

set -e

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Get project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Default configuration
BUILD_TYPE="Debug"
CLEAN_BUILD=false
RUN_TESTS=false
VERBOSE=false
FORCE_MODE=""  # empty, "vbox", "native"

# =============================================================================
# Helper functions
# =============================================================================
print_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
print_success() { echo -e "${GREEN}[✓]${NC} $1"; }
print_error() { echo -e "${RED}[✗]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[!]${NC} $1"; }

show_help() {
    cat << EOF
Kalahari Build Script (Linux)

Automatically detects shared filesystems (VirtualBox, WSL) and builds in local storage.

Usage: $0 [OPTIONS]

Options:
  --release, -r       Build in Release mode (default: Debug)
  --clean, -c         Clean build directory before building
  --test, -t          Run tests after building
  --verbose, -v       Enable verbose build output
  --force-vbox        Force shared filesystem workflow (rsync + local build)
  --force-native      Force native build (skip auto-detection)
  --help, -h          Show this help message

Examples:
  $0                      # Debug build (auto-detect)
  $0 --release            # Release build
  $0 --clean --release    # Clean + Release build
  $0 --test               # Debug build + run tests
  $0 --force-native       # Force native build (not recommended on shared fs)

Shared Filesystem Support:
  Script automatically detects:
  - VirtualBox shared folders (/media/sf_*, vboxsf filesystem)
  - WSL Windows mounts (/mnt/*, 9p/drvfs filesystem)

  When detected, builds in ~/kalahari-build/ for full symlink support.
  Binaries are copied back to the shared folder automatically.

  Use --force-native to build directly on shared folder (not recommended).
  Use --force-vbox to force local build workflow even on native paths.

Build output:
  Executable: build-linux/bin/kalahari
  Tests:      build-linux/bin/kalahari-tests
EOF
}

# =============================================================================
# Parse arguments
# =============================================================================
while [[ $# -gt 0 ]]; do
    case $1 in
        --release|-r) BUILD_TYPE="Release"; shift ;;
        --clean|-c) CLEAN_BUILD=true; shift ;;
        --test|-t) RUN_TESTS=true; shift ;;
        --verbose|-v) VERBOSE=true; shift ;;
        --force-vbox) FORCE_MODE="vbox"; shift ;;
        --force-native) FORCE_MODE="native"; shift ;;
        --help|-h) show_help; exit 0 ;;
        *) print_error "Unknown option: $1"; echo ""; show_help; exit 1 ;;
    esac
done

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Kalahari Build Script (Linux)${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# =============================================================================
# VirtualBox Detection & Auto-Setup
# =============================================================================
IS_VBOX=false
BUILD_DIR_ACTUAL="$PROJECT_ROOT"

# Detect shared/mounted filesystems with symlink issues
# VirtualBox: /media/sf_* (vboxsf filesystem)
# WSL: /mnt/* (Windows drives via 9p/drvfs)
if [[ "$PROJECT_ROOT" =~ ^/media/sf_ ]]; then
    IS_VBOX=true
    print_info "VirtualBox shared folder detected (path: /media/sf_*)"
elif [[ "$PROJECT_ROOT" =~ ^/mnt/[a-z] ]] && mount | grep -q "type 9p"; then
    IS_VBOX=true  # Reusing same workflow for WSL
    print_info "WSL Windows mount detected (path: /mnt/*, type: 9p/drvfs)"
fi

# Apply force flags if set
if [ "$FORCE_MODE" = "vbox" ]; then
    IS_VBOX=true
    print_warning "VirtualBox workflow FORCED by --force-vbox flag"
elif [ "$FORCE_MODE" = "native" ]; then
    IS_VBOX=false
    print_warning "Native build FORCED by --force-native flag"
fi

# Shared filesystem workflow: rsync to local storage and build there
# (VirtualBox vboxsf or WSL 9p/drvfs - both have symlink issues)
if [ "$IS_VBOX" = true ]; then
    BUILD_DIR_ACTUAL="$HOME/kalahari-build"

    print_warning "Shared filesystem detected (symlink issues during build)"
    print_info "Building in local VM storage: $BUILD_DIR_ACTUAL"
    print_info "Binaries will be copied back automatically"
    echo ""

    # Create build directory
    mkdir -p "$BUILD_DIR_ACTUAL"

    # Sync source code (fast - only changes)
    print_info "Syncing source code..."
    rsync -a --delete \
        --exclude='build-*/' \
        --exclude='vcpkg/packages/' \
        --exclude='vcpkg/buildtrees/' \
        --exclude='vcpkg/downloads/' \
        --exclude='vcpkg/installed/' \
        --exclude='vcpkg_installed/' \
        --exclude='.git/objects/' \
        --exclude='.git/logs/' \
        --exclude='*.o' --exclude='*.so' --exclude='*.a' \
        "$PROJECT_ROOT/" "$BUILD_DIR_ACTUAL/"

    print_success "Source synced to local VM storage"
    echo ""
fi

cd "$BUILD_DIR_ACTUAL"

# =============================================================================
# Check prerequisites
# =============================================================================
print_info "Checking prerequisites..."

detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        echo "$ID"
    elif [ -f /etc/debian_version ]; then
        echo "debian"
    elif [ -f /etc/fedora-release ]; then
        echo "fedora"
    else
        echo "unknown"
    fi
}

install_package() {
    local package_name=$1
    local distro=$(detect_distro)

    print_warning "$package_name not found - installing..."

    case $distro in
        ubuntu|debian|linuxmint|pop)
            sudo apt update && sudo apt install -y $package_name
            ;;
        fedora|rhel|centos)
            sudo dnf install -y $package_name
            ;;
        *)
            print_error "Unsupported distribution: $distro"
            print_error "Please install $package_name manually"
            exit 1
            ;;
    esac

    if [ $? -eq 0 ]; then
        print_success "$package_name installed"
    else
        print_error "Failed to install $package_name"
        exit 1
    fi
}

# Check essential tools (autotools removed - vcpkg installs them if needed)
for tool in cmake ninja g++ git pkg-config; do
    if ! command -v $tool &> /dev/null; then
        case $tool in
            g++) install_package "build-essential" ;;
            ninja)
                # Ninja package name differs by distro
                distro=$(detect_distro)
                case $distro in
                    ubuntu|debian|linuxmint|pop) install_package "ninja-build" ;;
                    fedora|rhel|centos) install_package "ninja-build" ;;
                    arch|manjaro) install_package "ninja" ;;
                    *) install_package "ninja-build" ;;  # Default to ninja-build
                esac
                ;;
            *) install_package "$tool" ;;
        esac
    fi
done

# Check wxWidgets dependencies
print_info "Checking wxWidgets system dependencies..."
if ! pkg-config --exists gtk+-3.0 2>/dev/null; then
    print_warning "GTK3 development files not found - installing..."
    distro=$(detect_distro)
    case $distro in
        ubuntu|debian|linuxmint|pop)
            sudo apt update && sudo apt install -y \
                libgtk-3-dev libx11-dev libxext-dev libxtst-dev \
                libgl1-mesa-dev libglu1-mesa-dev
            ;;
        fedora|rhel|centos)
            sudo dnf install -y \
                gtk3-devel libX11-devel libXext-devel libXtst-devel \
                mesa-libGL-devel mesa-libGLU-devel
            ;;
        *)
            print_error "Unsupported distribution: $distro"
            print_error "Please install GTK3 development files manually"
            exit 1
            ;;
    esac

    if [ $? -eq 0 ]; then
        print_success "GTK3 dependencies installed"
    else
        print_error "Failed to install GTK3 dependencies"
        exit 1
    fi
fi

print_success "All prerequisites found"

# =============================================================================
# Initialize vcpkg
# =============================================================================
if [ ! -f "vcpkg/bootstrap-vcpkg.sh" ]; then
    print_warning "vcpkg submodule not initialized"
    print_info "Initializing git submodules..."
    git submodule update --init --recursive
    print_success "Submodules initialized"
else
    print_success "vcpkg submodule already initialized"
fi

if [ ! -f "vcpkg/vcpkg" ]; then
    print_info "Bootstrapping vcpkg (first time setup)..."
    print_warning "This may take 2-5 minutes..."
    cd vcpkg
    ./bootstrap-vcpkg.sh
    cd ..
    print_success "vcpkg bootstrapped"
else
    print_success "vcpkg already bootstrapped"
fi

# =============================================================================
# Clean build (optional)
# =============================================================================
if [ "$CLEAN_BUILD" = true ]; then
    print_info "Cleaning build directory..."
    rm -rf build-linux/
    print_success "Build directory cleaned"
fi

# =============================================================================
# Configure CMake
# =============================================================================
print_info "Configuring CMake ($BUILD_TYPE build)..."
print_warning "First build may take 15-30 minutes (vcpkg dependencies)"
print_warning "Subsequent builds: ~1-2 minutes (cached)"

CMAKE_ARGS=(
    -B build-linux
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
    -G Ninja
)

if [ "$VERBOSE" = true ]; then
    CMAKE_ARGS+=(-DCMAKE_VERBOSE_MAKEFILE=ON)
fi

# Force system binaries for VirtualBox (avoids cmake download issues)
export VCPKG_FORCE_SYSTEM_BINARIES=1

if ! cmake "${CMAKE_ARGS[@]}"; then
    print_error "CMake configuration failed"
    echo ""
    echo "Troubleshooting:"
    echo "  1. Check BUILDING.md for system dependencies"
    echo "  2. Install missing GTK3/X11 libraries"
    echo "  3. Try: sudo apt install libgtk-3-dev libx11-dev"
    exit 1
fi

print_success "CMake configuration completed"

# =============================================================================
# Build project
# =============================================================================
print_info "Building Kalahari ($BUILD_TYPE)..."

START_TIME=$(date +%s)

BUILD_ARGS=(
    --build build-linux
    --config "$BUILD_TYPE"
)

if [ "$VERBOSE" = true ]; then
    BUILD_ARGS+=(--verbose)
fi

NUM_CORES=$(nproc 2>/dev/null || echo 4)
BUILD_ARGS+=(--parallel "$NUM_CORES")

if ! cmake "${BUILD_ARGS[@]}"; then
    print_error "Build failed"
    exit 1
fi

END_TIME=$(date +%s)
BUILD_TIME=$((END_TIME - START_TIME))

print_success "Build completed in ${BUILD_TIME}s"

# =============================================================================
# Copy binaries back to shared folder (environment-specific directories)
# =============================================================================
if [ "$IS_VBOX" = true ]; then
    print_info "Copying binaries back to shared folder..."

    # Detect environment for unique output directory (avoid conflicts)
    if [[ "$PROJECT_ROOT" =~ ^/mnt/[a-z] ]]; then
        OUTPUT_DIR="$PROJECT_ROOT/build-linux-wsl"
        ENV_NAME="WSL"
    else
        OUTPUT_DIR="$PROJECT_ROOT/build-linux-vbox"
        ENV_NAME="VirtualBox"
    fi

    # Create bin directory on shared folder
    mkdir -p "$OUTPUT_DIR/bin"

    # Copy binaries
    if [ -d "build-linux/bin" ]; then
        cp -v build-linux/bin/* "$OUTPUT_DIR/bin/" 2>/dev/null || true
        print_success "Binaries copied to $OUTPUT_DIR/bin/ ($ENV_NAME)"
    fi

    # Copy plugins directory for tests
    if [ -d "build-linux/plugins" ]; then
        cp -r build-linux/plugins "$OUTPUT_DIR/"
        print_success "Plugins copied to $OUTPUT_DIR/plugins/ ($ENV_NAME)"
    fi

    echo ""
    print_info "Build artifacts remain in: $BUILD_DIR_ACTUAL/build-linux/"
    print_info "Binaries available at: $OUTPUT_DIR/bin/"
fi

# =============================================================================
# Run tests (optional)
# =============================================================================
if [ "$RUN_TESTS" = true ]; then
    cd build-linux
    print_info "Running tests..."

    if ! ctest --output-on-failure; then
        print_error "Some tests failed"
        exit 1
    fi

    print_success "All tests passed"
    cd ..
fi

# =============================================================================
# Show summary
# =============================================================================
echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}✓ Build Successful!${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "Build type:    ${GREEN}$BUILD_TYPE${NC}"

if [ "$IS_VBOX" = true ]; then
    # Detect environment again for summary (same logic as copy)
    if [[ "$PROJECT_ROOT" =~ ^/mnt/[a-z] ]]; then
        SUMMARY_OUTPUT="$PROJECT_ROOT/build-linux-wsl"
        SUMMARY_ENV="WSL"
    else
        SUMMARY_OUTPUT="$PROJECT_ROOT/build-linux-vbox"
        SUMMARY_ENV="VirtualBox"
    fi

    echo -e "Environment:    ${GREEN}$SUMMARY_ENV${NC}"
    echo -e "Build location: ${BLUE}$BUILD_DIR_ACTUAL/build-linux/${NC}"
    echo -e "Binaries:       ${BLUE}$SUMMARY_OUTPUT/bin/${NC}"
    echo ""
    echo -e "Run application:"
    echo -e "  ${BLUE}$SUMMARY_OUTPUT/bin/kalahari${NC}"
else
    echo -e "Executable:    ${BLUE}build-linux/bin/kalahari${NC}"
    echo -e "Tests:         ${BLUE}build-linux/bin/kalahari-tests${NC}"
    echo ""
    echo -e "Run application:"
    echo -e "  ${BLUE}./build-linux/bin/kalahari${NC}"
fi

echo ""
echo -e "Run tests:"
echo -e "  ${BLUE}cd build-linux && ctest --output-on-failure${NC}"
echo ""
echo -e "${BLUE}========================================${NC}"
