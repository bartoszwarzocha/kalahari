# Proposal: Qt6 Hello World - Minimal QMainWindow

**Change ID:** `00001-qt6-hello-world`
**Type:** Implementation
**Phase:** 0 (Qt Foundation)
**Estimated Effort:** 2-3 hours
**Actual Time:** ~45 minutes
**Task Number:** #00001

---

## Why

Validate Qt6 installation, CMake configuration, and build system on all platforms. Create foundation for all GUI development.

---

## What Changes

- Update CMake to find Qt6::Gui component
- Replace QMessageBox placeholder with minimal QMainWindow
- Implement Qt event loop with app.exec()
- Set window title and size (1280x720)

---

## Impact

**Affected code:**
- Modified: `CMakeLists.txt` (add Gui component)
- Modified: `src/CMakeLists.txt` (link Qt6::Gui)
- Modified: `src/main.cpp` (QMainWindow implementation)

---

## Status

**Status:** ✅ DONE
**Completed:** 2025-11-20
**Implementation Time:** ~45 minutes

---

## Success Criteria

- [x] Empty QMainWindow appears when app runs
- [x] Window title: "Kalahari Writer's IDE"
- [x] Window size: 1280x720 px
- [x] Build successful on all platforms
- [x] CI/CD passing
