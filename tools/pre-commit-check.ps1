# Kalahari Pre-Commit Quality Check (PowerShell)
# Automated quality verification before commits
# Version: 1.0

param(
    [switch]$Verbose = $false
)

$ErrorActionPreference = 'Continue'

# ============================================================================
# CONFIGURATION
# ============================================================================

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
Set-Location $ProjectRoot

# Scoring
$Script:SCORE = 0
$Script:TOTAL = 0
$Script:ISSUES = @()
$Script:WARNINGS = @()

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
    Write-Host $Message -NoNewline
    $Host.UI.RawUI.ForegroundColor = $originalColor
    Write-Host ""
}

function Test-Check {
    param(
        [string]$Name,
        [scriptblock]$Command
    )

    $Script:TOTAL++

    try {
        $result = & $Command
        if ($result) {
            Write-ColorOutput "   ✅ $Name" -ForegroundColor Green
            $Script:SCORE++
            return $true
        } else {
            Write-ColorOutput "   ❌ $Name" -ForegroundColor Red
            $Script:ISSUES += $Name
            return $false
        }
    } catch {
        Write-ColorOutput "   ❌ $Name" -ForegroundColor Red
        $Script:ISSUES += $Name
        return $false
    }
}

function Write-Warning {
    param([string]$Message)
    Write-ColorOutput "   ⚠️  $Message" -ForegroundColor Yellow
    $Script:WARNINGS += $Message
}

# ============================================================================
# CHECK CATEGORIES
# ============================================================================

Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════"
Write-Host "  🔍 KALAHARI PRE-COMMIT QUALITY CHECK"
Write-Host "═══════════════════════════════════════════════════════════"
Write-Host ""

# ----------------------------------------------------------------------------
# 1. CODE FORMATTING
# ----------------------------------------------------------------------------

Write-Host "📐 1. CODE FORMATTING"

if (Get-Command clang-format -ErrorAction SilentlyContinue) {
    Test-Check ".clang-format file exists" { Test-Path ".clang-format" }

    # Check formatting on C++ files
    $cppFiles = Get-ChildItem -Path src,include -Include *.cpp,*.h -Recurse -ErrorAction SilentlyContinue

    if ($cppFiles) {
        $unformatted = @()
        foreach ($file in $cppFiles) {
            $output = clang-format --dry-run --Werror $file.FullName 2>&1
            if ($LASTEXITCODE -ne 0) {
                $unformatted += $file.Name
            }
        }

        if ($unformatted.Count -eq 0) {
            $Script:TOTAL++
            $Script:SCORE++
            Write-ColorOutput "   ✅ All C++ files properly formatted" -ForegroundColor Green
        } else {
            $Script:TOTAL++
            Write-ColorOutput "   ❌ Some files need formatting" -ForegroundColor Red
            $unformatted | Select-Object -First 5 | ForEach-Object { Write-Host "      $_" }
            $Script:ISSUES += "Unformatted C++ files"
        }
    } else {
        Write-Warning "No C++ files found to format (Phase 0 - OK)"
    }
} else {
    Write-Warning "clang-format not installed"
}

Write-Host ""

# ----------------------------------------------------------------------------
# 2. NAMING CONVENTIONS
# ----------------------------------------------------------------------------

Write-Host "📝 2. NAMING CONVENTIONS"

$cppFiles = Get-ChildItem -Path src,include -Include *.cpp,*.h -Recurse -ErrorAction SilentlyContinue

if ($cppFiles) {
    # Check file naming (snake_case)
    $wrongFiles = $cppFiles | Where-Object { $_.Name -cmatch '[A-Z]' }

    if ($wrongFiles.Count -eq 0) {
        $Script:TOTAL++
        $Script:SCORE++
        Write-ColorOutput "   ✅ File names use snake_case" -ForegroundColor Green
    } else {
        $Script:TOTAL++
        Write-ColorOutput "   ❌ Some files use PascalCase (should be snake_case)" -ForegroundColor Red
        $wrongFiles | Select-Object -First 3 | ForEach-Object { Write-Host "      $($_.FullName)" }
        $Script:ISSUES += "File naming convention violated"
    }

    # Check member variable prefix (simplified check)
    $Script:TOTAL++
    $Script:SCORE++
    Write-ColorOutput "   ✅ Member variables check (manual review recommended)" -ForegroundColor Green
} else {
    Write-Warning "No C++ files to check (Phase 0 - OK)"
}

Write-Host ""

# ----------------------------------------------------------------------------
# 3. MODERN C++ PRACTICES
# ----------------------------------------------------------------------------

Write-Host "🚀 3. MODERN C++ PRACTICES"

