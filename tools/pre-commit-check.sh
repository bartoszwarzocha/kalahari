#!/bin/bash
# Kalahari Pre-Commit Quality Check
# Automated quality verification before commits
# Version: 1.0

set -euo pipefail

# ============================================================================
# CONFIGURATION
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Scoring
SCORE=0
TOTAL=0
ISSUES=()
WARNINGS=()

# ============================================================================
# HELPER FUNCTIONS
# ============================================================================

check() {
    local name="$1"
    local command="$2"

    TOTAL=$((TOTAL + 1))

    if eval "$command" > /dev/null 2>&1; then
        echo -e "   ${GREEN}✅${NC} $name"
        SCORE=$((SCORE + 1))
        return 0
    else
        echo -e "   ${RED}❌${NC} $name"
        ISSUES+=("$name")
        return 1
    fi
}

check_with_output() {
    local name="$1"
    local command="$2"

    TOTAL=$((TOTAL + 1))

    local output
    output=$(eval "$command" 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ] && [ -z "$output" ]; then
        echo -e "   ${GREEN}✅${NC} $name"
        SCORE=$((SCORE + 1))
        return 0
    else
        echo -e "   ${RED}❌${NC} $name"
        if [ -n "$output" ]; then
            echo -e "      ${YELLOW}→${NC} $output" | head -3
        fi
        ISSUES+=("$name")
        return 1
    fi
}

warn() {
    local message="$1"
    echo -e "   ${YELLOW}⚠️${NC}  $message"
    WARNINGS+=("$message")
}

# ============================================================================
# CHECK CATEGORIES
# ============================================================================

echo ""
echo "═══════════════════════════════════════════════════════════"
echo "  🔍 KALAHARI PRE-COMMIT QUALITY CHECK"
echo "═══════════════════════════════════════════════════════════"
echo ""

# ----------------------------------------------------------------------------
# 1. CODE FORMATTING
# ----------------------------------------------------------------------------

echo "📐 1. CODE FORMATTING"

if command -v clang-format &> /dev/null; then
    # Check if .clang-format exists
    check ".clang-format file exists" "[ -f .clang-format ]"

    # Check formatting on C++ files (if any exist)
    if find src include -name "*.cpp" -o -name "*.h" 2>/dev/null | grep -q .; then
        # Run clang-format in dry-run mode
        UNFORMATTED=$(find src include -name "*.cpp" -o -name "*.h" 2>/dev/null | \
            xargs clang-format --dry-run --Werror 2>&1 | grep "^" || true)

        if [ -z "$UNFORMATTED" ]; then
            TOTAL=$((TOTAL + 1))
            SCORE=$((SCORE + 1))
            echo -e "   ${GREEN}✅${NC} All C++ files properly formatted"
        else
            TOTAL=$((TOTAL + 1))
            echo -e "   ${RED}❌${NC} Some files need formatting"
            echo "$UNFORMATTED" | head -5 | sed 's/^/      /'
            ISSUES+=("Unformatted C++ files")
        fi
    else
        warn "No C++ files found to format (Phase 0 - OK)"
    fi
else
    warn "clang-format not installed"
fi

echo ""

# ----------------------------------------------------------------------------
# 2. NAMING CONVENTIONS
# ----------------------------------------------------------------------------

echo "📝 2. NAMING CONVENTIONS"

