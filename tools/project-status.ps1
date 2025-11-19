# Kalahari Project Health Dashboard (PowerShell)
# Comprehensive health check covering all project aspects
# Part of Quality Assurance Framework

$ErrorActionPreference = 'Continue'

# ============================================================================
# CONFIGURATION
# ============================================================================

# Health score tracking
$Script:HEALTH_SCORE = 0
$Script:HEALTH_TOTAL = 0

# Issue tracking
$Script:ISSUES = @()
$Script:WARNINGS = @()
$Script:INFO = @()

# ============================================================================
# HELPER FUNCTIONS
# ============================================================================

function Write-ColorOutput {
    param(
        [string]$Message,
        [ConsoleColor]$ForegroundColor = [ConsoleColor]::White
    )
    $originalColor = $Host.UI.RawUI.ForegroundColor
    $Host.UI.RawUI.ForegroundColor = $ForegroundColor
    Write-Host $Message
    $Host.UI.RawUI.ForegroundColor = $originalColor
}

function Test-HealthCheck {
    param(
        [string]$Name,
        [scriptblock]$Command
    )

    $Script:HEALTH_TOTAL++

    try {
        $result = & $Command
        if ($result) {
            Write-ColorOutput "   ✅ $Name" -ForegroundColor Green
            $Script:HEALTH_SCORE++
            return $true
        } else {
            Write-ColorOutput "   ❌ $Name (MISSING)" -ForegroundColor Red
            $Script:ISSUES += "Missing: $Name"
            return $false
        }
    } catch {
        Write-ColorOutput "   ❌ $Name (ERROR)" -ForegroundColor Red
        $Script:ISSUES += "Error checking: $Name"
        return $false
    }
}

# ============================================================================
# HEADER
# ============================================================================

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════════╗"
Write-Host "║                                                               ║"
Write-Host "║          🏥 KALAHARI PROJECT HEALTH DASHBOARD 🏥              ║"
Write-Host "║                                                               ║"
Write-Host "╚═══════════════════════════════════════════════════════════════╝"
Write-Host ""
Write-Host "Timestamp: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-Host "Location:  $(Get-Location)"
Write-Host ""

# ============================================================================
# 1. DOCUMENTATION CONSISTENCY
# ============================================================================

Write-Host "════════════════════════════════════════════════════════════════"
Write-ColorOutput "📚 1. DOCUMENTATION CONSISTENCY" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════════════"
Write-Host ""

# Check critical documentation files
$criticalDocs = @(
    "CLAUDE.md",
    "CHANGELOG.md",
    "ROADMAP.md",
    "README.md",
    ".claude/QUALITY_CHECKLIST.md"
)

$docsPresent = 0
foreach ($doc in $criticalDocs) {
    if (Test-HealthCheck $doc { Test-Path $doc }) {
        $docsPresent++
    }
}

Write-Host ""
Write-Host "   Critical docs: $docsPresent / $($criticalDocs.Count)"

# Check project_docs/
if (Test-Path "project_docs") {
    $docsCount = (Get-ChildItem -Path project_docs -Filter *.md -Recurse | Measure-Object).Count
    Write-Host "   Detailed docs: $docsCount files in project_docs/"

    $expectedDocs = 11
    if ($docsCount -ge $expectedDocs) {
        Write-ColorOutput "   ✅ Expected $expectedDocs documents, found $docsCount" -ForegroundColor Green
    } else {
        Write-ColorOutput "   ⚠️  Expected $expectedDocs documents, found only $docsCount" -ForegroundColor Yellow
        $Script:WARNINGS += "project_docs/ has only $docsCount documents, expected $expectedDocs"
    }
} else {
    Write-ColorOutput "   ❌ project_docs/ directory missing" -ForegroundColor Red
    $Script:ISSUES += "project_docs/ directory missing"
}

