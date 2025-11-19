# Kalahari CI/CD Monitor Script (PowerShell)
# Monitors GitHub Actions workflow runs
# Requires: GitHub CLI (gh)

param(
    [Parameter(Position=0)]
    [ValidateSet('status', 'watch', 'logs', 'list', 'summary', 'help')]
    [string]$Command = 'status',

    [Parameter(Position=1)]
    [string]$Argument
)

# ============================================================================
# CONFIGURATION
# ============================================================================

$ErrorActionPreference = 'Stop'

# Colors (PowerShell Console Colors)
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

# ============================================================================
# HELPER FUNCTIONS
# ============================================================================

function Test-GitHubCLI {
    if (!(Get-Command gh -ErrorAction SilentlyContinue)) {
        Write-ColorOutput "Error: GitHub CLI (gh) not installed" -ForegroundColor Red
        Write-Host "Install: https://cli.github.com/"
        Write-Host "  Windows: winget install GitHub.cli"
        Write-Host "  Or download from: https://github.com/cli/cli/releases"
        exit 1
    }

    # Check authentication
    $authStatus = gh auth status 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-ColorOutput "Error: Not authenticated with GitHub CLI" -ForegroundColor Red
        Write-Host "Run: gh auth login"
        exit 1
    }
}

function Get-StatusEmoji {
    param([string]$Status)

    switch -Regex ($Status) {
        '^(success|completed)$' { return '✅' }
        '^(failure|failed)$' { return '❌' }
        '^(in_progress|queued)$' { return '🔄' }
        '^cancelled$' { return '⛔' }
        '^skipped$' { return '⏭️' }
        default { return '❓' }
    }
}

function Get-StatusColor {
    param([string]$Status)

    switch -Regex ($Status) {
        '^(success|completed)$' { return [ConsoleColor]::Green }
        '^(failure|failed)$' { return [ConsoleColor]::Red }
        '^(in_progress|queued)$' { return [ConsoleColor]::Yellow }
        default { return [ConsoleColor]::White }
    }
}

function Show-Usage {
    Write-ColorOutput "Kalahari CI/CD Monitor" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Usage: .\check-ci.ps1 [COMMAND] [OPTIONS]"
    Write-Host ""
    Write-Host "Commands:"
    Write-Host "  status              Show latest workflow run status (default)"
    Write-Host "  watch               Watch workflow runs in real-time"
    Write-Host "  logs [RUN_ID]       Download logs for specific run"
    Write-Host "  list [N]            List last N runs (default: 10)"
    Write-Host "  summary [RUN_ID]    Show detailed summary of a run"
    Write-Host "  help                Show this help message"
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\check-ci.ps1                  # Show latest status"
    Write-Host "  .\check-ci.ps1 watch            # Watch runs (auto-refresh)"
    Write-Host "  .\check-ci.ps1 list 20          # Show last 20 runs"
    Write-Host "  .\check-ci.ps1 logs 12345678    # Download logs for run 12345678"
    Write-Host "  .\check-ci.ps1 summary          # Show summary of latest run"
    Write-Host ""
}

function Show-Status {
    Write-ColorOutput "=== Latest CI/CD Status ===" -ForegroundColor Cyan
    Write-Host ""

    # Get latest run
    $runJson = gh run list --limit 1 --json databaseId,displayTitle,status,conclusion,event,createdAt,headBranch,workflowName | ConvertFrom-Json

    if (!$runJson -or $runJson.Count -eq 0) {
        Write-ColorOutput "No workflow runs found" -ForegroundColor Yellow
        return
    }

    $run = $runJson[0]
    $runId = $run.databaseId
    $title = $run.displayTitle
    $status = $run.status
    $conclusion = if ($run.conclusion) { $run.conclusion } else { 'in_progress' }
    $branch = $run.headBranch
    $workflow = $run.workflowName
    $created = $run.createdAt

    $emoji = Get-StatusEmoji -Status $conclusion
    $color = Get-StatusColor -Status $conclusion

    Write-ColorOutput "$emoji Run #$runId" -ForegroundColor $color
    Write-Host "  Title:    $title"
    Write-Host "  Workflow: $workflow"
    Write-Host "  Branch:   $branch"
    Write-ColorOutput "  Status:   $status" -ForegroundColor $color
    Write-ColorOutput "  Result:   $conclusion" -ForegroundColor $color
    Write-Host "  Created:  $created"
    Write-Host ""

    # Get job statuses
    Write-ColorOutput "Job Details:" -ForegroundColor Cyan
    $jobs = gh run view $runId --json jobs | ConvertFrom-Json

    foreach ($job in $jobs.jobs) {
        $jobName = $job.name
        $jobStatus = if ($job.conclusion) { $job.conclusion } else { $job.status }
        $jobEmoji = Get-StatusEmoji -Status $jobStatus
        $jobColor = Get-StatusColor -Status $jobStatus

        Write-ColorOutput "  $jobEmoji $jobName`: $jobStatus" -ForegroundColor $jobColor
    }

    Write-Host ""
    $repoUrl = gh repo view --json nameWithOwner | ConvertFrom-Json
    $actionsUrl = "https://github.com/$($repoUrl.nameWithOwner)/actions"
    Write-ColorOutput "View in browser:" -ForegroundColor Cyan
    Write-Host "  $actionsUrl"
}