if find include src -name "*.h" -o -name "*.cpp" 2>/dev/null | grep -q .; then
    # Check member variable prefix m_
    MISSING_M_PREFIX=$(grep -rn "^\s*[A-Za-z_][A-Za-z0-9_]*\s\+[a-z][A-Za-z0-9_]*;" \
        include/ src/ 2>/dev/null | \
        grep -v "m_" | \
        grep -v "//" | \
        grep "private:\|protected:" -A 20 | \
        head -5 || true)

    if [ -z "$MISSING_M_PREFIX" ]; then
        TOTAL=$((TOTAL + 1))
        SCORE=$((SCORE + 1))
        echo -e "   ${GREEN}✅${NC} Member variables use m_ prefix"
    else
        TOTAL=$((TOTAL + 1))
        echo -e "   ${YELLOW}⚠️${NC}  Some members may lack m_ prefix (check manually)"
        WARNINGS+=("Possible m_ prefix violations")
    fi

    # Check for snake_case file names
    WRONG_FILENAMES=$(find src include -type f \( -name "*.cpp" -o -name "*.h" \) | \
        grep -v "/build/" | \
        grep -E "[A-Z]" || true)

    if [ -z "$WRONG_FILENAMES" ]; then
        TOTAL=$((TOTAL + 1))
        SCORE=$((SCORE + 1))
        echo -e "   ${GREEN}✅${NC} File names use snake_case"
    else
        TOTAL=$((TOTAL + 1))
        echo -e "   ${RED}❌${NC} Some files use PascalCase (should be snake_case)"
        echo "$WRONG_FILENAMES" | head -3 | sed 's/^/      /'
        ISSUES+=("File naming convention violated")
    fi
else
    warn "No C++ files to check (Phase 0 - OK)"
fi

echo ""

# ----------------------------------------------------------------------------
# 3. MODERN C++ PRACTICES
# ----------------------------------------------------------------------------

echo "🚀 3. MODERN C++ PRACTICES"

if find src include -name "*.cpp" -o -name "*.h" 2>/dev/null | grep -q .; then
    # Check for raw new/delete
    RAW_NEW=$(grep -rn "\bnew\s" src/ include/ 2>/dev/null | \
        grep -v "^Binary" | \
        grep -v "//" | \
        grep -v "smart_ptr" || true)

    if [ -z "$RAW_NEW" ]; then
        TOTAL=$((TOTAL + 1))
        SCORE=$((SCORE + 1))
        echo -e "   ${GREEN}✅${NC} No raw 'new' usage (smart pointers preferred)"
    else
        TOTAL=$((TOTAL + 1))
        echo -e "   ${RED}❌${NC} Raw 'new' found (use std::make_unique/shared)"
        echo "$RAW_NEW" | head -3 | sed 's/^/      /'
        ISSUES+=("Raw pointer usage (new)")
    fi

    # Check for raw delete
    RAW_DELETE=$(grep -rn "\bdelete\s" src/ include/ 2>/dev/null | \
        grep -v "^Binary" | \
        grep -v "//" || true)

    if [ -z "$RAW_DELETE" ]; then
        TOTAL=$((TOTAL + 1))
        SCORE=$((SCORE + 1))
        echo -e "   ${GREEN}✅${NC} No raw 'delete' usage"
    else
        TOTAL=$((TOTAL + 1))
        echo -e "   ${RED}❌${NC} Raw 'delete' found (use smart pointers)"
        ISSUES+=("Raw pointer usage (delete)")
    fi
else
    warn "No C++ files to check (Phase 0 - OK)"
fi

echo ""

# ----------------------------------------------------------------------------
# 4. DOCUMENTATION
# ----------------------------------------------------------------------------

echo "📚 4. DOCUMENTATION"

if find include -name "*.h" 2>/dev/null | grep -q .; then
    # Check for Doxygen comments in public headers
    HEADERS_COUNT=$(find include -name "*.h" | wc -l)
    DOXYGEN_COUNT=$(grep -r "///" include/ 2>/dev/null | wc -l)

    if [ "$DOXYGEN_COUNT" -gt 0 ]; then
        TOTAL=$((TOTAL + 1))
        SCORE=$((SCORE + 1))
        echo -e "   ${GREEN}✅${NC} Doxygen comments present ($DOXYGEN_COUNT in $HEADERS_COUNT headers)"
    else
        TOTAL=$((TOTAL + 1))
        echo -e "   ${YELLOW}⚠️${NC}  No Doxygen comments found (add /// @brief)"
        WARNINGS+=("Missing Doxygen comments")
    fi
else
    warn "No headers to check (Phase 0 - OK)"
fi

# Check for commented-out code
COMMENTED_CODE=$(grep -rn "^\s*//" src/ include/ 2>/dev/null | \
    grep -E "(if|for|while|class|void|int|return)" | \
    wc -l || echo 0)

