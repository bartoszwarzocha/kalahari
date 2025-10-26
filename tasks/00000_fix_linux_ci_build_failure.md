# Task #00000: Fix Linux CI Build Failure

## Context
- **Phase:** Phase 0 Week 1 (hotfix for Week 2 blocker)
- **Roadmap Reference:** ROADMAP.md Phase 0 - Foundation infrastructure must work on all platforms
- **Related Docs:**
  - [.github/workflows/ci-linux.yml](../.github/workflows/ci-linux.yml)
  - [.github/workflows/ci-macos.yml](../.github/workflows/ci-macos.yml)
  - [.github/workflows/ci-windows.yml](../.github/workflows/ci-windows.yml)
- **Dependencies:** None (blocker for all Week 2+ work)

## Objective
Fix Linux CI build failure that prevents Phase 0 Week 2 progress. CI must pass on all platforms (Windows, macOS, Linux) before starting Plugin Manager implementation.

**Success Criteria:**
- Linux Debug build passes on GitHub Actions
- Linux Release build passes on GitHub Actions
- All three platforms (Windows, macOS, Linux) show green checkmarks
- No changes break existing macOS/Windows CI

## Root Cause Analysis

### Identified Problem
**vcpkg is NOT a git submodule in the repository!**

### Evidence
1. **Local investigation:**
   ```bash
   $ file vcpkg
   vcpkg: ELF 64-bit LSB pie executable  # It's a binary file!

   $ cat .gitmodules
   cat: .gitmodules: No such file or directory  # No submodules!

   $ git submodule status
   (empty)  # No vcpkg submodule
   ```

2. **CI workflow expects vcpkg directory:**
   ```yaml
   # .github/workflows/ci-linux.yml:59-61
   - name: Bootstrap vcpkg
     run: |
       cd vcpkg                    # ← This fails!
       ./bootstrap-vcpkg.sh
   ```

3. **GitHub Actions CI status** (commit a7324cc):
   - macOS Debug: ✅ success
   - macOS Release: ✅ success
   - Linux Debug: ❌ **FAILURE**
   - Linux Release: ⚪ cancelled (dependency failed)
   - Windows Debug: ⚪ cancelled (dependency failed)
   - Windows Release: ?

4. **Error message from CI:**
   - "Configure CMake" step failed with exit code 1
   - Likely: `cd vcpkg` failed because directory doesn't exist

### Why This Happened
- vcpkg binary was committed to `.gitignore` (line 34: `/vcpkg`)
- vcpkg was never added as git submodule
- Local development works because vcpkg binary exists locally
- CI fails because fresh checkout has no vcpkg directory

### Why macOS CI Passes
**Hypothesis:** macOS workflow might handle vcpkg differently, or cache restored it

**Action:** Verify all three workflows use same vcpkg approach

## Proposed Approach

### Solution: Add vcpkg as Git Submodule

**Steps:**
1. Remove vcpkg binary from `.gitignore` (keep `/vcpkg_installed/`)
2. Add vcpkg as git submodule:
   ```bash
   git submodule add https://github.com/microsoft/vcpkg.git vcpkg
   git submodule update --init --recursive
   ```
3. Pin to specific commit (current: `271a5b8850aa50f9a40269cbf3cf414b36e333d6`)
4. Verify .gitmodules created
5. Update .gitignore to allow submodule but ignore vcpkg artifacts

### Why This Approach
- ✅ **Standard practice:** vcpkg documentation recommends git submodule
- ✅ **Reproducible builds:** Everyone gets exact same vcpkg version
- ✅ **CI friendly:** `actions/checkout@v4` with `submodules: recursive` handles it
- ✅ **Already in workflows:** Line 25 has `submodules: recursive`!

### Alternatives Considered
- **Alternative A: Install vcpkg in CI manually**
  - ❌ Not recommended by vcpkg team
  - ❌ More complex CI scripts
  - ❌ Version consistency issues

- **Alternative B: Use vcpkg's bootstrap directly**
  - ❌ Requires downloading vcpkg in every CI run
  - ❌ Network dependency (slower, can fail)

- **Alternative C: Commit entire vcpkg directory**
  - ❌ Huge repo size (vcpkg is ~500MB)
  - ❌ Violates best practices

## Implementation Plan (Checklist)

### 1. Backup & Cleanup
- [ ] Verify current local vcpkg state (`git submodule status`)
- [ ] Check if vcpkg binary can be safely removed locally
- [ ] Backup current .gitignore vcpkg section

### 2. Update .gitignore
- [ ] Remove `/vcpkg` from .gitignore (allow submodule)
- [ ] Keep `/vcpkg_installed/` (build artifacts)
- [ ] Keep `vcpkg.exe` (Windows binary)
- [ ] Keep `vcpkg.disable-metrics`

