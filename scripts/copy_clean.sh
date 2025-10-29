#!/bin/bash
# Copy Kalahari project without build artifacts and temporary files
# Usage: ./copy_clean.sh <destination_directory>

set -e  # Exit on error

# ANSI color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Kalahari Clean Copy Script${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check if destination provided
if [ $# -eq 0 ]; then
    echo -e "${RED}[✗]${NC} Error: No destination directory specified"
    echo ""
    echo "Usage: $0 <destination_directory>"
    echo ""
    echo "Example:"
    echo "  $0 ~/Kalahari           # Copy to home directory"
    echo "  $0 /tmp/kalahari-clean  # Copy to /tmp"
    echo ""
    exit 1
fi

DEST="$1"
SOURCE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo -e "${BLUE}[INFO]${NC} Source:      $SOURCE"
echo -e "${BLUE}[INFO]${NC} Destination: $DEST"
echo ""

# Check if destination exists
if [ -d "$DEST" ]; then
    echo -e "${YELLOW}[!]${NC} Destination directory already exists!"
    read -p "Remove existing directory and continue? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo -e "${YELLOW}[!]${NC} Copy cancelled"
        exit 0
    fi
    echo -e "${BLUE}[INFO]${NC} Removing existing directory..."
    rm -rf "$DEST"
fi

# Create destination directory
echo -e "${BLUE}[INFO]${NC} Creating destination directory..."
mkdir -p "$DEST"

# Check if rsync is available (faster and more reliable)
if command -v rsync &> /dev/null; then
    echo -e "${BLUE}[INFO]${NC} Using rsync for fast copying..."

    rsync -av \
        --exclude='build-*/' \
        --exclude='build/' \
        --exclude='vcpkg/packages/' \
        --exclude='vcpkg/buildtrees/' \
        --exclude='vcpkg/downloads/' \
        --exclude='vcpkg/installed/' \
        --exclude='vcpkg/.git/objects/' \
        --exclude='vcpkg/.git/logs/' \
        --exclude='.git/objects/' \
        --exclude='.git/logs/' \
        --exclude='*.o' \
        --exclude='*.obj' \
        --exclude='*.exe' \
        --exclude='*.dll' \
        --exclude='*.so' \
        --exclude='*.dylib' \
        --exclude='*.a' \
        --exclude='*.lib' \
        --exclude='*.log' \
        --exclude='*.tmp' \
        --exclude='*.bak' \
        --exclude='*.swp' \
        --exclude='*.swo' \
        --exclude='*~' \
        --exclude='.DS_Store' \
        --exclude='__pycache__/' \
        --exclude='*.pyc' \
        --exclude='.pytest_cache/' \
        --exclude='node_modules/' \
        --exclude='.cache/' \
        --exclude='*.jpg' \
        --exclude='*.png' \
        --exclude='*.jpeg' \
        --exclude='Schowek_*' \
        "$SOURCE/" "$DEST/"

    COPY_STATUS=$?
else
    echo -e "${BLUE}[INFO]${NC} rsync not found, using cp (slower)..."

    # Copy with cp, excluding patterns
    cd "$SOURCE"

    # Find all files excluding build artifacts
    find . -type f \
        ! -path './build-*/*' \
        ! -path './build/*' \
        ! -path './vcpkg/packages/*' \
        ! -path './vcpkg/buildtrees/*' \
        ! -path './vcpkg/downloads/*' \
        ! -path './vcpkg/installed/*' \
        ! -path './vcpkg/.git/objects/*' \
        ! -path './vcpkg/.git/logs/*' \
        ! -path './.git/objects/*' \
        ! -path './.git/logs/*' \
        ! -name '*.o' \
        ! -name '*.obj' \
        ! -name '*.exe' \
        ! -name '*.dll' \
        ! -name '*.so' \
        ! -name '*.dylib' \
        ! -name '*.a' \
        ! -name '*.lib' \
        ! -name '*.log' \
        ! -name '*.tmp' \
        ! -name '*.bak' \
        ! -name '*.swp' \
        ! -name '*.swo' \
        ! -name '*~' \
        ! -name '.DS_Store' \
        ! -name '*.pyc' \
        ! -name '*.jpg' \
        ! -name '*.png' \
        ! -name '*.jpeg' \
        ! -name 'Schowek_*' \
        -exec sh -c 'mkdir -p "$1/$(dirname "$2")" && cp "$2" "$1/$2"' _ "$DEST" {} \;

    COPY_STATUS=$?
fi

# Check copy status
if [ $COPY_STATUS -eq 0 ]; then
    echo ""
    echo -e "${GREEN}[✓]${NC} Copy completed successfully"
    echo ""
    echo -e "${GREEN}[✓]${NC} Clean copy ready at: ${DEST}"
    echo ""
    echo "Next steps:"
    echo "  cd $DEST"
    echo "  ./scripts/build_linux.sh"
    echo ""
else
    echo ""
    echo -e "${RED}[✗]${NC} Copy failed!"
    exit 1
fi