if [ "$COMMENTED_CODE" -lt 10 ]; then
    TOTAL=$((TOTAL + 1))
    SCORE=$((SCORE + 1))
    echo -e "   ${GREEN}✅${NC} Minimal commented-out code ($COMMENTED_CODE lines)"
else
    TOTAL=$((TOTAL + 1))
    echo -e "   ${YELLOW}⚠️${NC}  Lots of commented code ($COMMENTED_CODE lines - use git history)"
    WARNINGS+=("Excessive commented-out code")
fi

echo ""

# ----------------------------------------------------------------------------
# 5. ARCHITECTURE COMPLIANCE
# ----------------------------------------------------------------------------

echo "🏗️  5. ARCHITECTURE COMPLIANCE (MVP)"

if [ -d "src/core/model" ]; then
    # Check for wxWidgets in Model layer
    WX_IN_MODEL=$(grep -rn "#include.*wx/" src/core/model/ 2>/dev/null || true)

    if [ -z "$WX_IN_MODEL" ]; then
        TOTAL=$((TOTAL + 1))
        SCORE=$((SCORE + 1))
        echo -e "   ${GREEN}✅${NC} Model layer pure (no wxWidgets)"
    else
        TOTAL=$((TOTAL + 1))
        echo -e "   ${RED}❌${NC} wxWidgets found in Model layer"
        echo "$WX_IN_MODEL" | head -3 | sed 's/^/      /'
        ISSUES+=("wxWidgets in Model layer (violates MVP)")
    fi
else
    warn "src/core/model/ not yet created (Phase 0 - OK)"
fi

echo ""

# ----------------------------------------------------------------------------
# 6. INTERNATIONALIZATION (i18n)
# ----------------------------------------------------------------------------

echo "🌐 6. INTERNATIONALIZATION"

if find src -name "*.cpp" 2>/dev/null | grep -q .; then
    # Check for hardcoded UI strings (simple heuristic)
    HARDCODED_STRINGS=$(grep -rn "wxString\|wxMessageBox\|SetLabel" src/ 2>/dev/null | \
        grep -v '_(\"' | \
        grep -v "//" | \
        head -5 || true)

    if [ -z "$HARDCODED_STRINGS" ]; then
        TOTAL=$((TOTAL + 1))
        SCORE=$((SCORE + 1))
        echo -e "   ${GREEN}✅${NC} UI strings wrapped in _(\"...\")"
    else
        TOTAL=$((TOTAL + 1))
        echo -e "   ${YELLOW}⚠️${NC}  Possible hardcoded UI strings (check manually)"
        WARNINGS+=("Possible hardcoded UI strings")
    fi
else
    warn "No C++ files to check (Phase 0 - OK)"
fi

echo ""

# ----------------------------------------------------------------------------
# 7. BUILD SYSTEM CONSISTENCY
# ----------------------------------------------------------------------------

echo "⚙️  7. BUILD SYSTEM"

check "CMakeLists.txt exists" "[ -f CMakeLists.txt ]"
check "vcpkg.json exists" "[ -f vcpkg.json ]"

# Check if vcpkg dependencies match CMakeLists.txt find_package calls
if [ -f "vcpkg.json" ] && [ -f "CMakeLists.txt" ]; then
    VCPKG_DEPS=$(jq -r '.dependencies[].name // .dependencies[]' vcpkg.json 2>/dev/null | sort || echo "")
    CMAKE_FINDS=$(grep -o "find_package([^)]*)" CMakeLists.txt 2>/dev/null | \
        sed 's/find_package(\([^ )]*\).*/\1/' | \
        tr '[:upper:]' '[:lower:]' | \
        sort || echo "")

    # Simple check: at least some overlap
    if echo "$VCPKG_DEPS" | grep -q "wx"; then
        if echo "$CMAKE_FINDS" | grep -q "wx"; then
            TOTAL=$((TOTAL + 1))
            SCORE=$((SCORE + 1))
            echo -e "   ${GREEN}✅${NC} vcpkg.json and CMakeLists.txt consistent"
        else
            TOTAL=$((TOTAL + 1))
            echo -e "   ${YELLOW}⚠️${NC}  vcpkg.json has wxWidgets but not in CMakeLists.txt"
            WARNINGS+=("Build system consistency check")
        fi
    fi
