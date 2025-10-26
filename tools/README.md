# Kalahari Work Scripts

**Directory:** `tools/`
**Purpose:** Development and quality assurance automation scripts
**Status:** Active (3 scripts)
**Last Updated:** 2025-10-26

---

## 📋 Overview

This directory contains **standalone bash scripts** for project monitoring, quality assurance, and CI/CD integration. These scripts are **NOT called automatically** by Claude Code agents, skills, or commands - they are **manual tools** for developers and maintainers.

**Key Principle:** Each script serves a distinct purpose with minimal overlap.

---

## 📊 Script Inventory

| Script | Purpose | Size | Lines | Type |
|--------|---------|------|-------|------|
| `check-ci.sh` | CI/CD monitoring via GitHub CLI | 6.4KB | 226 | External monitoring |
| `pre-commit-check.sh` | Automated quality gates (blocks commits) | 17KB | 538 | Quality enforcement |
| `project-status.sh` | Overall project health dashboard | 29KB | 751 | Manual overview |

**Total:** 3 scripts, 1,515 lines, 52.4KB

---

## 🔧 Script Details

### 1. check-ci.sh - CI/CD Monitoring

**Purpose:** Monitor GitHub Actions workflow runs from the command line.

**Dependencies:**
- GitHub CLI (`gh`) - [Install guide](https://cli.github.com/)
- Authenticated GitHub session (`gh auth login`)
- `jq` for JSON parsing

**Usage:**

```bash
# Show latest workflow status (default)
./tools/check-ci.sh
./tools/check-ci.sh status

# Watch runs in real-time (auto-refresh)
./tools/check-ci.sh watch

# List last N runs
./tools/check-ci.sh list 20

# Download logs for specific run
./tools/check-ci.sh logs 12345678

# Show detailed summary of latest run
./tools/check-ci.sh summary

# Help
./tools/check-ci.sh --help
```

**Features:**
- ✅ Real-time workflow monitoring
- 📊 Status color coding (✅ success, ❌ failure, 🔄 in_progress)
- 📥 Log downloading
- 🔍 Detailed run summaries

**When to use:**
- Before creating pull requests (verify CI/CD passing)
- Debugging CI/CD failures
- Monitoring deployment pipelines
- Quick status check without opening browser

**Referenced in:**
- `.claude/QUALITY_CHECKLIST.md` (line 35): CI/CD verification for releases
- `CLAUDE.md` (line 499): Quick Start guide
- `project-status.sh` (lines 600-612): Work scripts verification

---

### 2. pre-commit-check.sh - Quality Gates

**Purpose:** Automated quality verification before commits. **BLOCKS commits** if quality score < 70%.

**Dependencies:**
- `clang-format` (optional, C++ formatting)
- Standard Unix tools: `grep`, `find`, `wc`

**Usage:**

```bash
# Run all checks (recommended before every commit)
./tools/pre-commit-check.sh

# Typical output:
# 📊 QUALITY SCORE: 16 / 17 (94%)
# ✅ EXCELLENT QUALITY (90%+)
# Ready to commit!
```

**Exit Codes:**
- `0` - Quality acceptable (≥70%)
- `1` - Quality failed (<70%) - **BLOCKS commit**

**Quality Gates:**
- **< 70%**: ❌ QUALITY GATE FAILED (exit 1)
- **70-89%**: ⚠️ ACCEPTABLE BUT NEEDS IMPROVEMENT (exit 0)
- **90-100%**: ✅ EXCELLENT QUALITY (exit 0)

**12 Check Categories** (35+ checks total):

1. **Code Formatting**
   - `.clang-format` file exists
   - All C++ files properly formatted

2. **Naming Conventions**
   - Member variables use `m_` prefix
   - File names use `snake_case`

3. **Modern C++ Practices**
   - No raw `new`/`delete` usage
   - Smart pointers preferred

4. **Documentation**
   - Doxygen comments (`/// @brief`)
   - Minimal commented-out code

5. **Architecture Compliance (MVP)**
   - No wxWidgets in Model layer (separation of concerns)

6. **Internationalization**
   - UI strings wrapped in `_("...")`

7. **Build System**
   - `CMakeLists.txt` exists and consistent
   - `vcpkg.json` exists and synchronized

8. **Documentation Consistency**
   - `CLAUDE.md`, `CHANGELOG.md`, `ROADMAP.md` exist
   - Recent `CHANGELOG.md` entries (within 7 days)

9. **Code Annotations**
   - TODO/FIXME count (<10 acceptable)

10. **Security**
    - No hardcoded secrets (passwords, API keys, tokens)

11. **Testing**
    - Test coverage ratio (tests/ vs src/)

12. **File Size**
    - Warning for files >1000 lines

**When to use:**
- **Before every commit** (recommended workflow)
- After major refactoring
- Before creating pull requests
- During code reviews

**Git Hook Integration** (optional):

```bash
# Create pre-commit hook
cat > .git/hooks/pre-commit << 'EOF'
#!/bin/bash
./tools/pre-commit-check.sh
exit $?
EOF

chmod +x .git/hooks/pre-commit
```

**Referenced in:**
- `.claude/QUALITY_CHECKLIST.md` (lines 16, 29): Automated checks for releases
- `.claude/settings.local.json` (line 135): Permission allowlist
- `CLAUDE.md` (line 496): Quick Start guide

---

### 3. project-status.sh - Health Dashboard

**Purpose:** Comprehensive project health overview (read-only, non-blocking).

**Dependencies:**
- Standard Unix tools: `grep`, `find`, `wc`, `git`
- Optional: `clang-format`, `cmake`, `doxygen` (for full checks)

**Usage:**

```bash
# Run full health check
./tools/project-status.sh

# Typical output:
# ╔═══════════════════════════════════════════════════════════════╗
# ║          🏥 KALAHARI PROJECT HEALTH DASHBOARD 🏥              ║
# ╚═══════════════════════════════════════════════════════════════╝
#
# 📚 1. DOCUMENTATION CONSISTENCY
# 📊 2. CLAUDE CODE RESOURCES
# 🛠️  3. CODE QUALITY TOOLS
# 🏗️  4. BUILD SYSTEM
# 📁 5. SOURCE STRUCTURE
# 🔧 6. GIT REPOSITORY
# 🔄 7. CI/CD STATUS
# 📝 8. WORK SCRIPTS
#
# ═══════════════════════════════════════════════════════════════
# 🏥 OVERALL PROJECT HEALTH: 85% (GOOD)
# ═══════════════════════════════════════════════════════════════
```

**8 Health Categories:**

1. **Documentation Consistency**
   - Critical docs existence (CLAUDE.md, CHANGELOG.md, ROADMAP.md, etc.)
   - `project_docs/` completeness (11 expected documents)
   - Cross-references verification

2. **Claude Code Resources**
   - Skills count (expected 3+)
   - Slash commands count (expected 6+)
   - Agents count (expected 6+)

3. **Code Quality Tools**
   - `clang-format` availability
   - `clang-tidy` availability
   - `doxygen` availability

4. **Build System**
   - CMake installation and version
   - vcpkg installation
   - Build directory status
   - Executable binaries status

5. **Source Structure**
   - MVP directory structure (src/, include/, tests/, etc.)

6. **Git Repository**
   - Git initialization
   - Clean working tree status
   - Current branch tracking

7. **CI/CD Status**
   - GitHub Actions configuration
   - CI/CD script availability (check-ci.sh)

8. **Work Scripts**
   - Work directory scripts verification
   - Executable permissions

**Health Score:**
- **90-100%**: ✅ EXCELLENT
- **70-89%**: ⚠️ GOOD (minor issues)
- **50-69%**: ⚠️ NEEDS ATTENTION
- **< 50%**: ❌ CRITICAL ISSUES

**When to use:**
- Weekly project health check
- After major structural changes
- Before starting new development phase
- Onboarding new contributors

**Difference from pre-commit-check.sh:**
- **pre-commit-check.sh**: CODE QUALITY (blocks commits)
- **project-status.sh**: PROJECT OVERVIEW (informational)

**Referenced in:**
- `CLAUDE.md` (line 493): Quick Start guide (renamed from health-check.sh)

---

## 🔗 Integration Points

### Where These Scripts Are Referenced

**`.claude/QUALITY_CHECKLIST.md`:**
- Uses `pre-commit-check.sh` for automated quality checks (lines 16, 29)
- Uses `check-ci.sh` for CI/CD verification (line 35)

**`.claude/settings.local.json`:**
- Permission for `Bash(./tools/pre-commit-check.sh:*)` (line 135)

**`CLAUDE.md` - Quick Start Section:**
```bash
# Automated file/tool checks
./tools/project-status.sh

# Pre-commit quality verification
./tools/pre-commit-check.sh

# CI/CD monitoring
./tools/check-ci.sh status
```

**NO automatic execution:**
- ✅ Scripts are **NOT** called by `.claude/agents/*.md`
- ✅ Scripts are **NOT** called by `.claude/commands/*.md`
- ✅ Scripts are **NOT** called by `.claude/skills/*/SKILL.md`
- ✅ Scripts are **standalone tools** for manual use

---

## 📊 Script Comparison Matrix

| Feature | check-ci.sh | pre-commit-check.sh | project-status.sh |
|---------|-------------|---------------------|-------------------|
| **Primary Focus** | CI/CD monitoring | Code quality gates | Project overview |
| **Execution Mode** | Manual | Manual (or git hook) | Manual |
| **Exit Code Matters** | ❌ No | ✅ Yes (blocks commits) | ❌ No |
| **External Services** | GitHub Actions | None | None |
| **Dependency Heavy** | Yes (gh CLI) | Minimal | Minimal |
| **Score/Rating** | ❌ No | ✅ Yes (0-100%) | ✅ Yes (0-100%) |
| **Can Block Commit** | ❌ No | ✅ Yes (<70% fails) | ❌ No |
| **Typical Runtime** | 2-5s | 3-10s | 5-15s |
| **Update Frequency** | Per commit | Per commit | Weekly |

**Overlap Analysis:**
- **Minimal overlap by design**
- `pre-commit-check.sh` checks **CODE** (formatting, naming, security)
- `project-status.sh` checks **PROJECT** (docs, structure, tools)
- `check-ci.sh` checks **EXTERNAL** (GitHub Actions)

---

## 🚀 Recommended Workflow

### Daily Development

```bash
# 1. Before starting work - check project health
./tools/project-status.sh

# 2. During development - check CI/CD status
./tools/check-ci.sh status

# 3. Before committing - verify code quality
./tools/pre-commit-check.sh

# 4. If quality passed (≥70%) - commit
git add .
git commit -m "feat: Add feature X"

# 5. After push - verify CI/CD passed
./tools/check-ci.sh watch
```

### Weekly Maintenance

```bash
# Monday morning - full project health check
./tools/project-status.sh > project-health-$(date +%Y-%m-%d).log

# Review health score and address warnings
```

### Pre-Release

```bash
# Ensure all checks pass
./tools/pre-commit-check.sh          # Must be 100%
./tools/project-status.sh            # Should be ≥90%
./tools/check-ci.sh status           # Must be ✅

# Then review .claude/QUALITY_CHECKLIST.md for manual verification
cat .claude/QUALITY_CHECKLIST.md
```

---

## 🛠️ Development Guidelines

### Adding New Checks

**To add checks to pre-commit-check.sh:**

1. Add new category section:
```bash
# ----------------------------------------------------------------------------
# 13. YOUR NEW CATEGORY
# ----------------------------------------------------------------------------

echo "🔍 13. YOUR NEW CATEGORY"

check "Description of check" "command to run"

echo ""
```

2. Update total check count in documentation
3. Test with various scenarios
4. Update this README

**To add checks to project-status.sh:**

1. Add new category section (numbered 9+)
2. Update `HEALTH_TOTAL` counter
3. Update this README

### Script Maintenance

**All scripts follow Bash best practices:**
- ✅ `set -euo pipefail` (exit on error, undefined vars, pipe failures)
- ✅ Proper error handling
- ✅ Color-coded output
- ✅ Informative messages
- ✅ Exit codes (where applicable)

**When modifying:**
1. Test on all platforms (Windows WSL, macOS, Linux)
2. Ensure backward compatibility
3. Update help/usage text
4. Update this README

---

## 📝 Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-10-26 | Initial creation of all 3 scripts |
| 1.1 | 2025-10-26 | Renamed health-check.sh → project-status.sh (avoid /health-check conflict) |
| 1.2 | 2025-10-26 | Added tools/README.md documentation |

---

## ❓ FAQ

**Q: Why three scripts instead of one?**
A: Each serves a distinct purpose:
- `check-ci.sh` - External CI/CD monitoring (requires gh CLI)
- `pre-commit-check.sh` - Code quality enforcement (blocks bad commits)
- `project-status.sh` - Project health overview (informational)

**Q: Should I run these scripts automatically?**
A: `pre-commit-check.sh` can be automated via git hook. Others are manual tools.

**Q: What's the difference between pre-commit-check.sh and project-status.sh?**
A:
- `pre-commit-check.sh`: **CODE QUALITY** - blocks commits if <70%
- `project-status.sh`: **PROJECT OVERVIEW** - informational dashboard

**Q: Do Claude Code agents/skills/commands call these scripts?**
A: **NO.** These are standalone tools. They are *referenced* in documentation but never executed automatically.

**Q: Why was health-check.sh renamed to project-status.sh?**
A: To avoid naming conflict with `/health-check` slash command (AI-driven analysis) vs `project-status.sh` (automated file checks).

**Q: Can I skip pre-commit-check.sh?**
A: You *can*, but it's **strongly recommended** to run before every commit. It catches common issues early.

**Q: What if check-ci.sh fails to authenticate?**
A: Run `gh auth login` to authenticate GitHub CLI.

---

## 📚 References

- **CLAUDE.md**: Master project document (lines 493-499: Quick Start)
- **QUALITY_CHECKLIST.md**: Release verification checklist
- **ROADMAP.md**: Project phases and milestones
- **GitHub Actions**: `.github/workflows/build.yml` (CI/CD configuration)

---

**Document Version:** 1.0
**Maintained By:** Project Lead + Claude Code
**Last Review:** 2025-10-26
