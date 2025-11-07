#!/bin/bash
# =============================================================================
# Kalahari Build Script (macOS)
# =============================================================================
# Automated build-macos script for macOS (Intel + Apple Silicon)
# Requires: Xcode Command Line Tools, Homebrew, CMake, Ninja
#
# Usage:
#   ./scripts/build_macos.sh              # Debug build-macos (native arch)
#   ./scripts/build_macos.sh --release    # Release build-macos
#   ./scripts/build_macos.sh --universal  # Universal binary (Intel + ARM)
#   ./scripts/build_macos.sh --clean      # Clean + rebuild
#   ./scripts/build_macos.sh --test       # Build + run tests
#   ./scripts/build_macos.sh --help       # Show help
#
# =============================================================================

set -e  # Exit on error

# Colors for output (ANSI escape codes)
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'  # No Color

# Get project root (script is in scripts/ subdirectory)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Default configuration
BUILD_TYPE="Debug"
CLEAN_BUILD=false
RUN_TESTS=false
VERBOSE=false
UNIVERSAL_BINARY=false

# Detect architecture
ARCH="$(uname -m)"

# =============================================================================
# Function: Print colored message
# =============================================================================
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[!]${NC} $1"
}

# =============================================================================
# Function: Show help
# =============================================================================
show_help() {
    cat << EOF
Kalahari Build Script (macOS)

Usage: $0 [OPTIONS]

Options:
  --release, -r      Build in Release mode (default: Debug)
  --clean, -c        Clean build-macos directory before building
  --test, -t         Run tests after building
  --universal, -u    Build Universal Binary (Intel + Apple Silicon)
  --verbose, -v      Enable verbose build-macos output
  --help, -h         Show this help message

Examples:
  $0                       # Debug build-macos (native architecture)
  $0 --release             # Release build-macos
  $0 --universal           # Universal binary (both architectures)
  $0 --clean --release     # Clean + Release build-macos
  $0 --test                # Debug build-macos + run tests
  $0 -r -u -t              # Release universal binary + tests

Architecture Info:
  Current:   $ARCH
  Intel:     x86_64
  Apple M1+: arm64

Build output:
  Executable: build-macos/bin/kalahari
  Tests:      build-macos/bin/kalahari-tests

For more information, see BUILDING.md
EOF
}

# =============================================================================
# Function: Parse command-line arguments
# =============================================================================
parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --release|-r)
                BUILD_TYPE="Release"
                shift
                ;;
            --clean|-c)
                CLEAN_BUILD=true
                shift
                ;;
            --test|-t)
                RUN_TESTS=true
                shift
                ;;
            --universal|-u)
                UNIVERSAL_BINARY=true
                shift
                ;;
            --verbose|-v)
                VERBOSE=true
                shift
                ;;
            --help|-h)
                show_help
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                echo "Run '$0 --help' for usage information"
                exit 1
                ;;
        esac
    done
}

# =============================================================================
# Function: Install package via Homebrew
# =============================================================================
install_package() {
    local package_name=$1

    if ! command -v brew &> /dev/null; then
        print_error "Homebrew not installed - cannot auto-install $package_name"
        echo ""
        echo "Install Homebrew first: https://brew.sh"
        echo "Then run: brew install $package_name"
        exit 1
    fi

    print_warning "$package_name not found - installing..."
    if brew install "$package_name"; then
        print_success "$package_name installed"
    else
        print_error "Failed to install $package_name"
        exit 1
    fi
}

# =============================================================================
# Function: Check prerequisites
# =============================================================================
check_prerequisites() {
    print_info "Checking prerequisites..."

    # Check Xcode Command Line Tools (MANDATORY - cannot auto-install)
    if ! xcode-select -p &> /dev/null; then
        print_error "Xcode Command Line Tools not installed!"
        echo ""
        echo "Install with:"
        echo "  xcode-select --install"
        echo ""
        echo "After installation completes, run this script again."
        exit 1
    fi

    # Check Homebrew (MANDATORY for auto-install)
    if ! command -v brew &> /dev/null; then
        print_error "Homebrew not found!"
        echo ""
        echo "Install Homebrew from: https://brew.sh"
        echo ""
        echo "Run this command:"
        echo '  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"'
        echo ""
        echo "After installation, run this script again."
        exit 1
    fi

    # Check essential build tools (auto-install if missing)
    for tool in cmake git pkg-config curl autoconf automake libtool m4 bison flex ninja; do
        if ! command -v $tool &> /dev/null; then
            install_package "$tool"
        fi
    done

    # Check autoconf-archive (provides additional autoconf macros)
    if ! brew list autoconf-archive &> /dev/null; then
        print_warning "autoconf-archive not found - installing..."
        install_package "autoconf-archive"
    fi

    GENERATOR="Ninja"
    print_success "All prerequisites installed (using $GENERATOR)"
}

# =============================================================================
# Function: Initialize vcpkg submodule
# =============================================================================
init_vcpkg_submodule() {
    cd "$PROJECT_ROOT"

    if [ ! -f "vcpkg/bootstrap-vcpkg.sh" ]; then
        print_warning "vcpkg submodule not initialized"
        print_info "Initializing git submodules..."

        if ! git submodule update --init --recursive; then
            print_error "Failed to initialize submodules"
            exit 1
        fi

        print_success "Submodules initialized"
    else
        print_success "vcpkg submodule already initialized"
    fi
}

