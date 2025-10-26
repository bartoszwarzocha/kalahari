#!/bin/bash
# Kalahari Project Health Dashboard
# Comprehensive health check covering all project aspects
# Part of Quality Assurance Framework

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Health score tracking
HEALTH_SCORE=0
HEALTH_TOTAL=0

# Issue tracking
ISSUES=()
WARNINGS=()
INFO=()

# ============================================================================
# HEADER
# ============================================================================

echo ""
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║                                                               ║"
echo "║          🏥 KALAHARI PROJECT HEALTH DASHBOARD 🏥              ║"
echo "║                                                               ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""
echo "Timestamp: $(date '+%Y-%m-%d %H:%M:%S')"
echo "Location:  $(pwd)"
echo ""

# ============================================================================
# 1. DOCUMENTATION CONSISTENCY
# ============================================================================

echo "════════════════════════════════════════════════════════════════"
echo -e "${BLUE}📚 1. DOCUMENTATION CONSISTENCY${NC}"
echo "════════════════════════════════════════════════════════════════"
echo ""

# Check critical documentation files
CRITICAL_DOCS=(
    "CLAUDE.md"
    "CHANGELOG.md"
    "ROADMAP.md"
    "README.md"
    ".claude/QUALITY_CHECKLIST.md"
)

DOCS_PRESENT=0
for doc in "${CRITICAL_DOCS[@]}"; do
    HEALTH_TOTAL=$((HEALTH_TOTAL + 1))
    if [ -f "$doc" ]; then
        echo -e "   ${GREEN}✅${NC} $doc"
        DOCS_PRESENT=$((DOCS_PRESENT + 1))
        HEALTH_SCORE=$((HEALTH_SCORE + 1))
    else
        echo -e "   ${RED}❌${NC} $doc (MISSING)"
        ISSUES+=("Missing critical documentation: $doc")
    fi
done

echo ""
echo "   Critical docs: $DOCS_PRESENT / ${#CRITICAL_DOCS[@]}"

# Check project_docs/
if [ -d "project_docs" ]; then
    DOCS_COUNT=$(find project_docs -name "*.md" 2>/dev/null | wc -l)
    echo "   Detailed docs: $DOCS_COUNT files in project_docs/"

    # Check for expected docs
    EXPECTED_DOCS=11  # From CLAUDE.md
    if [ "$DOCS_COUNT" -ge "$EXPECTED_DOCS" ]; then
        echo -e "   ${GREEN}✅${NC} Expected $EXPECTED_DOCS documents, found $DOCS_COUNT"
    else
        echo -e "   ${YELLOW}⚠️${NC}  Expected $EXPECTED_DOCS documents, found only $DOCS_COUNT"
        WARNINGS+=("project_docs/ has only $DOCS_COUNT documents, expected $EXPECTED_DOCS")
    fi
else
    echo -e "   ${RED}❌${NC} project_docs/ directory missing"
    ISSUES+=("project_docs/ directory missing")
fi

# Check CLAUDE.md references
if [ -f "CLAUDE.md" ]; then
    if grep -q "ROADMAP.md" CLAUDE.md; then
        echo -e "   ${GREEN}✅${NC} CLAUDE.md references ROADMAP.md"
    else
        echo -e "   ${YELLOW}⚠️${NC}  CLAUDE.md missing ROADMAP.md reference"
        WARNINGS+=("CLAUDE.md doesn't reference ROADMAP.md")
    fi

    if grep -q "CHANGELOG.md" CLAUDE.md; then
        echo -e "   ${GREEN}✅${NC} CLAUDE.md references CHANGELOG.md"
    else
        echo -e "   ${YELLOW}⚠️${NC}  CLAUDE.md missing CHANGELOG.md reference"
        WARNINGS+=("CLAUDE.md doesn't reference CHANGELOG.md")
    fi
fi

echo ""

# ============================================================================
# 2. CLAUDE CODE RESOURCES
# ============================================================================

echo "════════════════════════════════════════════════════════════════"
echo -e "${BLUE}🤖 2. CLAUDE CODE RESOURCES${NC}"
echo "════════════════════════════════════════════════════════════════"
echo ""

# Skills
SKILLS_COUNT=$(find .claude/skills -maxdepth 1 -type d ! -name skills 2>/dev/null | wc -l || echo 0)
HEALTH_TOTAL=$((HEALTH_TOTAL + 1))

