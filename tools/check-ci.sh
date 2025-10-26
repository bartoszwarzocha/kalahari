#!/usr/bin/env bash
# Kalahari CI/CD Monitor Script
# Monitors GitHub Actions workflow runs
# Requires: GitHub CLI (gh)

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Check if gh is installed
if ! command -v gh &> /dev/null; then
    echo -e "${RED}Error: GitHub CLI (gh) not installed${NC}"
    echo "Install: https://cli.github.com/"
    echo "  Ubuntu/Debian: sudo apt install gh"
    echo "  macOS: brew install gh"
    echo "  Windows: winget install GitHub.cli"
    exit 1
fi

# Check if authenticated
if ! gh auth status &> /dev/null; then
    echo -e "${RED}Error: Not authenticated with GitHub CLI${NC}"
    echo "Run: gh auth login"
    exit 1
fi

# Function to display usage
usage() {
    cat << EOF
${BLUE}Kalahari CI/CD Monitor${NC}

Usage: $0 [COMMAND] [OPTIONS]

Commands:
  status              Show latest workflow run status (default)
  watch               Watch workflow runs in real-time
  logs [RUN_ID]       Download logs for specific run
  list [N]            List last N runs (default: 10)
  summary [RUN_ID]    Show detailed summary of a run

Options:
  -h, --help          Show this help message

Examples:
  $0                  # Show latest status
  $0 watch            # Watch runs (auto-refresh)
  $0 list 20          # Show last 20 runs
  $0 logs 12345678    # Download logs for run 12345678
  $0 summary          # Show summary of latest run

EOF
}

# Function to get status emoji
status_emoji() {
    case "$1" in
        success|completed) echo "✅" ;;
        failure|failed) echo "❌" ;;
        in_progress|queued) echo "🔄" ;;
        cancelled) echo "⛔" ;;
        skipped) echo "⏭️" ;;
        *) echo "❓" ;;
    esac
}

# Function to get status color
status_color() {
    case "$1" in
        success|completed) echo "$GREEN" ;;
        failure|failed) echo "$RED" ;;
        in_progress|queued) echo "$YELLOW" ;;
        *) echo "$NC" ;;
    esac
}

# Function to show latest status
show_status() {
    echo -e "${BLUE}=== Latest CI/CD Status ===${NC}\n"

    # Get latest run
    local run=$(gh run list --limit 1 --json databaseId,displayTitle,status,conclusion,event,createdAt,headBranch,workflowName)

    if [[ -z "$run" ]]; then
        echo -e "${YELLOW}No workflow runs found${NC}"
        return
    fi

    # Parse run data
    local run_id=$(echo "$run" | jq -r '.[0].databaseId')
    local title=$(echo "$run" | jq -r '.[0].displayTitle')
    local status=$(echo "$run" | jq -r '.[0].status')
    local conclusion=$(echo "$run" | jq -r '.[0].conclusion // "in_progress"')
    local branch=$(echo "$run" | jq -r '.[0].headBranch')
    local workflow=$(echo "$run" | jq -r '.[0].workflowName')
    local created=$(echo "$run" | jq -r '.[0].createdAt')

    local emoji=$(status_emoji "$conclusion")
    local color=$(status_color "$conclusion")

    echo -e "${color}${emoji} Run #${run_id}${NC}"
    echo -e "  Title:    ${title}"
    echo -e "  Workflow: ${workflow}"
    echo -e "  Branch:   ${branch}"
    echo -e "  Status:   ${color}${status}${NC}"
    echo -e "  Result:   ${color}${conclusion}${NC}"
    echo -e "  Created:  ${created}"
    echo ""

    # Get job statuses
    echo -e "${BLUE}Job Details:${NC}"
    gh run view "$run_id" --json jobs --jq '.jobs[] | "\(.name): \(.conclusion // .status)"' | while read -r line; do
        local job_name=$(echo "$line" | cut -d':' -f1)
        local job_status=$(echo "$line" | cut -d':' -f2 | xargs)
        local job_emoji=$(status_emoji "$job_status")
        local job_color=$(status_color "$job_status")
        echo -e "  ${job_color}${job_emoji} ${job_name}: ${job_status}${NC}"
    done

    echo ""
    echo -e "${BLUE}View in browser:${NC} $(gh run view "$run_id" --web --json url -q .url 2>/dev/null || echo 'https://github.com/$(gh repo view --json nameWithOwner -q .nameWithOwner)/actions')"
}

# Function to watch runs
watch_runs() {
    echo -e "${BLUE}=== Watching CI/CD Runs (Ctrl+C to exit) ===${NC}\n"

    while true; do
        clear
        show_status
        echo -e "${YELLOW}Refreshing in 30 seconds...${NC}"
        sleep 30
    done
}

# Function to list runs
list_runs() {
    local limit=${1:-10}
    echo -e "${BLUE}=== Last ${limit} Workflow Runs ===${NC}\n"

    gh run list --limit "$limit" --json databaseId,displayTitle,status,conclusion,createdAt,headBranch | \
    jq -r '.[] | "\(.databaseId)|\(.displayTitle)|\(.conclusion // .status)|\(.headBranch)|\(.createdAt)"' | \
    while IFS='|' read -r id title status branch created; do
        local emoji=$(status_emoji "$status")
        local color=$(status_color "$status")
        printf "${color}${emoji} #%-10s %-50s %-15s %s${NC}\n" "$id" "${title:0:50}" "$status" "$branch"
    done
}

# Function to download logs
download_logs() {
    local run_id=${1:-}

    if [[ -z "$run_id" ]]; then
        # Get latest run ID
        run_id=$(gh run list --limit 1 --json databaseId -q '.[0].databaseId')
        echo -e "${YELLOW}No run ID specified, using latest: ${run_id}${NC}\n"
    fi

    local log_dir="logs/run-${run_id}"
    mkdir -p "$log_dir"

    echo -e "${BLUE}Downloading logs for run #${run_id}...${NC}"

    if gh run download "$run_id" --dir "$log_dir" 2>/dev/null; then
        echo -e "${GREEN}✅ Logs downloaded to: ${log_dir}${NC}"
        echo ""
        echo "Contents:"
        find "$log_dir" -type f -name "*.txt" | sort | while read -r file; do
            echo "  - $(basename "$file")"
        done
    else
        echo -e "${RED}❌ Failed to download logs${NC}"
        echo "Run may still be in progress or logs not available yet"
    fi
}

# Function to show detailed summary
show_summary() {
    local run_id=${1:-}

    if [[ -z "$run_id" ]]; then
        run_id=$(gh run list --limit 1 --json databaseId -q '.[0].databaseId')
    fi

    echo -e "${BLUE}=== Detailed Summary for Run #${run_id} ===${NC}\n"

    gh run view "$run_id"
}

# Main script
main() {
    local command=${1:-status}

    case "$command" in
        -h|--help)
            usage
            ;;
        status)
            show_status
            ;;
        watch)
            watch_runs
            ;;
        list)
            list_runs "${2:-10}"
            ;;
        logs)
            download_logs "${2:-}"
            ;;
        summary)
            show_summary "${2:-}"
            ;;
        *)
            echo -e "${RED}Unknown command: $command${NC}\n"
            usage
            exit 1
            ;;
    esac
}

main "$@"