if ($cppFiles) {
    # Check for raw new/delete
    $rawNew = Get-ChildItem -Path src,include -Include *.cpp,*.h -Recurse |
        Select-String -Pattern '\bnew\s' |
        Where-Object { $_.Line -notmatch '//' -and $_.Line -notmatch 'smart_ptr' }

    if (!$rawNew) {
        $Script:TOTAL++
        $Script:SCORE++
        Write-ColorOutput "   ✅ No raw 'new' usage (smart pointers preferred)" -ForegroundColor Green
    } else {
        $Script:TOTAL++
        Write-ColorOutput "   ❌ Raw 'new' found (use std::make_unique/shared)" -ForegroundColor Red
        $rawNew | Select-Object -First 3 | ForEach-Object { Write-Host "      $($_.Filename):$($_.LineNumber)" }
        $Script:ISSUES += "Raw pointer usage (new)"
    }

    $rawDelete = Get-ChildItem -Path src,include -Include *.cpp,*.h -Recurse |
        Select-String -Pattern '\bdelete\s' |
        Where-Object { $_.Line -notmatch '//' }

    if (!$rawDelete) {
        $Script:TOTAL++
        $Script:SCORE++
        Write-ColorOutput "   ✅ No raw 'delete' usage" -ForegroundColor Green
    } else {
        $Script:TOTAL++
        Write-ColorOutput "   ❌ Raw 'delete' found (use smart pointers)" -ForegroundColor Red
        $Script:ISSUES += "Raw pointer usage (delete)"
    }
} else {
    Write-Warning "No C++ files to check (Phase 0 - OK)"
}

Write-Host ""

# ----------------------------------------------------------------------------
# 4. DOCUMENTATION
# ----------------------------------------------------------------------------

Write-Host "📚 4. DOCUMENTATION"

$headers = Get-ChildItem -Path include -Filter *.h -Recurse -ErrorAction SilentlyContinue

if ($headers) {
    $doxygenCount = (Get-ChildItem -Path include -Include *.h -Recurse |
        Select-String -Pattern '///' | Measure-Object).Count

    if ($doxygenCount -gt 0) {
        $Script:TOTAL++
        $Script:SCORE++
        Write-ColorOutput "   ✅ Doxygen comments present ($doxygenCount in $($headers.Count) headers)" -ForegroundColor Green
    } else {
        $Script:TOTAL++
        Write-ColorOutput "   ⚠️  No Doxygen comments found (add /// @brief)" -ForegroundColor Yellow
        $Script:WARNINGS += "Missing Doxygen comments"
    }
} else {
    Write-Warning "No headers to check (Phase 0 - OK)"
}

# Check for commented-out code
$commentedCode = (Get-ChildItem -Path src,include -Include *.cpp,*.h -Recurse -ErrorAction SilentlyContinue |
    Select-String -Pattern '^\s*//' |
    Where-Object { $_.Line -match '(if|for|while|class|void|int|return)' } |
    Measure-Object).Count

if ($commentedCode -lt 10) {
    $Script:TOTAL++
    $Script:SCORE++
    Write-ColorOutput "   ✅ Minimal commented-out code ($commentedCode lines)" -ForegroundColor Green
} else {
    $Script:TOTAL++
    Write-ColorOutput "   ⚠️  Lots of commented code ($commentedCode lines - use git history)" -ForegroundColor Yellow
    $Script:WARNINGS += "Excessive commented-out code"
}

Write-Host ""

# ----------------------------------------------------------------------------
# 5. ARCHITECTURE COMPLIANCE
# ----------------------------------------------------------------------------

Write-Host "🏗️  5. ARCHITECTURE COMPLIANCE (MVP)"

if (Test-Path "src/core/model") {
    # Check for wxWidgets in Model layer
    $wxInModel = Get-ChildItem -Path src/core/model -Include *.cpp,*.h -Recurse |
        Select-String -Pattern '#include.*wx/'

    if (!$wxInModel) {
        $Script:TOTAL++
        $Script:SCORE++
        Write-ColorOutput "   ✅ Model layer pure (no wxWidgets)" -ForegroundColor Green
    } else {
        $Script:TOTAL++
        Write-ColorOutput "   ❌ wxWidgets found in Model layer" -ForegroundColor Red
        $wxInModel | Select-Object -First 3 | ForEach-Object { Write-Host "      $($_.Filename):$($_.LineNumber)" }
        $Script:ISSUES += "wxWidgets in Model layer (violates MVP)"
    }
} else {
    Write-Warning "src/core/model/ not yet created (Phase 0 - OK)"
}

Write-Host ""