if [ "$SKILLS_COUNT" -ge 3 ]; then
    echo -e "   ${GREEN}✅${NC} Skills: $SKILLS_COUNT (expected 3+)"
    HEALTH_SCORE=$((HEALTH_SCORE + 1))

    find .claude/skills -maxdepth 1 -type d ! -name skills -exec basename {} \; | while read skill; do
        if [ -f ".claude/skills/$skill/SKILL.md" ]; then
            echo -e "      ${GREEN}✅${NC} $skill (SKILL.md present)"
        else
            echo -e "      ${RED}❌${NC} $skill (SKILL.md MISSING)"
            ISSUES+=("Skill $skill missing SKILL.md")
        fi
    done
else
    echo -e "   ${RED}❌${NC} Skills: $SKILLS_COUNT (expected 3+)"
    ISSUES+=("Only $SKILLS_COUNT skills found, expected 3+")
fi

echo ""

# Slash Commands
COMMANDS_COUNT=$(find .claude/commands -name "*.md" 2>/dev/null | wc -l || echo 0)
HEALTH_TOTAL=$((HEALTH_TOTAL + 1))

if [ "$COMMANDS_COUNT" -ge 6 ]; then
    echo -e "   ${GREEN}✅${NC} Commands: $COMMANDS_COUNT (expected 6+)"
    HEALTH_SCORE=$((HEALTH_SCORE + 1))

    find .claude/commands -name "*.md" -exec basename {} .md \; | while read cmd; do
        if grep -q "^description:" ".claude/commands/$cmd.md" 2>/dev/null; then
            echo -e "      ${GREEN}✅${NC} /$cmd (valid frontmatter)"
        else
            echo -e "      ${YELLOW}⚠️${NC}  /$cmd (missing frontmatter)"
            WARNINGS+=("Command /$cmd missing frontmatter")
        fi
    done
else
    echo -e "   ${RED}❌${NC} Commands: $COMMANDS_COUNT (expected 6+)"
    ISSUES+=("Only $COMMANDS_COUNT commands found, expected 6+")
fi

echo ""

# Agents
AGENTS_COUNT=$(find .claude/agents -name "*.md" 2>/dev/null | wc -l || echo 0)
HEALTH_TOTAL=$((HEALTH_TOTAL + 1))

if [ "$AGENTS_COUNT" -ge 6 ]; then
    echo -e "   ${GREEN}✅${NC} Agents: $AGENTS_COUNT (expected 6+)"
    HEALTH_SCORE=$((HEALTH_SCORE + 1))

    find .claude/agents -name "*.md" -exec basename {} .md \; | while read agent; do
        echo -e "      ${GREEN}✅${NC} $agent"
    done
else
    echo -e "   ${RED}❌${NC} Agents: $AGENTS_COUNT (expected 6+)"
    ISSUES+=("Only $AGENTS_COUNT agents found, expected 6+")
fi

echo ""

# Hooks
HOOKS_COUNT=$(find .claude/hooks -name "*.sh" -type f 2>/dev/null | wc -l || echo 0)
HEALTH_TOTAL=$((HEALTH_TOTAL + 1))

if [ "$HOOKS_COUNT" -ge 3 ]; then
    echo -e "   ${GREEN}✅${NC} Hooks: $HOOKS_COUNT (expected 3+)"
    HEALTH_SCORE=$((HEALTH_SCORE + 1))

    find .claude/hooks -name "*.sh" -type f -exec basename {} \; | while read hook; do
        if [ -x ".claude/hooks/$hook" ]; then
            echo -e "      ${GREEN}✅${NC} $hook (executable)"
        else
            echo -e "      ${YELLOW}⚠️${NC}  $hook (not executable)"
            WARNINGS+=("Hook $hook is not executable")
        fi
    done
else
    echo -e "   ${RED}❌${NC} Hooks: $HOOKS_COUNT (expected 3+)"
    ISSUES+=("Only $HOOKS_COUNT hooks found, expected 3+")
fi

echo ""

# ============================================================================
# 3. CODE QUALITY TOOLS
# ============================================================================

echo "════════════════════════════════════════════════════════════════"
echo -e "${BLUE}🔧 3. CODE QUALITY TOOLS${NC}"
echo "════════════════════════════════════════════════════════════════"
echo ""

