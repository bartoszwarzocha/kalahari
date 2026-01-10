#!/bin/bash
# =============================================================================
# Kalahari Git Hooks Installer
# =============================================================================
# Installs git hooks for the Kalahari project.
#
# Usage:
#   ./scripts/install-hooks.sh           # Install all hooks
#   ./scripts/install-hooks.sh --check   # Check if hooks are installed
#   ./scripts/install-hooks.sh --remove  # Remove installed hooks
#
# =============================================================================

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Get script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
HOOKS_DIR="$PROJECT_ROOT/.git/hooks"

# =============================================================================
# Helper functions
# =============================================================================
print_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
print_success() { echo -e "${GREEN}[OK]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[WARN]${NC} $1"; }

show_help() {
    cat << EOF
Kalahari Git Hooks Installer

Usage: $0 [OPTIONS]

Options:
  --check, -c     Check if hooks are installed
  --remove, -r    Remove installed hooks
  --help, -h      Show this help message

Hooks installed:
  pre-commit      Validates C++ patterns before commit
                  - Blocks QIcon("...") (use ArtProvider)
                  - Blocks QColor(r,g,b) (use ArtProvider colors)
                  - Blocks qDebug() (use Logger)
                  - Blocks std::cout (use Logger)

Examples:
  $0              Install all hooks
  $0 --check      Check installation status
  $0 --remove     Remove all hooks

EOF
}

# =============================================================================
# Hook definitions
# =============================================================================
# Add more hooks here as needed
HOOKS=("pre-commit")

# =============================================================================
# Commands
# =============================================================================

check_hooks() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}  Kalahari Git Hooks Status${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""

    local all_installed=true

    for hook in "${HOOKS[@]}"; do
        local hook_path="$HOOKS_DIR/$hook"
        if [ -f "$hook_path" ] && [ -x "$hook_path" ]; then
            print_success "$hook: installed and executable"
        elif [ -f "$hook_path" ]; then
            print_warning "$hook: exists but NOT executable"
            all_installed=false
        else
            print_error "$hook: NOT installed"
            all_installed=false
        fi
    done

    echo ""
    if [ "$all_installed" = true ]; then
        echo -e "${GREEN}All hooks are properly installed.${NC}"
    else
        echo -e "${YELLOW}Some hooks are missing. Run: ./scripts/install-hooks.sh${NC}"
    fi
    echo ""
}

remove_hooks() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}  Removing Git Hooks${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""

    for hook in "${HOOKS[@]}"; do
        local hook_path="$HOOKS_DIR/$hook"
        if [ -f "$hook_path" ]; then
            rm -f "$hook_path"
            print_success "Removed: $hook"
        else
            print_info "Not found: $hook (skipped)"
        fi
    done

    echo ""
    echo -e "${GREEN}Hooks removed successfully.${NC}"
    echo ""
}

install_hooks() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}  Installing Git Hooks${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""

    # Verify we're in a git repository
    if [ ! -d "$PROJECT_ROOT/.git" ]; then
        print_error "Not a git repository: $PROJECT_ROOT"
        exit 1
    fi

    # Create hooks directory if it doesn't exist
    if [ ! -d "$HOOKS_DIR" ]; then
        mkdir -p "$HOOKS_DIR"
        print_info "Created hooks directory: $HOOKS_DIR"
    fi

    # Install pre-commit hook
    local precommit_source="$HOOKS_DIR/pre-commit"

    if [ -f "$precommit_source" ]; then
        # Make it executable
        chmod +x "$precommit_source"
        print_success "pre-commit hook: made executable"
    else
        print_error "pre-commit hook not found at: $precommit_source"
        print_info "The hook should already exist in .git/hooks/"
        exit 1
    fi

    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}  Installation Complete!${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo -e "Installed hooks:"
    for hook in "${HOOKS[@]}"; do
        echo -e "  - $hook"
    done
    echo ""
    echo -e "${BLUE}The pre-commit hook will now validate C++ patterns:${NC}"
    echo -e "  - QIcon(\"...\") -> use ArtProvider::getInstance().getIcon()"
    echo -e "  - QColor(r,g,b) -> use ArtProvider colors"
    echo -e "  - qDebug()      -> use Logger::getInstance()"
    echo -e "  - std::cout     -> use Logger::getInstance()"
    echo ""
    echo -e "${YELLOW}To bypass the hook (not recommended):${NC}"
    echo -e "  git commit --no-verify"
    echo ""
}

# =============================================================================
# Main
# =============================================================================

case "${1:-}" in
    --check|-c)
        check_hooks
        ;;
    --remove|-r)
        remove_hooks
        ;;
    --help|-h)
        show_help
        ;;
    "")
        install_hooks
        ;;
    *)
        print_error "Unknown option: $1"
        echo ""
        show_help
        exit 1
        ;;
esac