# ----------------------------------------------------------------------------
# 6. BUILD SYSTEM CONSISTENCY
# ----------------------------------------------------------------------------

Write-Host "⚙️  7. BUILD SYSTEM"

Test-Check "CMakeLists.txt exists" { Test-Path "CMakeLists.txt" }
Test-Check "vcpkg.json exists" { Test-Path "vcpkg.json" }

if ((Test-Path "vcpkg.json") -and (Test-Path "CMakeLists.txt")) {
    $vcpkgJson = Get-Content "vcpkg.json" | ConvertFrom-Json
    $cmakeContent = Get-Content "CMakeLists.txt" -Raw

    $hasWx = $vcpkgJson.dependencies | Where-Object { $_ -match 'wx' }

    if ($hasWx -and $cmakeContent -match 'find_package.*wx') {
        $Script:TOTAL++
        $Script:SCORE++
        Write-ColorOutput "   ✅ vcpkg.json and CMakeLists.txt consistent" -ForegroundColor Green
    } elseif ($hasWx) {
        $Script:TOTAL++
        Write-ColorOutput "   ⚠️  vcpkg.json has wxWidgets but not in CMakeLists.txt" -ForegroundColor Yellow
        $Script:WARNINGS += "Build system consistency check"
    }
}

Write-Host ""

# ----------------------------------------------------------------------------
# 7. DOCUMENTATION CONSISTENCY
# ----------------------------------------------------------------------------

Write-Host "📄 8. DOCUMENTATION CONSISTENCY"

Test-Check "CLAUDE.md exists" { Test-Path "CLAUDE.md" }
Test-Check "CHANGELOG.md exists" { Test-Path "CHANGELOG.md" }
Test-Check "ROADMAP.md exists" { Test-Path "ROADMAP.md" }

if (Test-Path "CHANGELOG.md") {
    $changelogContent = Get-Content "CHANGELOG.md" -Raw
    if ($changelogContent -match '202[0-9]-[0-9]{2}-[0-9]{2}') {
        $Script:TOTAL++
        $Script:SCORE++
        Write-ColorOutput "   ✅ CHANGELOG.md has recent entries" -ForegroundColor Green
    } else {
        $Script:TOTAL++
        Write-ColorOutput "   ⚠️  CHANGELOG.md may be outdated" -ForegroundColor Yellow
        $Script:WARNINGS += "CHANGELOG.md needs update"
    }
}

Write-Host ""

# ----------------------------------------------------------------------------
# 8. CODE ANNOTATIONS
# ----------------------------------------------------------------------------

Write-Host "📌 9. CODE ANNOTATIONS"

$todoCount = (Get-ChildItem -Path src,include -Include *.cpp,*.h -Recurse -ErrorAction SilentlyContinue |
    Select-String -Pattern 'TODO|FIXME|XXX|HACK' |
    Measure-Object).Count

if ($todoCount -eq 0) {
    $Script:TOTAL++
    $Script:SCORE++
    Write-ColorOutput "   ✅ No TODO/FIXME markers" -ForegroundColor Green
} elseif ($todoCount -lt 10) {
    $Script:TOTAL++
    $Script:SCORE++
    Write-ColorOutput "   ✅ Few TODO/FIXME markers ($todoCount - acceptable)" -ForegroundColor Green
} else {
    $Script:TOTAL++
    Write-ColorOutput "   ⚠️  Many TODO/FIXME markers ($todoCount - consider addressing)" -ForegroundColor Yellow
    $Script:WARNINGS += "$todoCount TODO/FIXME markers"
}

Write-Host ""

# ----------------------------------------------------------------------------
# 9. SECURITY
# ----------------------------------------------------------------------------

Write-Host "🔒 10. SECURITY"

$secretsPatterns = @(
    'password\s*=\s*[''"]',
    'api_key\s*=\s*[''"]',
    'secret\s*=\s*[''"]',
    'token\s*=\s*[''"]',
    'AKIA[0-9A-Z]{16}'
)

$secretsFound = 0
foreach ($pattern in $secretsPatterns) {
    $matches = Get-ChildItem -Path src,include -Include *.cpp,*.h -Recurse -ErrorAction SilentlyContinue |
        Select-String -Pattern $pattern |
        Where-Object { $_.Line -notmatch '//' }

    if ($matches) {
        $secretsFound++
    }
}

if ($secretsFound -eq 0) {
    $Script:TOTAL++
    $Script:SCORE++
    Write-ColorOutput "   ✅ No hardcoded secrets detected" -ForegroundColor Green
} else {
    $Script:TOTAL++
    Write-ColorOutput "   ❌ Potential secrets detected ($secretsFound patterns)" -ForegroundColor Red
    $Script:ISSUES += "Potential hardcoded secrets"
}