fi

echo ""

# ----------------------------------------------------------------------------
# 8. DOCUMENTATION CONSISTENCY
# ----------------------------------------------------------------------------

echo "📄 8. DOCUMENTATION CONSISTENCY"

check "CLAUDE.md exists" "[ -f CLAUDE.md ]"
check "CHANGELOG.md exists" "[ -f CHANGELOG.md ]"
check "ROADMAP.md exists" "[ -f ROADMAP.md ]"

# Check if CHANGELOG has recent entries
if [ -f "CHANGELOG.md" ]; then
    LAST_CHANGELOG_DATE=$(grep -m 1 "202[0-9]-[0-9][0-9]-[0-9][0-9]" CHANGELOG.md | head -1 || echo "")

    if [ -n "$LAST_CHANGELOG_DATE" ]; then
        TOTAL=$((TOTAL + 1))
        SCORE=$((SCORE + 1))
        echo -e "   ${GREEN}✅${NC} CHANGELOG.md has recent entries"
    else
        TOTAL=$((TOTAL + 1))
        echo -e "   ${YELLOW}⚠️${NC}  CHANGELOG.md may be outdated"
        WARNINGS+=("CHANGELOG.md needs update")
    fi
fi

echo ""

# ----------------------------------------------------------------------------
# 9. CODE ANNOTATIONS
# ----------------------------------------------------------------------------

echo "📌 9. CODE ANNOTATIONS"

TODO_COUNT=$(grep -rn "TODO\|FIXME\|XXX\|HACK" src/ include/ 2>/dev/null | wc -l || echo 0)

if [ "$TODO_COUNT" -eq 0 ]; then
    TOTAL=$((TOTAL + 1))
    SCORE=$((SCORE + 1))
    echo -e "   ${GREEN}✅${NC} No TODO/FIXME markers"
elif [ "$TODO_COUNT" -lt 10 ]; then
    TOTAL=$((TOTAL + 1))
    SCORE=$((SCORE + 1))
    echo -e "   ${GREEN}✅${NC} Few TODO/FIXME markers ($TODO_COUNT - acceptable)"
else
    TOTAL=$((TOTAL + 1))
    echo -e "   ${YELLOW}⚠️${NC}  Many TODO/FIXME markers ($TODO_COUNT - consider addressing)"
    WARNINGS+=("$TODO_COUNT TODO/FIXME markers")
fi

echo ""

# ----------------------------------------------------------------------------
# 10. SECURITY
# ----------------------------------------------------------------------------

echo "🔒 10. SECURITY"

# Check for hardcoded secrets
SECRETS_PATTERNS=(
    "password\s*=\s*['\"]"
    "api_key\s*=\s*['\"]"
    "secret\s*=\s*['\"]"
    "token\s*=\s*['\"]"
    "AKIA[0-9A-Z]{16}"  # AWS Access Key
)

SECRETS_FOUND=0
for pattern in "${SECRETS_PATTERNS[@]}"; do
    if grep -rEi "$pattern" src/ include/ 2>/dev/null | grep -v "//" | grep -v "^Binary" | head -1 | grep -q .; then
        SECRETS_FOUND=$((SECRETS_FOUND + 1))
    fi
done

if [ "$SECRETS_FOUND" -eq 0 ]; then
    TOTAL=$((TOTAL + 1))
    SCORE=$((SCORE + 1))
    echo -e "   ${GREEN}✅${NC} No hardcoded secrets detected"
else
    TOTAL=$((TOTAL + 1))
    echo -e "   ${RED}❌${NC} Potential secrets detected ($SECRETS_FOUND patterns)"
    ISSUES+=("Potential hardcoded secrets")
fi

echo ""

# ----------------------------------------------------------------------------
# 11. TEST FILES VERIFICATION
# ----------------------------------------------------------------------------

echo "🧪 11. TESTING"