# Check CLAUDE.md references
if (Test-Path "CLAUDE.md") {
    $claudeContent = Get-Content "CLAUDE.md" -Raw

    if ($claudeContent -match 'ROADMAP\.md') {
        Write-ColorOutput "   ✅ CLAUDE.md references ROADMAP.md" -ForegroundColor Green
    } else {
        Write-ColorOutput "   ⚠️  CLAUDE.md missing ROADMAP.md reference" -ForegroundColor Yellow
        $Script:WARNINGS += "CLAUDE.md doesn't reference ROADMAP.md"
    }

    if ($claudeContent -match 'CHANGELOG\.md') {
        Write-ColorOutput "   ✅ CLAUDE.md references CHANGELOG.md" -ForegroundColor Green
    } else {
        Write-ColorOutput "   ⚠️  CLAUDE.md missing CHANGELOG.md reference" -ForegroundColor Yellow
        $Script:WARNINGS += "CLAUDE.md doesn't reference CHANGELOG.md"
    }
}

Write-Host ""

# ============================================================================
# 2. CLAUDE CODE RESOURCES
# ============================================================================

Write-Host "════════════════════════════════════════════════════════════════"
Write-ColorOutput "🤖 2. CLAUDE CODE RESOURCES" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════════════"
Write-Host ""

# Skills
$skillsCount = 0
if (Test-Path ".claude/skills") {
    $skillsCount = (Get-ChildItem -Path .claude/skills -Directory | Measure-Object).Count
}

$Script:HEALTH_TOTAL++
if ($skillsCount -ge 3) {
    Write-ColorOutput "   ✅ Skills: $skillsCount (expected 3+)" -ForegroundColor Green
    $Script:HEALTH_SCORE++

    Get-ChildItem -Path .claude/skills -Directory | ForEach-Object {
        $skillMd = Join-Path $_.FullName "SKILL.md"
        if (Test-Path $skillMd) {
            Write-ColorOutput "      ✅ $($_.Name) (SKILL.md present)" -ForegroundColor Green
        } else {
            Write-ColorOutput "      ❌ $($_.Name) (SKILL.md MISSING)" -ForegroundColor Red
            $Script:ISSUES += "Skill $($_.Name) missing SKILL.md"
        }
    }
} else {
    Write-ColorOutput "   ❌ Skills: $skillsCount (expected 3+)" -ForegroundColor Red
    $Script:ISSUES += "Only $skillsCount skills found, expected 3+"
}

Write-Host ""

# Slash Commands
$commandsCount = 0
if (Test-Path ".claude/commands") {
    $commandsCount = (Get-ChildItem -Path .claude/commands -Filter *.md | Measure-Object).Count
}

$Script:HEALTH_TOTAL++
if ($commandsCount -ge 6) {
    Write-ColorOutput "   ✅ Commands: $commandsCount (expected 6+)" -ForegroundColor Green
    $Script:HEALTH_SCORE++

    Get-ChildItem -Path .claude/commands -Filter *.md | ForEach-Object {
        $content = Get-Content $_.FullName -Raw
        if ($content -match '^description:') {
            Write-ColorOutput "      ✅ /$($_.BaseName) (valid frontmatter)" -ForegroundColor Green
        } else {
            Write-ColorOutput "      ⚠️  /$($_.BaseName) (missing frontmatter)" -ForegroundColor Yellow
            $Script:WARNINGS += "Command /$($_.BaseName) missing frontmatter"
        }
    }
} else {
    Write-ColorOutput "   ❌ Commands: $commandsCount (expected 6+)" -ForegroundColor Red
    $Script:ISSUES += "Only $commandsCount commands found, expected 6+"
}

Write-Host ""

# Agents
$agentsCount = 0
if (Test-Path ".claude/agents") {
    $agentsCount = (Get-ChildItem -Path .claude/agents -Filter *.md | Measure-Object).Count
}

$Script:HEALTH_TOTAL++
if ($agentsCount -ge 6) {
    Write-ColorOutput "   ✅ Agents: $agentsCount (expected 6+)" -ForegroundColor Green
    $Script:HEALTH_SCORE++

    Get-ChildItem -Path .claude/agents -Filter *.md | ForEach-Object {
        Write-ColorOutput "      ✅ $($_.BaseName)" -ForegroundColor Green
    }
} else {
    Write-ColorOutput "   ❌ Agents: $agentsCount (expected 6+)" -ForegroundColor Red
    $Script:ISSUES += "Only $agentsCount agents found, expected 6+"
}

Write-Host ""