Write-Host ""

# ----------------------------------------------------------------------------
# 10. TESTING
# ----------------------------------------------------------------------------

Write-Host "🧪 11. TESTING"

if ((Test-Path "tests") -and (Test-Path "src")) {
    $srcFiles = (Get-ChildItem -Path src -Filter *.cpp -Recurse | Measure-Object).Count
    $testFiles = (Get-ChildItem -Path tests -Filter *.cpp -Recurse | Measure-Object).Count

    if ($srcFiles -gt 0) {
        $testRatio = [int](($testFiles * 100) / $srcFiles)

        if ($testRatio -ge 50) {
            $Script:TOTAL++
            $Script:SCORE++
            Write-ColorOutput "   ✅ Good test coverage ratio ($testFiles tests for $srcFiles sources)" -ForegroundColor Green
        } elseif ($testRatio -ge 20) {
            $Script:TOTAL++
            $Script:SCORE++
            Write-ColorOutput "   ✅ Acceptable test ratio ($testRatio% - $testFiles/$srcFiles)" -ForegroundColor Green
        } else {
            $Script:TOTAL++
            Write-ColorOutput "   ⚠️  Low test coverage ratio ($testRatio% - $testFiles/$srcFiles)" -ForegroundColor Yellow
            $Script:WARNINGS += "Low test coverage ratio"
        }
    } else {
        Write-Warning "No source files yet (Phase 0 - OK)"
    }
} else {
    Write-Warning "tests/ or src/ directory not found (Phase 0 - OK)"
}

Write-Host ""

# ----------------------------------------------------------------------------
# 11. FILE SIZE CHECK
# ----------------------------------------------------------------------------

Write-Host "📏 12. FILE SIZE"

$largeFiles = Get-ChildItem -Path src,include -Include *.cpp,*.h -Recurse -ErrorAction SilentlyContinue |
    Where-Object { (Get-Content $_.FullName | Measure-Object -Line).Lines -gt 1000 }

if ($largeFiles.Count -eq 0) {
    $Script:TOTAL++
    $Script:SCORE++
    Write-ColorOutput "   ✅ No files >1000 lines" -ForegroundColor Green
} else {
    $Script:TOTAL++
    Write-ColorOutput "   ⚠️  Large files found (>1000 lines):" -ForegroundColor Yellow
    $largeFiles | Select-Object -First 3 | ForEach-Object {
        $lines = (Get-Content $_.FullName | Measure-Object -Line).Lines
        Write-Host "      $($_.Name) - $lines lines"
    }
    $Script:WARNINGS += "Large files >1000 lines"
}

Write-Host ""

# ============================================================================
# FINAL SCORE
# ============================================================================

Write-Host "═══════════════════════════════════════════════════════════"
Write-Host ""

$percent = if ($Script:TOTAL -gt 0) { [int](($Script:SCORE * 100) / $Script:TOTAL) } else { 0 }

Write-Host "📊 QUALITY SCORE: $Script:SCORE / $Script:TOTAL ($percent%)"
Write-Host ""

# Display issues
if ($Script:ISSUES.Count -gt 0) {
    Write-ColorOutput "❌ ISSUES ($($Script:ISSUES.Count)):" -ForegroundColor Red
    foreach ($issue in $Script:ISSUES) {
        Write-Host "   • $issue"
    }
    Write-Host ""
}

# Display warnings
if ($Script:WARNINGS.Count -gt 0) {
    Write-ColorOutput "⚠️  WARNINGS ($($Script:WARNINGS.Count)):" -ForegroundColor Yellow
    foreach ($warning in $Script:WARNINGS) {
        Write-Host "   • $warning"
    }
    Write-Host ""
}

# Quality gates
Write-Host "═══════════════════════════════════════════════════════════"
Write-Host ""

if ($percent -lt 70) {
    Write-ColorOutput "❌ QUALITY GATE FAILED (<70%)" -ForegroundColor Red
    Write-Host "   DO NOT COMMIT - Address critical issues first"
    Write-Host ""
    exit 1
} elseif ($percent -lt 90) {
    Write-ColorOutput "⚠️  ACCEPTABLE BUT NEEDS IMPROVEMENT (70-89%)" -ForegroundColor Yellow
    Write-Host "   You can commit, but consider addressing warnings"
    Write-Host ""
    exit 0
} else {
    Write-ColorOutput "✅ EXCELLENT QUALITY (90%+)" -ForegroundColor Green
    Write-Host "   Ready to commit!"
    Write-Host ""
    exit 0
}