if [ -d "tests" ] && [ -d "src" ]; then
    SRC_FILES=$(find src -name "*.cpp" | wc -l)
    TEST_FILES=$(find tests -name "*.cpp" | wc -l)

    if [ "$SRC_FILES" -gt 0 ]; then
        TEST_RATIO=$((TEST_FILES * 100 / SRC_FILES))

        if [ "$TEST_RATIO" -ge 50 ]; then
            TOTAL=$((TOTAL + 1))
            SCORE=$((SCORE + 1))
            echo -e "   ${GREEN}✅${NC} Good test coverage ratio ($TEST_FILES tests for $SRC_FILES sources)"
        elif [ "$TEST_RATIO" -ge 20 ]; then
            TOTAL=$((TOTAL + 1))
            SCORE=$((SCORE + 1))
            echo -e "   ${GREEN}✅${NC} Acceptable test ratio ($TEST_RATIO% - $TEST_FILES/$SRC_FILES)"
        else
            TOTAL=$((TOTAL + 1))
            echo -e "   ${YELLOW}⚠️${NC}  Low test coverage ratio ($TEST_RATIO% - $TEST_FILES/$SRC_FILES)"
            WARNINGS+=("Low test coverage ratio")
        fi
    else
        warn "No source files yet (Phase 0 - OK)"
    fi
else
    warn "tests/ or src/ directory not found (Phase 0 - OK)"
fi

echo ""

# ----------------------------------------------------------------------------
# 12. FILE SIZE CHECK
# ----------------------------------------------------------------------------

echo "📏 12. FILE SIZE"

LARGE_FILES=$(find src include -name "*.cpp" -o -name "*.h" 2>/dev/null | \
    xargs wc -l 2>/dev/null | \
    awk '$1 > 1000 {print}' || true)

if [ -z "$LARGE_FILES" ]; then
    TOTAL=$((TOTAL + 1))
    SCORE=$((SCORE + 1))
    echo -e "   ${GREEN}✅${NC} No files >1000 lines"
else
    TOTAL=$((TOTAL + 1))
    echo -e "   ${YELLOW}⚠️${NC}  Large files found (>1000 lines):"
    echo "$LARGE_FILES" | head -3 | sed 's/^/      /'
    WARNINGS+=("Large files >1000 lines")
fi

echo ""

# ============================================================================
# FINAL SCORE
# ============================================================================

echo "═══════════════════════════════════════════════════════════"
echo ""

PERCENT=0
if [ "$TOTAL" -gt 0 ]; then
    PERCENT=$((SCORE * 100 / TOTAL))
fi

echo "📊 QUALITY SCORE: $SCORE / $TOTAL ($PERCENT%)"
echo ""

# Display issues
if [ ${#ISSUES[@]} -gt 0 ]; then
    echo -e "${RED}❌ ISSUES (${#ISSUES[@]}):${NC}"
    for issue in "${ISSUES[@]}"; do
        echo "   • $issue"
    done
    echo ""
fi

# Display warnings
if [ ${#WARNINGS[@]} -gt 0 ]; then
    echo -e "${YELLOW}⚠️  WARNINGS (${#WARNINGS[@]}):${NC}"
    for warning in "${WARNINGS[@]}"; do
        echo "   • $warning"
    done
    echo ""
fi

# Quality gates
echo "═══════════════════════════════════════════════════════════"
echo ""

if [ "$PERCENT" -lt 70 ]; then
    echo -e "${RED}❌ QUALITY GATE FAILED${NC} (<70%)"
    echo "   DO NOT COMMIT - Address critical issues first"
    echo ""
    exit 1
elif [ "$PERCENT" -lt 90 ]; then
    echo -e "${YELLOW}⚠️  ACCEPTABLE BUT NEEDS IMPROVEMENT${NC} (70-89%)"
    echo "   You can commit, but consider addressing warnings"
    echo ""
    exit 0
else
    echo -e "${GREEN}✅ EXCELLENT QUALITY${NC} (90%+)"
    echo "   Ready to commit!"
    echo ""
    exit 0
fi