# Hooks
$hooksCount = 0
if (Test-Path ".claude/hooks") {
    $hooksCount = (Get-ChildItem -Path .claude/hooks -Filter *.sh -File | Measure-Object).Count
}

$Script:HEALTH_TOTAL++
if ($hooksCount -ge 3) {
    Write-ColorOutput "   ✅ Hooks: $hooksCount (expected 3+)" -ForegroundColor Green
    $Script:HEALTH_SCORE++

    Get-ChildItem -Path .claude/hooks -Filter *.sh -File | ForEach-Object {
        # Note: Unix execute bit not applicable on Windows, but file should exist
        Write-ColorOutput "      ✅ $($_.Name)" -ForegroundColor Green
    }
} else {
    Write-ColorOutput "   ❌ Hooks: $hooksCount (expected 3+)" -ForegroundColor Red
    $Script:ISSUES += "Only $hooksCount hooks found, expected 3+"
}

Write-Host ""

# ============================================================================
# 3. CODE QUALITY TOOLS
# ============================================================================

Write-Host "════════════════════════════════════════════════════════════════"
Write-ColorOutput "🔧 3. CODE QUALITY TOOLS" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════════════"
Write-Host ""

# .clang-format
$Script:HEALTH_TOTAL++
if (Test-Path ".clang-format") {
    Write-ColorOutput "   ✅ .clang-format configured" -ForegroundColor Green
    $Script:HEALTH_SCORE++

    if (Get-Command clang-format -ErrorAction SilentlyContinue) {
        $version = (clang-format --version 2>&1 | Select-Object -First 1)
        Write-Host "      Tool: $version"
    } else {
        Write-ColorOutput "      ⚠️  clang-format tool not installed" -ForegroundColor Yellow
        $Script:WARNINGS += "clang-format tool not installed"
    }
} else {
    Write-ColorOutput "   ❌ .clang-format missing" -ForegroundColor Red
    $Script:ISSUES += ".clang-format configuration missing"
}

# .clang-tidy
$Script:HEALTH_TOTAL++
if (Test-Path ".clang-tidy") {
    Write-ColorOutput "   ✅ .clang-tidy configured" -ForegroundColor Green
    $Script:HEALTH_SCORE++

    if (Get-Command clang-tidy -ErrorAction SilentlyContinue) {
        $version = (clang-tidy --version 2>&1 | Select-Object -First 1)
        Write-Host "      Tool: $version"
    } else {
        Write-ColorOutput "      ⚠️  clang-tidy tool not installed" -ForegroundColor Yellow
        $Script:WARNINGS += "clang-tidy tool not installed"
    }
} else {
    Write-ColorOutput "   ❌ .clang-tidy missing" -ForegroundColor Red
    $Script:ISSUES += ".clang-tidy configuration missing"
}

# Doxygen
$Script:HEALTH_TOTAL++
if (Test-Path "Doxyfile") {
    Write-ColorOutput "   ✅ Doxyfile configured" -ForegroundColor Green
    $Script:HEALTH_SCORE++

    if (Get-Command doxygen -ErrorAction SilentlyContinue) {
        $version = (doxygen --version 2>&1)
        Write-Host "      Tool: Doxygen $version"
    } else {
        Write-ColorOutput "      ⚠️  Doxygen not installed" -ForegroundColor Yellow
        $Script:WARNINGS += "Doxygen not installed"
    }
} else {
    Write-ColorOutput "   ❌ Doxyfile missing" -ForegroundColor Red
    $Script:ISSUES += "Doxyfile configuration missing"
}

Write-Host ""

# ============================================================================
# 4. BUILD SYSTEM
# ============================================================================

Write-Host "════════════════════════════════════════════════════════════════"
Write-ColorOutput "🏗️  4. BUILD SYSTEM" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════════════"
Write-Host ""