# =============================================================================
# Function: Bootstrap vcpkg
# =============================================================================
bootstrap_vcpkg() {
    cd "$PROJECT_ROOT"

    if [ ! -f "vcpkg/vcpkg" ]; then
        print_info "Bootstrapping vcpkg (first time setup)..."
        print_warning "This may take 2-5 minutes..."

        cd vcpkg
        if ! ./bootstrap-vcpkg.sh; then
            print_error "vcpkg bootstrap failed"
            exit 1
        fi
        cd ..

        print_success "vcpkg bootstrapped successfully"
    else
        print_success "vcpkg already bootstrapped"
    fi
}

# =============================================================================
# Function: Clean build-macos directory
# =============================================================================
clean_build() {
    cd "$PROJECT_ROOT"

    if [ "$CLEAN_BUILD" = true ]; then
        print_info "Cleaning build-macos directory..."
        rm -rf build-macos/
        print_success "Build directory cleaned"
    fi
}

# =============================================================================
# Function: Configure CMake
# =============================================================================
configure_cmake() {
    cd "$PROJECT_ROOT"

    print_info "Configuring CMake ($BUILD_TYPE build-macos)..."

    if [ "$UNIVERSAL_BINARY" = true ]; then
        print_info "Building Universal Binary (Intel + Apple Silicon)"
        OSX_ARCHITECTURES="x86_64;arm64"
    else
        print_info "Building for native architecture ($ARCH)"
        OSX_ARCHITECTURES="$ARCH"
    fi

    print_warning "First build-macos may take 10-20 minutes (vcpkg dependencies)"
    print_warning "Subsequent builds: ~1-2 minutes (cached)"

    CMAKE_ARGS=(
        -B build-macos
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
        -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
        -DCMAKE_OSX_ARCHITECTURES="$OSX_ARCHITECTURES"
        -G "$GENERATOR"
    )

    if [ "$VERBOSE" = true ]; then
        CMAKE_ARGS+=(-DCMAKE_VERBOSE_MAKEFILE=ON)
    fi

    # Ensure vcpkg can find ninja (for sub-builds like Python3, OpenSSL)
    if command -v ninja &> /dev/null; then
        export CMAKE_MAKE_PROGRAM=$(command -v ninja)
    fi

    if ! cmake "${CMAKE_ARGS[@]}"; then
        print_error "CMake configuration failed"
        echo ""
        echo "Troubleshooting:"
        echo "  1. Check BUILDING.md for detailed instructions"
        echo "  2. Make sure Xcode Command Line Tools are installed"
        echo "  3. Try: xcode-select --install"
        exit 1
    fi

    print_success "CMake configuration completed"
}

# =============================================================================
# Function: Build project
# =============================================================================
build_project() {
    cd "$PROJECT_ROOT"

    print_info "Building Kalahari ($BUILD_TYPE)..."

    START_TIME=$(date +%s)

    BUILD_ARGS=(
        --build-macos build-macos
        --config "$BUILD_TYPE"
    )

    if [ "$VERBOSE" = true ]; then
        BUILD_ARGS+=(--verbose)
    fi

    # Get number of CPU cores for parallel build-macos
    NUM_CORES=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
    BUILD_ARGS+=(--parallel "$NUM_CORES")

    if ! cmake "${BUILD_ARGS[@]}"; then
        print_error "Build failed"
        exit 1
    fi

    END_TIME=$(date +%s)
    BUILD_TIME=$((END_TIME - START_TIME))

    print_success "Build completed in ${BUILD_TIME}s"
}

# =============================================================================
# Function: Run tests
# =============================================================================
run_tests() {
    if [ "$RUN_TESTS" = true ]; then
        cd "$PROJECT_ROOT/build-macos"

        print_info "Running tests..."

        if ! ctest --output-on-failure; then
            print_error "Some tests failed"
            exit 1
        fi

        print_success "All tests passed"
        cd "$PROJECT_ROOT"
    fi
}

# =============================================================================
# Function: Show build-macos summary
# =============================================================================
show_summary() {
    cd "$PROJECT_ROOT"

    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${GREEN}✓ Build Successful!${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    echo -e "Build type:    ${GREEN}$BUILD_TYPE${NC}"
    echo -e "Architecture:  ${GREEN}$OSX_ARCHITECTURES${NC}"
    echo -e "Executable:    ${BLUE}build-macos/bin/kalahari${NC}"
    echo -e "Tests:         ${BLUE}build-macos/bin/kalahari-tests${NC}"
    echo ""
    echo -e "Run application:"
    echo -e "  ${BLUE}./build-macos/bin/kalahari${NC}"
    echo ""
    echo -e "Run tests:"
    echo -e "  ${BLUE}cd build-macos && ctest --output-on-failure${NC}"
    echo ""

    # Show binary architecture info
    if [ -f "build-macos/bin/kalahari" ]; then
        print_info "Binary architectures:"
        lipo -info build-macos/bin/kalahari
    fi

    echo ""
    echo -e "${BLUE}========================================${NC}"
}

# =============================================================================
# Main execution
# =============================================================================
main() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}Kalahari Build Script (macOS)${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""

    parse_arguments "$@"
    check_prerequisites
    init_vcpkg_submodule
    bootstrap_vcpkg
    clean_build
    configure_cmake
    build_project
    run_tests
    show_summary
}

# Run main function with all arguments
main "$@"