# .clang-format
HEALTH_TOTAL=$((HEALTH_TOTAL + 1))
if [ -f ".clang-format" ]; then
    echo -e "   ${GREEN}✅${NC} .clang-format configured"
    HEALTH_SCORE=$((HEALTH_SCORE + 1))

    if command -v clang-format &> /dev/null; then
        CLANG_FORMAT_VERSION=$(clang-format --version | head -1)
        echo -e "      Tool: $CLANG_FORMAT_VERSION"
    else
        echo -e "      ${YELLOW}⚠️${NC}  clang-format tool not installed"
        WARNINGS+=("clang-format tool not installed")
    fi
else
    echo -e "   ${RED}❌${NC} .clang-format missing"
    ISSUES+=(".clang-format configuration missing")
fi

# .clang-tidy
HEALTH_TOTAL=$((HEALTH_TOTAL + 1))
if [ -f ".clang-tidy" ]; then
    echo -e "   ${GREEN}✅${NC} .clang-tidy configured"
    HEALTH_SCORE=$((HEALTH_SCORE + 1))

    if command -v clang-tidy &> /dev/null; then
        CLANG_TIDY_VERSION=$(clang-tidy --version | head -1)
        echo -e "      Tool: $CLANG_TIDY_VERSION"
    else
        echo -e "      ${YELLOW}⚠️${NC}  clang-tidy tool not installed"
        WARNINGS+=("clang-tidy tool not installed")
    fi
else
    echo -e "   ${RED}❌${NC} .clang-tidy missing"
    ISSUES+=(".clang-tidy configuration missing")
fi

# Doxygen
HEALTH_TOTAL=$((HEALTH_TOTAL + 1))
if [ -f "Doxyfile" ]; then
    echo -e "   ${GREEN}✅${NC} Doxyfile configured"
    HEALTH_SCORE=$((HEALTH_SCORE + 1))

    if command -v doxygen &> /dev/null; then
        DOXYGEN_VERSION=$(doxygen --version)
        echo -e "      Tool: Doxygen $DOXYGEN_VERSION"
    else
        echo -e "      ${YELLOW}⚠️${NC}  Doxygen not installed"
        WARNINGS+=("Doxygen not installed")
    fi
else
    echo -e "   ${RED}❌${NC} Doxyfile missing"
    ISSUES+=("Doxyfile configuration missing")
fi

echo ""

# ============================================================================
# 4. BUILD SYSTEM
# ============================================================================

echo "════════════════════════════════════════════════════════════════"
echo -e "${BLUE}🏗️  4. BUILD SYSTEM${NC}"
echo "════════════════════════════════════════════════════════════════"
echo ""

# CMakeLists.txt
HEALTH_TOTAL=$((HEALTH_TOTAL + 1))
if [ -f "CMakeLists.txt" ]; then
    echo -e "   ${GREEN}✅${NC} CMakeLists.txt exists"
    HEALTH_SCORE=$((HEALTH_SCORE + 1))

    # Check for advanced features
    if grep -q "ENABLE_ASAN" CMakeLists.txt; then
        echo -e "      ${GREEN}✅${NC} Sanitizers configured"
    else
        echo -e "      ${YELLOW}⚠️${NC}  Sanitizers not configured"
        INFO+=("Sanitizers (ASAN) not configured in CMakeLists.txt")
    fi

    if grep -q "kalahari_add_pch" CMakeLists.txt || grep -q "target_precompile_headers" CMakeLists.txt; then
        echo -e "      ${GREEN}✅${NC} PCH support configured"
    else
        echo -e "      ${YELLOW}⚠️${NC}  PCH support not configured"
        INFO+=("Precompiled headers not configured")
    fi

    if grep -q "ENABLE_CLANG_TIDY" CMakeLists.txt; then
        echo -e "      ${GREEN}✅${NC} clang-tidy integration available"
    else
        echo -e "      ${YELLOW}⚠️${NC}  clang-tidy integration not available"
        INFO+=("clang-tidy CMake integration missing")
    fi
else
    echo -e "   ${RED}❌${NC} CMakeLists.txt missing"
    ISSUES+=("CMakeLists.txt missing")
fi

echo ""