# CMakeLists.txt
$Script:HEALTH_TOTAL++
if (Test-Path "CMakeLists.txt") {
    Write-ColorOutput "   ✅ CMakeLists.txt exists" -ForegroundColor Green
    $Script:HEALTH_SCORE++

    $cmakeContent = Get-Content "CMakeLists.txt" -Raw

    # Check for advanced features
    if ($cmakeContent -match 'ENABLE_ASAN') {
        Write-ColorOutput "      ✅ Sanitizers configured" -ForegroundColor Green
    } else {
        Write-ColorOutput "      ⚠️  Sanitizers not configured" -ForegroundColor Yellow
        $Script:INFO += "Sanitizers (ASAN) not configured in CMakeLists.txt"
    }

    if ($cmakeContent -match 'kalahari_add_pch|target_precompile_headers') {
        Write-ColorOutput "      ✅ PCH support configured" -ForegroundColor Green
    } else {
        Write-ColorOutput "      ⚠️  PCH support not configured" -ForegroundColor Yellow
        $Script:INFO += "Precompiled headers not configured"
    }

    if ($cmakeContent -match 'ENABLE_CLANG_TIDY') {
        Write-ColorOutput "      ✅ clang-tidy integration available" -ForegroundColor Green
    } else {
        Write-ColorOutput "      ⚠️  clang-tidy integration not available" -ForegroundColor Yellow
        $Script:INFO += "clang-tidy CMake integration missing"
    }
} else {
    Write-ColorOutput "   ❌ CMakeLists.txt missing" -ForegroundColor Red
    $Script:ISSUES += "CMakeLists.txt missing"
}

Write-Host ""

# vcpkg.json
$Script:HEALTH_TOTAL++
if (Test-Path "vcpkg.json") {
    Write-ColorOutput "   ✅ vcpkg.json exists" -ForegroundColor Green
    $Script:HEALTH_SCORE++

    # Validate JSON syntax
    try {
        $vcpkgJson = Get-Content "vcpkg.json" | ConvertFrom-Json
        Write-ColorOutput "      ✅ Valid JSON" -ForegroundColor Green

        $depCount = $vcpkgJson.dependencies.Count
        Write-Host "      Dependencies: $depCount declared"
    } catch {
        Write-ColorOutput "      ❌ Invalid JSON syntax" -ForegroundColor Red
        $Script:ISSUES += "vcpkg.json has invalid JSON syntax"
    }
} else {
    Write-ColorOutput "   ❌ vcpkg.json missing" -ForegroundColor Red
    $Script:ISSUES += "vcpkg.json missing"
}

Write-Host ""

# vcpkg submodule
$Script:HEALTH_TOTAL++
if (Test-Path "vcpkg") {
    Write-ColorOutput "   ✅ vcpkg/ submodule present" -ForegroundColor Green
    $Script:HEALTH_SCORE++

    if ((Test-Path "vcpkg/vcpkg") -or (Test-Path "vcpkg/vcpkg.exe")) {
        Write-ColorOutput "      ✅ vcpkg executable found" -ForegroundColor Green
    } else {
        Write-ColorOutput "      ⚠️  vcpkg not bootstrapped" -ForegroundColor Yellow
        $Script:WARNINGS += "vcpkg submodule not bootstrapped (run ./bootstrap-vcpkg.sh)"
    }
} else {
    Write-ColorOutput "   ❌ vcpkg/ submodule missing" -ForegroundColor Red
    $Script:ISSUES += "vcpkg submodule missing (run: git submodule update --init)"
}

Write-Host ""

# Build directory
$Script:HEALTH_TOTAL++
if (Test-Path "build") {
    Write-ColorOutput "   ✅ build/ directory exists" -ForegroundColor Green
    $Script:HEALTH_SCORE++

    if (Test-Path "build/CMakeCache.txt") {
        $cacheContent = Get-Content "build/CMakeCache.txt" -Raw
        if ($cacheContent -match 'CMAKE_BUILD_TYPE:STRING=(.+)') {
            $buildType = $Matches[1]
            Write-Host "      Build Type: $buildType"
        }

        # Check for binaries
        if ((Test-Path "build/bin/kalahari") -or (Test-Path "build/bin/kalahari.exe")) {
            Write-ColorOutput "      ✅ Main executable built" -ForegroundColor Green
        } else {
            Write-ColorOutput "      ⚠️  Main executable not built" -ForegroundColor Yellow
            $Script:INFO += "Main executable not yet built (run: cmake --build build)"
        }

        # Check for test binary
        if ((Test-Path "build/bin/kalahari-tests") -or (Test-Path "build/bin/kalahari-tests.exe")) {
            Write-ColorOutput "      ✅ Test executable built" -ForegroundColor Green
        } else {
            Write-ColorOutput "      ⚠️  Test executable not built" -ForegroundColor Yellow
            $Script:INFO += "Test executable not yet built"
        }
    } else {
        Write-ColorOutput "      ⚠️  build/ not configured (run: cmake -B build)" -ForegroundColor Yellow
        $Script:INFO += "Build directory exists but not configured"
    }
} else {
    Write-ColorOutput "   ⚠️  build/ directory missing" -ForegroundColor Yellow
    $Script:INFO += "Build directory not created (run: cmake -B build)"
}

