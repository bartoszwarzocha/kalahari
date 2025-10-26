# Kalahari Release Quality Checklist

**Version:** 2.0
**Last Updated:** 2025-10-26
**Purpose:** Manual quality verification for releases

---

## 📋 About This Checklist

**This checklist is for RELEASE preparation ONLY.**

For everyday commits, use automated checks:
```bash
# Run before every commit
./tools/pre-commit-check.sh
```

This checklist covers **manual verification** that cannot be automated.

---

## ✅ PRE-RELEASE CHECKLIST

### 1. Automated Checks

- [ ] **All automated checks passing**
  ```bash
  ./tools/pre-commit-check.sh
  # Score must be 100%
  ```

- [ ] **CI/CD passing on all platforms**
  ```bash
  ./tools/check-ci.sh status
  # All 3 platforms (Windows/Linux/macOS) × 2 configs (Debug/Release) = 6 jobs ✅
  ```

- [ ] **All tests passing**
  ```bash
  cd build && ctest --output-on-failure
  # 0 failed tests
  ```

---

### 2. Manual Testing

- [ ] **UX Testing Performed**
  - [ ] Application launches on all 3 platforms
  - [ ] Main workflows tested (create project, edit, save, export)
  - [ ] No UI freezes or crashes
  - [ ] Dark mode works correctly
  - [ ] Docking panels work (drag, close, restore)
  - [ ] Focus modes work (Normal, Focused, Distraction-free)

- [ ] **Accessibility Verified**
  - [ ] Keyboard navigation works
  - [ ] Screen reader compatible (ARIA labels)
  - [ ] High contrast themes readable

- [ ] **Performance Acceptable**
  - [ ] Startup time < 3 seconds
  - [ ] Large documents (10,000+ words) load within 5 seconds
  - [ ] No memory leaks (run for 1 hour, check memory usage)

---

### 3. Documentation

- [ ] **CHANGELOG.md Updated**
  - [ ] All changes since last release documented
  - [ ] Categorized: Added/Changed/Fixed/Removed
  - [ ] Version number correct
  - [ ] Release date set

- [ ] **User Documentation Current**
  - [ ] README.md reflects current features
  - [ ] Installation instructions accurate
  - [ ] Screenshots up-to-date
  - [ ] Tutorials reflect current UI

- [ ] **API Documentation Generated**
  ```bash
  doxygen Doxyfile
  # Check docs/api/html/ for completeness
  ```

---

### 4. Release Preparation

- [ ] **Version Number Bumped**
  - [ ] CMakeLists.txt: `project(Kalahari VERSION X.Y.Z)`
  - [ ] CLAUDE.md: Document version updated
  - [ ] Follows Semantic Versioning (MAJOR.MINOR.PATCH)

- [ ] **Release Notes Written**
  - [ ] Highlights (3-5 major features/fixes)
  - [ ] Breaking changes listed
  - [ ] Migration guide if needed
  - [ ] Known issues documented

- [ ] **Git Tags Prepared**
  ```bash
  git tag -a vX.Y.Z -m "Release X.Y.Z"
  # Don't push yet - wait for final approval
  ```

---

### 5. Installers & Packaging

- [ ] **Installers Built & Tested**
  - [ ] **Windows**: NSIS installer tested
    - [ ] Installation works
    - [ ] Uninstallation works
    - [ ] File associations work (.klh files)
    - [ ] Shortcuts created
  - [ ] **macOS**: DMG tested
    - [ ] Application launches from DMG
    - [ ] Application can be moved to /Applications
    - [ ] Signed (if applicable)
  - [ ] **Linux**: AppImage tested
    - [ ] Launches on Ubuntu 22.04+
    - [ ] Launches on Fedora 38+
    - [ ] Desktop integration works

- [ ] **Embedded Python Bundled**
  - [ ] Python 3.11 included in installers
  - [ ] All required pip packages bundled
  - [ ] Plugins load correctly

---

### 6. Localization (i18n/l10n)

- [ ] **Translations Complete**
  - [ ] English: 100%
  - [ ] Polish: 100%
  - [ ] Other languages: Acceptable % (list here)

- [ ] **Language Switching Works**
  - [ ] Test switching between EN ↔ PL at runtime
  - [ ] No untranslated strings in UI

---

### 7. Licensing & Legal

- [ ] **License Files Present**
  - [ ] LICENSE (MIT)
  - [ ] THIRD_PARTY_LICENSES.txt (vcpkg dependencies)

- [ ] **Trademark Compliance**
  - [ ] "Kalahari" branding consistent
  - [ ] Logo usage correct
  - [ ] No trademark violations

---

### 8. Security

- [ ] **No Hardcoded Secrets**
  ```bash
  # Already checked by pre-commit-check.sh, but verify manually
  grep -r "password\|api_key\|secret\|token" src/ include/
  ```

- [ ] **Dependencies Up-to-Date**
  ```bash
  # Check for known vulnerabilities
  vcpkg/vcpkg update
  ```

---

### 9. Communication

- [ ] **Stakeholders Notified**
  - [ ] Team informed of release schedule
  - [ ] Beta testers recruited (if applicable)
  - [ ] Community announcement prepared

- [ ] **Release Channels Ready**
  - [ ] GitHub Release draft created
  - [ ] Download links prepared
  - [ ] Social media posts drafted

---

### 10. Final Approval

- [ ] **Code Review Complete**
  - [ ] All PRs merged
  - [ ] No pending changes

- [ ] **Project Lead Approval**
  - [ ] User (project manager) approves release
  - [ ] Go/No-Go decision made

---

## 📊 RELEASE READINESS SCORE

Count completed checkboxes above. Calculate:

**Score = (Completed / Total) × 100%**

**Release Gates:**
- **< 90%**: ❌ **DO NOT RELEASE**
- **90-95%**: ⚠️ **RELEASE WITH CAUTION** (document known issues)
- **95-100%**: ✅ **READY FOR RELEASE**

---

## 🚀 RELEASE EXECUTION

Once checklist is 95%+ complete:

1. **Push Git Tag**
   ```bash
   git push origin vX.Y.Z
   ```

2. **Create GitHub Release**
   - Upload installers (Windows, macOS, Linux)
   - Copy release notes
   - Mark as pre-release (if beta) or stable

3. **Announce Release**
   - Social media
   - Email list (if applicable)
   - Community forums

4. **Monitor Post-Release**
   - Watch for bug reports (first 48 hours critical)
   - Prepare hotfix branch if needed

---

## 🔄 POST-RELEASE

- [ ] Update ROADMAP.md (mark completed phase)
- [ ] Create milestone for next release
- [ ] Archive release artifacts
- [ ] Thank contributors

---

**Document Version:** 2.0 (Release-Focused)
**Replaces:** v1.0 (454-line comprehensive checklist)
**Automation:** See `tools/pre-commit-check.sh` for automated checks