# vcpkg.json
HEALTH_TOTAL=$((HEALTH_TOTAL + 1))
if [ -f "vcpkg.json" ]; then
    echo -e "   ${GREEN}✅${NC} vcpkg.json exists"
    HEALTH_SCORE=$((HEALTH_SCORE + 1))

    # Validate JSON syntax
    if command -v jq &> /dev/null; then
        if jq empty vcpkg.json 2>/dev/null; then
            echo -e "      ${GREEN}✅${NC} Valid JSON"
        else
            echo -e "      ${RED}❌${NC} Invalid JSON syntax"
            ISSUES+=("vcpkg.json has invalid JSON syntax")
        fi

        # Count dependencies
        DEP_COUNT=$(jq '.dependencies | length' vcpkg.json 2>/dev/null || echo 0)
        echo -e "      Dependencies: $DEP_COUNT declared"
    else
        echo -e "      ${YELLOW}⚠️${NC}  jq not installed (cannot validate JSON)"
    fi
else
    echo -e "   ${RED}❌${NC} vcpkg.json missing"
    ISSUES+=("vcpkg.json missing")
fi

echo ""

# vcpkg submodule
HEALTH_TOTAL=$((HEALTH_TOTAL + 1))
if [ -d "vcpkg" ]; then
    echo -e "   ${GREEN}✅${NC} vcpkg/ submodule present"
    HEALTH_SCORE=$((HEALTH_SCORE + 1))

    if [ -f "vcpkg/vcpkg" ] || [ -f "vcpkg/vcpkg.exe" ]; then
        echo -e "      ${GREEN}✅${NC} vcpkg executable found"
    else
        echo -e "      ${YELLOW}⚠️${NC}  vcpkg not bootstrapped"
        WARNINGS+=("vcpkg submodule not bootstrapped (run ./bootstrap-vcpkg.sh)")
    fi
else
    echo -e "   ${RED}❌${NC} vcpkg/ submodule missing"
    ISSUES+=("vcpkg submodule missing (run: git submodule update --init)")
fi

echo ""

# Build directory
HEALTH_TOTAL=$((HEALTH_TOTAL + 1))
if [ -d "build" ]; then
    echo -e "   ${GREEN}✅${NC} build/ directory exists"
    HEALTH_SCORE=$((HEALTH_SCORE + 1))

    if [ -f "build/CMakeCache.txt" ]; then
        BUILD_TYPE=$(grep "CMAKE_BUILD_TYPE:STRING=" build/CMakeCache.txt | cut -d'=' -f2 2>/dev/null || echo 'Unknown')
        echo -e "      Build Type: $BUILD_TYPE"

        # Check for binaries
        if [ -f "build/bin/kalahari" ] || [ -f "build/bin/kalahari.exe" ]; then
            echo -e "      ${GREEN}✅${NC} Main executable built"
        else
            echo -e "      ${YELLOW}⚠️${NC}  Main executable not built"
            INFO+=("Main executable not yet built (run: cmake --build build)")
        fi

        # Check for test binary
        if [ -f "build/bin/kalahari-tests" ] || [ -f "build/bin/kalahari-tests.exe" ]; then
            echo -e "      ${GREEN}✅${NC} Test executable built"
        else
            echo -e "      ${YELLOW}⚠️${NC}  Test executable not built"
            INFO+=("Test executable not yet built")
        fi
    else
        echo -e "      ${YELLOW}⚠️${NC}  build/ not configured (run: cmake -B build)"
        INFO+=("Build directory exists but not configured")
    fi
else
    echo -e "   ${YELLOW}⚠️${NC}  build/ directory missing"
    INFO+=("Build directory not created (run: cmake -B build)")
fi

echo ""

# cmake/ directory
if [ -d "cmake" ]; then
    CMAKE_MODULES=$(find cmake -name "*.cmake" 2>/dev/null | wc -l)
    echo -e "   ${GREEN}✅${NC} cmake/ modules: $CMAKE_MODULES file(s)"

    if [ -f "cmake/PrecompiledHeaders.cmake" ]; then
        echo -e "      ${GREEN}✅${NC} PrecompiledHeaders.cmake present"
    fi
else
    echo -e "   ${YELLOW}⚠️${NC}  cmake/ directory missing"
    INFO+=("cmake/ directory for custom modules not created")
fi

echo ""

# ============================================================================
# 5. SOURCE STRUCTURE
# ============================================================================

echo "════════════════════════════════════════════════════════════════"
echo -e "${BLUE}📁 5. SOURCE STRUCTURE (MVP Pattern)${NC}"
echo "════════════════════════════════════════════════════════════════"
echo ""