Write-Host ""

# cmake/ directory
if (Test-Path "cmake") {
    $cmakeModules = (Get-ChildItem -Path cmake -Filter *.cmake | Measure-Object).Count
    Write-ColorOutput "   ✅ cmake/ modules: $cmakeModules file(s)" -ForegroundColor Green

    if (Test-Path "cmake/PrecompiledHeaders.cmake") {
        Write-ColorOutput "      ✅ PrecompiledHeaders.cmake present" -ForegroundColor Green
    }
} else {
    Write-ColorOutput "   ⚠️  cmake/ directory missing" -ForegroundColor Yellow
    $Script:INFO += "cmake/ directory for custom modules not created"
}

Write-Host ""

# ============================================================================
# 5. GIT STATUS
# ============================================================================

Write-Host "════════════════════════════════════════════════════════════════"
Write-ColorOutput "🌿 6. GIT STATUS" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════════════"
Write-Host ""

$Script:HEALTH_TOTAL++
try {
    $gitStatus = git rev-parse --git-dir 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-ColorOutput "   ✅ Git repository initialized" -ForegroundColor Green
        $Script:HEALTH_SCORE++

        # Branch
        $branch = git branch --show-current 2>&1
        Write-Host "      Current branch: $branch"

        # Remote
        $remote = git remote get-url origin 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "      Remote origin:  $remote"
        } else {
            Write-ColorOutput "      ⚠️  No remote 'origin' configured" -ForegroundColor Yellow
            $Script:WARNINGS += "No git remote 'origin' configured"
        }

        # Uncommitted changes
        $modified = (git status --short | Measure-Object).Count
        if ($modified -eq 0) {
            Write-ColorOutput "      ✅ Working tree clean" -ForegroundColor Green
        } else {
            Write-ColorOutput "      ⚠️  $modified uncommitted file(s)" -ForegroundColor Yellow
            $Script:INFO += "$modified uncommitted changes in working tree"
        }

        # Recent commits
        $commitCount = git rev-list --count HEAD 2>&1
        Write-Host "      Total commits:  $commitCount"

        $recent = git log -1 --format='%h %s' 2>&1
        Write-Host "      Latest commit:  $recent"

        # Check .gitignore
        if (Test-Path ".gitignore") {
            Write-ColorOutput "      ✅ .gitignore present" -ForegroundColor Green
        } else {
            Write-ColorOutput "      ❌ .gitignore missing" -ForegroundColor Red
            $Script:ISSUES += ".gitignore missing"
        }
    } else {
        Write-ColorOutput "   ❌ Not a git repository" -ForegroundColor Red
        $Script:ISSUES += "Not a git repository (run: git init)"
    }
} catch {
    Write-ColorOutput "   ❌ Not a git repository" -ForegroundColor Red
    $Script:ISSUES += "Not a git repository (run: git init)"
}

Write-Host ""

# ============================================================================
# 6. CI/CD STATUS
# ============================================================================

Write-Host "════════════════════════════════════════════════════════════════"
Write-ColorOutput "⚙️  7. CI/CD STATUS" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════════════"
Write-Host ""

# GitHub Actions workflows
$workflowsCount = 0
if (Test-Path ".github/workflows") {
    $workflowsCount = (Get-ChildItem -Path .github/workflows -Filter *.yml | Measure-Object).Count
}

