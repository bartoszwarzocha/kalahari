---
description: Safely push changes to git remote with quality gates
---

**Safe git push with mandatory quality verification**

**NEVER push without verification - this command enforces standards**

## Execution Steps:

### 1. **Pre-push Verification (MANDATORY)**

Check all quality gates before push:

```bash
# Gate 1: No uncommitted changes
git status --porcelain

# Gate 2: Run automated quality checks
./tools/pre-commit-check.sh

# Gate 3: Verify CHANGELOG.md updated
git diff HEAD..origin/$(git branch --show-current) CHANGELOG.md

# Gate 4: Check CI/CD status
./tools/check-ci.sh status
```

**Blocking conditions (STOP if found):**
- ❌ Uncommitted changes exist
- ❌ pre-commit-check.sh score < 100%
- ❌ CHANGELOG.md not updated for new commits
- ❌ Last CI/CD run failed
- ❌ Not on allowed branch (main, develop, feature/*)

### 2. **Branch Analysis**

Determine push safety based on branch:

- **main/master**: 🔴 CRITICAL - require explicit confirmation
  - Check: No force push
  - Check: All commits already in CI/CD
  - Ask user: "Pushing to MAIN - are you sure? (yes/no)"

- **develop**: 🟡 HIGH - verify tests pass
  - Check: CI/CD green
  - Check: No WIP commits

- **feature/***: 🟢 NORMAL - standard checks
  - Check: Branch exists on remote (or -u flag needed)

### 3. **Commit Analysis**

Verify commit quality:

```bash
# Check last commit message format
git log -1 --pretty=%B

# Verify no "WIP", "TODO", "FIXME" in commit messages
git log origin/$(git branch --show-current)..HEAD --pretty=%B | grep -E "WIP|TODO|FIXME"
```

**Warning conditions (warn but allow):**
- ⚠️ Commit message < 10 characters
- ⚠️ No reference to task file (tasks/NNNNN)
- ⚠️ Multiple unrelated changes in single commit

### 4. **Remote Sync Check**

```bash
# Fetch remote state
git fetch origin

# Check if behind remote
git rev-list HEAD..origin/$(git branch --show-current) --count

# Check if diverged
git rev-list --left-right --count HEAD...origin/$(git branch --show-current)
```

**Actions based on state:**
- Behind remote: Suggest `git pull --rebase` first
- Diverged: BLOCK - manual merge required
- Ahead only: ✅ Safe to push

### 5. **Execute Push**

If all gates pass:

```bash
# Standard push
git push origin $(git branch --show-current)

# First-time branch push (if needed)
git push -u origin $(git branch --show-current)
```

**Post-push actions:**
1. Display pushed commit summary
2. Display CI/CD monitoring link
3. Remind about session save (if not done)

### 6. **Output Report**

```
✅ Push Quality Gates: PASSED

📊 Summary:
- Branch: feature/settings-dialog
- Commits pushed: 3
- Files changed: 12 (+145, -23)
- Quality score: 100%
- CI/CD: Triggered (monitoring link below)

🔗 Monitor CI/CD:
https://github.com/user/repo/actions

⚠️ Reminder: Save session when done (/save-session)
```

## Error Handling:

**If quality gate fails:**
```
❌ Push BLOCKED - Quality gate failed

Issue: pre-commit-check.sh score: 87% (required: 100%)

Failed checks:
- Trailing whitespace found in 3 files
- Missing header guards in 2 files

Action required:
1. Run: ./tools/pre-commit-check.sh
2. Fix reported issues
3. Commit fixes
4. Retry push
```

**If branch protection:**
```
❌ Push BLOCKED - Main branch protection

Direct push to 'main' not allowed.

Required workflow:
1. Create feature branch
2. Push to feature branch
3. Create Pull Request
4. Merge after review

Or if emergency: User must explicitly override
```

## Configuration:

**Protected branches:** main, master, production
**Quality threshold:** 100% (pre-commit-check.sh)
**Required files check:** CHANGELOG.md must be updated
**CI/CD requirement:** Last run must be green