function Watch-Runs {
    Write-ColorOutput "=== Watching CI/CD Runs (Ctrl+C to exit) ===" -ForegroundColor Cyan
    Write-Host ""

    while ($true) {
        Clear-Host
        Show-Status
        Write-ColorOutput "Refreshing in 30 seconds..." -ForegroundColor Yellow
        Start-Sleep -Seconds 30
    }
}

function Show-RunList {
    param([int]$Limit = 10)

    Write-ColorOutput "=== Last $Limit Workflow Runs ===" -ForegroundColor Cyan
    Write-Host ""

    $runs = gh run list --limit $Limit --json databaseId,displayTitle,status,conclusion,createdAt,headBranch | ConvertFrom-Json

    foreach ($run in $runs) {
        $status = if ($run.conclusion) { $run.conclusion } else { $run.status }
        $emoji = Get-StatusEmoji -Status $status
        $color = Get-StatusColor -Status $status

        $titleTrunc = $run.displayTitle.Substring(0, [Math]::Min(50, $run.displayTitle.Length))

        Write-ColorOutput ("{0} #{1,-10} {2,-50} {3,-15} {4}" -f $emoji, $run.databaseId, $titleTrunc, $status, $run.headBranch) -ForegroundColor $color
    }
}

function Get-Logs {
    param([string]$RunId)

    if (!$RunId) {
        # Get latest run ID
        $runJson = gh run list --limit 1 --json databaseId | ConvertFrom-Json
        $RunId = $runJson[0].databaseId
        Write-ColorOutput "No run ID specified, using latest: $RunId" -ForegroundColor Yellow
        Write-Host ""
    }

    $logDir = "logs\run-$RunId"
    New-Item -ItemType Directory -Force -Path $logDir | Out-Null

    Write-ColorOutput "Downloading logs for run #$RunId..." -ForegroundColor Cyan

    try {
        gh run download $RunId --dir $logDir 2>&1 | Out-Null

        if ($LASTEXITCODE -eq 0) {
            Write-ColorOutput "✅ Logs downloaded to: $logDir" -ForegroundColor Green
            Write-Host ""
            Write-Host "Contents:"
            Get-ChildItem -Path $logDir -Filter *.txt -Recurse | ForEach-Object {
                Write-Host "  - $($_.Name)"
            }
        } else {
            throw "Download failed"
        }
    } catch {
        Write-ColorOutput "❌ Failed to download logs" -ForegroundColor Red
        Write-Host "Run may still be in progress or logs not available yet"
    }
}

function Show-Summary {
    param([string]$RunId)

    if (!$RunId) {
        $runJson = gh run list --limit 1 --json databaseId | ConvertFrom-Json
        $RunId = $runJson[0].databaseId
    }

    Write-ColorOutput "=== Detailed Summary for Run #$RunId ===" -ForegroundColor Cyan
    Write-Host ""

    gh run view $RunId
}

# ============================================================================
# MAIN SCRIPT
# ============================================================================

Test-GitHubCLI

switch ($Command) {
    'help' {
        Show-Usage
    }
    'status' {
        Show-Status
    }
    'watch' {
        Watch-Runs
    }
    'list' {
        $limit = if ($Argument) { [int]$Argument } else { 10 }
        Show-RunList -Limit $limit
    }
    'logs' {
        Get-Logs -RunId $Argument
    }
    'summary' {
        Show-Summary -RunId $Argument
    }
    default {
        Write-ColorOutput "Unknown command: $Command" -ForegroundColor Red
        Write-Host ""
        Show-Usage
        exit 1
    }
}