$Script:HEALTH_TOTAL++
if ($workflowsCount -ge 3) {
    Write-ColorOutput "   ✅ GitHub Actions: $workflowsCount workflows" -ForegroundColor Green
    $Script:HEALTH_SCORE++

    Get-ChildItem -Path .github/workflows -Filter *.yml | ForEach-Object {
        Write-ColorOutput "      ✅ $($_.Name)" -ForegroundColor Green
    }
} else {
    Write-ColorOutput "   ❌ GitHub Actions: $workflowsCount workflows (expected 3)" -ForegroundColor Red
    $Script:ISSUES += "Expected 3 CI workflows (Windows, Linux, macOS), found $workflowsCount"
}

Write-Host ""

# GitHub CLI check
if (Get-Command gh -ErrorAction SilentlyContinue) {
    Write-ColorOutput "   ✅ GitHub CLI (gh) installed" -ForegroundColor Green

    $authStatus = gh auth status 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-ColorOutput "      ✅ Authenticated" -ForegroundColor Green

        # Get latest run
        try {
            $latestRun = gh run list --limit 1 --json status,conclusion,workflowName | ConvertFrom-Json

            if ($latestRun -and $latestRun.Count -gt 0) {
                $run = $latestRun[0]
                $status = $run.status
                $conclusion = if ($run.conclusion) { $run.conclusion } else { 'in_progress' }
                $workflow = $run.workflowName

                if ($conclusion -eq 'success') {
                    Write-ColorOutput "      ✅ Latest run: $workflow - SUCCESS" -ForegroundColor Green
                } elseif ($conclusion -eq 'failure') {
                    Write-ColorOutput "      ❌ Latest run: $workflow - FAILED" -ForegroundColor Red
                    $Script:WARNINGS += "Latest CI run failed: $workflow"
                } else {
                    Write-ColorOutput "      🔄 Latest run: $workflow - $status" -ForegroundColor Yellow
                }
            } else {
                Write-ColorOutput "      ⚠️  No workflow runs found" -ForegroundColor Yellow
            }
        } catch {
            Write-ColorOutput "      ⚠️  Could not fetch workflow runs" -ForegroundColor Yellow
        }
    } else {
        Write-ColorOutput "      ⚠️  Not authenticated (run: gh auth login)" -ForegroundColor Yellow
        $Script:INFO += "GitHub CLI not authenticated"
    }
} else {
    Write-ColorOutput "   ⚠️  GitHub CLI (gh) not installed" -ForegroundColor Yellow
    $Script:INFO += "GitHub CLI not installed (install: https://cli.github.com/)"
}

Write-Host ""

# ============================================================================
# 7. WORK SCRIPTS
# ============================================================================

Write-Host "════════════════════════════════════════════════════════════════"
Write-ColorOutput "🛠️  8. WORK SCRIPTS" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════════════"
Write-Host ""

# check-ci scripts
$Script:HEALTH_TOTAL++
if ((Test-Path "tools/check-ci.sh") -or (Test-Path "tools/check-ci.ps1")) {
    Write-ColorOutput "   ✅ tools/check-ci (CI monitoring)" -ForegroundColor Green
    $Script:HEALTH_SCORE++

    if (Test-Path "tools/check-ci.ps1") {
        Write-Host "      PowerShell version available"
    }
    if (Test-Path "tools/check-ci.sh") {
        Write-Host "      Bash version available"
    }
} else {
    Write-ColorOutput "   ❌ tools/check-ci missing" -ForegroundColor Red
    $Script:ISSUES += "tools/check-ci scripts missing"
}

# project-status (this script)
if ((Test-Path "tools/project-status.sh") -or (Test-Path "tools/project-status.ps1")) {
    Write-ColorOutput "   ✅ tools/project-status (this script)" -ForegroundColor Green
} else {
    Write-ColorOutput "   ⚠️  tools/project-status (not found - running from memory?)" -ForegroundColor Yellow
}

# Check tools/ directory
if (Test-Path "tools") {
    $toolsCount = (Get-ChildItem -Path tools -Include *.sh,*.ps1 -Recurse | Measure-Object).Count
    Write-ColorOutput "   ✅ tools/ directory: $toolsCount script(s)" -ForegroundColor Green
} else {
    Write-ColorOutput "   ❌ tools/ directory missing" -ForegroundColor Red
    $Script:ISSUES += "tools/ directory missing"
}

Write-Host ""

# ============================================================================
# HEALTH SCORE CALCULATION
# ============================================================================