# Expected directories
EXPECTED_DIRS=(
    "src/core/model"
    "src/core/utils"
    "src/gui/views"
    "src/gui/widgets"
    "src/presenters"
    "src/services"
    "include/kalahari"
    "tests/core"
    "tests/gui"
    "tests/presenters"
    "tests/services"
)

DIRS_PRESENT=0
for dir in "${EXPECTED_DIRS[@]}"; do
    HEALTH_TOTAL=$((HEALTH_TOTAL + 1))
    if [ -d "$dir" ]; then
        FILE_COUNT=$(find "$dir" -type f 2>/dev/null | wc -l)
        echo -e "   ${GREEN}✅${NC} $dir ($FILE_COUNT files)"
        DIRS_PRESENT=$((DIRS_PRESENT + 1))
        HEALTH_SCORE=$((HEALTH_SCORE + 1))
    else
        echo -e "   ${YELLOW}⚠️${NC}  $dir (not created yet)"
        INFO+=("Directory $dir not created (Phase 0 infrastructure)")
    fi
done

echo ""
echo "   MVP directories: $DIRS_PRESENT / ${#EXPECTED_DIRS[@]}"

# Check for README files
if [ -f "src/README.md" ]; then
    echo -e "   ${GREEN}✅${NC} src/README.md (architecture guide)"
else
    echo -e "   ${YELLOW}⚠️${NC}  src/README.md missing"
    INFO+=("src/README.md not created")
fi

if [ -f "tests/README.md" ]; then
    echo -e "   ${GREEN}✅${NC} tests/README.md (testing guide)"
else
    echo -e "   ${YELLOW}⚠️${NC}  tests/README.md missing"
    INFO+=("tests/README.md not created")
fi

echo ""

# ============================================================================
# 6. GIT STATUS
# ============================================================================

echo "════════════════════════════════════════════════════════════════"
echo -e "${BLUE}🌿 6. GIT STATUS${NC}"
echo "════════════════════════════════════════════════════════════════"
echo ""

HEALTH_TOTAL=$((HEALTH_TOTAL + 1))
if git rev-parse --git-dir > /dev/null 2>&1; then
    echo -e "   ${GREEN}✅${NC} Git repository initialized"
    HEALTH_SCORE=$((HEALTH_SCORE + 1))

    # Branch
    BRANCH=$(git branch --show-current 2>/dev/null || echo 'DETACHED')
    echo -e "      Current branch: $BRANCH"

    # Remote
    if git remote -v | grep -q "origin"; then
        REMOTE=$(git remote get-url origin 2>/dev/null || echo 'N/A')
        echo -e "      Remote origin:  $REMOTE"
    else
        echo -e "      ${YELLOW}⚠️${NC}  No remote 'origin' configured"
        WARNINGS+=("No git remote 'origin' configured")
    fi

    # Uncommitted changes
    MODIFIED=$(git status --short | wc -l)
    if [ "$MODIFIED" -eq 0 ]; then
        echo -e "      ${GREEN}✅${NC} Working tree clean"
    else
        echo -e "      ${YELLOW}⚠️${NC}  $MODIFIED uncommitted file(s)"
        INFO+=("$MODIFIED uncommitted changes in working tree")
    fi

    # Recent commits
    COMMIT_COUNT=$(git rev-list --count HEAD 2>/dev/null || echo 0)
    echo -e "      Total commits:  $COMMIT_COUNT"

    RECENT=$(git log -1 --format='%h %s' 2>/dev/null || echo 'No commits')
    echo -e "      Latest commit:  $RECENT"

    # Check .gitignore
    if [ -f ".gitignore" ]; then
        echo -e "      ${GREEN}✅${NC} .gitignore present"
    else
        echo -e "      ${RED}❌${NC} .gitignore missing"
        ISSUES+=(".gitignore missing")
    fi
else
    echo -e "   ${RED}❌${NC} Not a git repository"
    ISSUES+=("Not a git repository (run: git init)")
fi

echo ""

# ============================================================================
# 7. CI/CD STATUS
# ============================================================================

echo "════════════════════════════════════════════════════════════════"
echo -e "${BLUE}⚙️  7. CI/CD STATUS${NC}"
echo "════════════════════════════════════════════════════════════════"
echo ""

# GitHub Actions workflows
WORKFLOWS_COUNT=$(find .github/workflows -name "*.yml" 2>/dev/null | wc -l || echo 0)
HEALTH_TOTAL=$((HEALTH_TOTAL + 1))

