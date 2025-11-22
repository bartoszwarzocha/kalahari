# Proposal: QMainWindow Skeleton

**Change ID:** `00002-qmainwindow-structure`
**Type:** Implementation
**Phase:** 0 (Qt Foundation)
**Task Number:** #00002

---

## Why

Create proper MainWindow structure with menus, toolbar, and status bar.

---

## What Changes

- Create MainWindow class (QMainWindow subclass)
- Add File menu (New, Open, Save, Exit)
- Add Edit menu (Undo, Redo, Cut, Copy, Paste, Settings)
- Add toolbar with File actions
- Add status bar with "Ready" message
- Integrate Logger and SettingsManager

---

## Impact

**Affected code:**
- New: `include/kalahari/gui/main_window.h`
- New: `src/gui/main_window.cpp`
- Modified: `src/main.cpp`
- Modified: `src/CMakeLists.txt`

---

## Status

**Status:** ✅ DONE
**Completed:** 2025-11-20