### 3. Add vcpkg Submodule
- [ ] Run: `git submodule add https://github.com/microsoft/vcpkg.git vcpkg`
- [ ] Verify .gitmodules created
- [ ] Run: `cd vcpkg && git checkout 271a5b8850aa50f9a40269cbf3cf414b36e333d6`
- [ ] Run: `cd .. && git add .gitmodules vcpkg`
- [ ] Verify submodule commit hash matches vcpkg.json baseline

### 4. Verify Workflows
- [ ] Check all three workflows use `submodules: recursive` (they do!)
- [ ] Verify bootstrap steps in all workflows are identical
- [ ] Verify CMake toolchain path is correct in all workflows

### 5. Local Testing
- [ ] Run: `git submodule update --init --recursive`
- [ ] Run: `./vcpkg/bootstrap-vcpkg.sh` (or `.bat` on Windows)
- [ ] Verify `./vcpkg/vcpkg version` works
- [ ] Clean build: `rm -rf build && cmake -B build ...`
- [ ] Verify build succeeds locally

### 6. Commit & Push
- [ ] Commit with message: "fix: Add vcpkg as git submodule (fixes Linux CI)"
- [ ] Push to GitHub
- [ ] Monitor CI immediately

### 7. Verification
- [ ] Linux Debug build passes
- [ ] Linux Release build passes
- [ ] macOS Debug build still passes
- [ ] macOS Release build still passes
- [ ] Windows Debug build passes
- [ ] Windows Release build passes

## Risks & Open Questions

### Risks
- **Risk:** Local development environment broken after submodule add
  - **Mitigation:** Test in clean directory first, backup current setup

- **Risk:** Submodule adds complexity for contributors
  - **Mitigation:** Document in README: `git clone --recursive`

- **Risk:** Existing CI cache invalidated
  - **Mitigation:** Cache key includes `hashFiles('vcpkg.json')`, should be fine

### Open Questions
- **Q:** Should we remove local vcpkg binary first?
  - **A:** Git submodule add should handle it, but check first

- **Q:** Will existing cache work with submodule?
  - **A:** Yes, cache paths are `vcpkg/` and `build/vcpkg_installed/`

- **Q:** Do we need to update checkout actions?
  - **A:** No, `submodules: recursive` already present (line 25)

## Status
- **Created:** 2025-10-26
- **Approved:** 2025-10-26 (by User - Option A)
- **Started:** 2025-10-26
- **Completed:** YYYY-MM-DD

## Implementation Notes

### Investigation Update (2025-10-26)

**Initial diagnosis was INCORRECT!**

New findings:
1. ✅ vcpkg **IS** a git submodule (verified locally and on GitHub)
2. ✅ .gitmodules exists and is correct
3. ✅ Submodule is tracked in git (commit 160000 271a5b88...)
4. ✅ Local build works perfectly (cmake configure successful)
5. ✅ Bootstrap script exists and works

**Revised Root Cause Hypothesis:**

**Problem:** CI cache restore may be interfering with vcpkg bootstrap!

**Sequence of events in CI:**
1. Checkout code + submodules (vcpkg directory populated but not bootstrapped)
2. **actions/cache@v4 restores cache** (key includes vcpkg/)
   - If cache exists, it restores vcpkg directory
   - This MAY overwrite the fresh submodule checkout
   - Cached vcpkg might be in inconsistent state
3. Bootstrap vcpkg runs unconditionally
   - If cache was corrupted/partial, bootstrap may fail
   - No check if vcpkg is already bootstrapped

**Evidence:**
- macOS CI passes (maybe cache works there, or no cache hit)
- Linux CI fails (cache corruption or incompatible state?)
- Windows CI cancelled (dependency on Linux?)

**New Solution:**
Add conditional bootstrap - only run if vcpkg binary doesn't exist:

```yaml
- name: Bootstrap vcpkg
  run: |
    if [ ! -f vcpkg/vcpkg ]; then
      cd vcpkg
      ./bootstrap-vcpkg.sh
    else
      echo "vcpkg already bootstrapped (from cache)"
    fi
```

This ensures:
- If cache provides working vcpkg → skip bootstrap (faster)
- If cache is empty/corrupted → bootstrap runs
- Idempotent operation

## Verification
- [ ] Linux CI passes (both Debug and Release)
- [ ] macOS CI still passes (both Debug and Release)
- [ ] Windows CI passes (both Debug and Release)
- [ ] Local build works after submodule add
- [ ] .gitmodules file present and correct
- [ ] Submodule commit pinned to correct baseline
- [ ] CHANGELOG.md updated
- [ ] CI status verified on GitHub

## Related Tasks
- **Blocks:** Task #00001 (Plugin Manager Skeleton)
- **Blocks:** All Phase 0 Week 2+ tasks
- **Related:** ROADMAP.md Phase 0 Week 1 infrastructure

## Post-Completion Review
(Will be filled after completion)

**Expected time:** 30-60 minutes
**Priority:** 🔴 **CRITICAL BLOCKER** - Must fix before Week 2