if [ "$WORKFLOWS_COUNT" -ge 3 ]; then
    echo -e "   ${GREEN}✅${NC} GitHub Actions: $WORKFLOWS_COUNT workflows"
    HEALTH_SCORE=$((HEALTH_SCORE + 1))

    find .github/workflows -name "*.yml" -exec basename {} \; | while read workflow; do
        echo -e "      ${GREEN}✅${NC} $workflow"
    done
else
    echo -e "   ${RED}❌${NC} GitHub Actions: $WORKFLOWS_COUNT workflows (expected 3)"
    ISSUES+=("Expected 3 CI workflows (Windows, Linux, macOS), found $WORKFLOWS_COUNT")
fi

echo ""

# GitHub CLI check
if command -v gh &> /dev/null; then
    echo -e "   ${GREEN}✅${NC} GitHub CLI (gh) installed"

    if gh auth status &> /dev/null 2>&1; then
        echo -e "      ${GREEN}✅${NC} Authenticated"

        # Get latest run
        LATEST_RUN=$(gh run list --limit 1 --json status,conclusion,workflowName 2>/dev/null | jq -r '.[0]' || echo '')

        if [ -n "$LATEST_RUN" ] && [ "$LATEST_RUN" != "null" ]; then
            STATUS=$(echo "$LATEST_RUN" | jq -r '.status')
            CONCLUSION=$(echo "$LATEST_RUN" | jq -r '.conclusion // "in_progress"')
            WORKFLOW=$(echo "$LATEST_RUN" | jq -r '.workflowName')

            if [ "$CONCLUSION" == "success" ]; then
                echo -e "      ${GREEN}✅${NC} Latest run: $WORKFLOW - SUCCESS"
            elif [ "$CONCLUSION" == "failure" ]; then
                echo -e "      ${RED}❌${NC} Latest run: $WORKFLOW - FAILED"
                WARNINGS+=("Latest CI run failed: $WORKFLOW")
            else
                echo -e "      ${YELLOW}🔄${NC} Latest run: $WORKFLOW - $STATUS"
            fi
        else
            echo -e "      ${YELLOW}⚠️${NC}  No workflow runs found"
        fi
    else
        echo -e "      ${YELLOW}⚠️${NC}  Not authenticated (run: gh auth login)"
        INFO+=("GitHub CLI not authenticated")
    fi
else
    echo -e "   ${YELLOW}⚠️${NC}  GitHub CLI (gh) not installed"
    INFO+=("GitHub CLI not installed (install: https://cli.github.com/)")
fi

echo ""

# ============================================================================
# 8. WORK SCRIPTS
# ============================================================================

echo "════════════════════════════════════════════════════════════════"
echo -e "${BLUE}🛠️  8. WORK SCRIPTS${NC}"
echo "════════════════════════════════════════════════════════════════"
echo ""

# check-ci.sh
HEALTH_TOTAL=$((HEALTH_TOTAL + 1))
if [ -f "tools/check-ci.sh" ]; then
    if [ -x "tools/check-ci.sh" ]; then
        echo -e "   ${GREEN}✅${NC} tools/check-ci.sh (CI monitoring)"
        HEALTH_SCORE=$((HEALTH_SCORE + 1))
    else
        echo -e "   ${YELLOW}⚠️${NC}  tools/check-ci.sh (not executable)"
        WARNINGS+=("tools/check-ci.sh exists but not executable")
    fi
else
    echo -e "   ${RED}❌${NC} tools/check-ci.sh missing"
    ISSUES+=("tools/check-ci.sh missing")
fi

# health-check.sh (this script)
if [ -f "tools/health-check.sh" ]; then
    echo -e "   ${GREEN}✅${NC} tools/health-check.sh (this script)"
else
    echo -e "   ${YELLOW}⚠️${NC}  tools/health-check.sh (not found - running from memory?)"
fi

# Check tools/ directory
if [ -d "work" ]; then
    WORK_COUNT=$(find work -name "*.sh" -type f 2>/dev/null | wc -l)
    echo -e "   ${GREEN}✅${NC} tools/ directory: $WORK_COUNT script(s)"
else
    echo -e "   ${RED}❌${NC} tools/ directory missing"
    ISSUES+=("tools/ directory missing")
fi

echo ""

# ============================================================================
# HEALTH SCORE CALCULATION
# ============================================================================