Write-Host "════════════════════════════════════════════════════════════════"
Write-ColorOutput "📊 HEALTH SCORE SUMMARY" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════════════"
Write-Host ""

$healthPercent = if ($Script:HEALTH_TOTAL -gt 0) { [int](($Script:HEALTH_SCORE * 100) / $Script:HEALTH_TOTAL) } else { 0 }

Write-Host "   Health Score: $Script:HEALTH_SCORE / $Script:HEALTH_TOTAL checks passed"
Write-Host ""

if ($healthPercent -ge 90) {
    Write-ColorOutput "   ✅ EXCELLENT ($healthPercent%)" -ForegroundColor Green
    Write-Host "      Project is in excellent health!"
} elseif ($healthPercent -ge 75) {
    Write-ColorOutput "   ✅ GOOD ($healthPercent%)" -ForegroundColor Green
    Write-Host "      Project is healthy with minor issues."
} elseif ($healthPercent -ge 60) {
    Write-ColorOutput "   ⚠️  FAIR ($healthPercent%)" -ForegroundColor Yellow
    Write-Host "      Project needs attention in some areas."
} else {
    Write-ColorOutput "   ❌ POOR ($healthPercent%)" -ForegroundColor Red
    Write-Host "      Project requires significant improvements."
}

Write-Host ""

# ============================================================================
# ISSUES, WARNINGS, INFO
# ============================================================================

if ($Script:ISSUES.Count -gt 0) {
    Write-Host "════════════════════════════════════════════════════════════════"
    Write-ColorOutput "❌ CRITICAL ISSUES ($($Script:ISSUES.Count))" -ForegroundColor Red
    Write-Host "════════════════════════════════════════════════════════════════"
    Write-Host ""
    foreach ($issue in $Script:ISSUES) {
        Write-Host "   • $issue"
    }
    Write-Host ""
}

if ($Script:WARNINGS.Count -gt 0) {
    Write-Host "════════════════════════════════════════════════════════════════"
    Write-ColorOutput "⚠️  WARNINGS ($($Script:WARNINGS.Count))" -ForegroundColor Yellow
    Write-Host "════════════════════════════════════════════════════════════════"
    Write-Host ""
    foreach ($warning in $Script:WARNINGS) {
        Write-Host "   • $warning"
    }
    Write-Host ""
}

if ($Script:INFO.Count -gt 0) {
    Write-Host "════════════════════════════════════════════════════════════════"
    Write-ColorOutput "ℹ️  INFORMATIONAL ($($Script:INFO.Count))" -ForegroundColor Cyan
    Write-Host "════════════════════════════════════════════════════════════════"
    Write-Host ""
    foreach ($info in $Script:INFO) {
        Write-Host "   • $info"
    }
    Write-Host ""
}

# ============================================================================
# RECOMMENDATIONS
# ============================================================================

Write-Host "════════════════════════════════════════════════════════════════"
Write-ColorOutput "💡 RECOMMENDATIONS" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════════════"
Write-Host ""

if ($Script:ISSUES.Count -gt 0) {
    Write-Host "   🔴 Address critical issues immediately"
}

if ($healthPercent -lt 75) {
    Write-Host "   📚 Review .claude/QUALITY_CHECKLIST.md"
}

if (!(Test-Path "build") -or !(Test-Path "build/CMakeCache.txt")) {
    Write-Host "   🏗️  Configure build: cmake -B build"
    Write-Host "   🏗️  Build project: cmake --build build"
}

if (Get-Command gh -ErrorAction SilentlyContinue) {
    $authStatus = gh auth status 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "   ⚙️  Monitor CI/CD: .\tools\check-ci.ps1 status"
    }
}

Write-Host ""

# ============================================================================
# FOOTER
# ============================================================================

Write-Host "════════════════════════════════════════════════════════════════"
Write-Host ""
Write-Host "Health check completed at $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-Host ""
Write-Host "For detailed quality requirements, see:"
Write-Host "   .claude/QUALITY_CHECKLIST.md"
Write-Host ""
Write-Host "════════════════════════════════════════════════════════════════"
Write-Host ""

# Exit with status based on health score
if ($healthPercent -ge 75) {
    exit 0
} else {
    exit 1
}