echo "════════════════════════════════════════════════════════════════"
echo -e "${BLUE}📊 HEALTH SCORE SUMMARY${NC}"
echo "════════════════════════════════════════════════════════════════"
echo ""

HEALTH_PERCENT=$(( HEALTH_SCORE * 100 / HEALTH_TOTAL ))

echo "   Health Score: $HEALTH_SCORE / $HEALTH_TOTAL checks passed"
echo ""

if [ "$HEALTH_PERCENT" -ge 90 ]; then
    echo -e "   ${GREEN}✅ EXCELLENT${NC} ($HEALTH_PERCENT%)"
    echo "      Project is in excellent health!"
elif [ "$HEALTH_PERCENT" -ge 75 ]; then
    echo -e "   ${GREEN}✅ GOOD${NC} ($HEALTH_PERCENT%)"
    echo "      Project is healthy with minor issues."
elif [ "$HEALTH_PERCENT" -ge 60 ]; then
    echo -e "   ${YELLOW}⚠️  FAIR${NC} ($HEALTH_PERCENT%)"
    echo "      Project needs attention in some areas."
else
    echo -e "   ${RED}❌ POOR${NC} ($HEALTH_PERCENT%)"
    echo "      Project requires significant improvements."
fi

echo ""

# ============================================================================
# ISSUES, WARNINGS, INFO
# ============================================================================

if [ ${#ISSUES[@]} -gt 0 ]; then
    echo "════════════════════════════════════════════════════════════════"
    echo -e "${RED}❌ CRITICAL ISSUES (${#ISSUES[@]})${NC}"
    echo "════════════════════════════════════════════════════════════════"
    echo ""
    for issue in "${ISSUES[@]}"; do
        echo -e "   ${RED}•${NC} $issue"
    done
    echo ""
fi

if [ ${#WARNINGS[@]} -gt 0 ]; then
    echo "════════════════════════════════════════════════════════════════"
    echo -e "${YELLOW}⚠️  WARNINGS (${#WARNINGS[@]})${NC}"
    echo "════════════════════════════════════════════════════════════════"
    echo ""
    for warning in "${WARNINGS[@]}"; do
        echo -e "   ${YELLOW}•${NC} $warning"
    done
    echo ""
fi

if [ ${#INFO[@]} -gt 0 ]; then
    echo "════════════════════════════════════════════════════════════════"
    echo -e "${BLUE}ℹ️  INFORMATIONAL (${#INFO[@]})${NC}"
    echo "════════════════════════════════════════════════════════════════"
    echo ""
    for info in "${INFO[@]}"; do
        echo -e "   ${BLUE}•${NC} $info"
    done
    echo ""
fi

# ============================================================================
# RECOMMENDATIONS
# ============================================================================

echo "════════════════════════════════════════════════════════════════"
echo -e "${BLUE}💡 RECOMMENDATIONS${NC}"
echo "════════════════════════════════════════════════════════════════"
echo ""

if [ ${#ISSUES[@]} -gt 0 ]; then
    echo "   🔴 Address critical issues immediately"
fi

if [ "$HEALTH_PERCENT" -lt 75 ]; then
    echo "   📚 Review .claude/QUALITY_CHECKLIST.md"
    echo "   🔧 Run: /health-check --detailed"
fi

if [ ! -d "build" ] || [ ! -f "build/CMakeCache.txt" ]; then
    echo "   🏗️  Configure build: cmake -B build"
    echo "   🏗️  Build project: cmake --build build"
fi

if command -v gh &> /dev/null && gh auth status &> /dev/null 2>&1; then
    echo "   ⚙️  Monitor CI/CD: ./tools/check-ci.sh status"
fi

echo "   🔍 Run code review: /code-review --scope=."
echo "   🏛️  Check architecture: /architecture-review"

echo ""

# ============================================================================
# FOOTER
# ============================================================================

echo "════════════════════════════════════════════════════════════════"
echo ""
echo "Health check completed at $(date '+%Y-%m-%d %H:%M:%S')"
echo ""
echo "For detailed quality requirements, see:"
echo "   .claude/QUALITY_CHECKLIST.md"
echo ""
echo "════════════════════════════════════════════════════════════════"
echo ""

# Exit with status based on health score
if [ "$HEALTH_PERCENT" -ge 75 ]; then
    exit 0
else
    exit 1
fi
