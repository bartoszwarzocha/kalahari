# Specification: Main Window Structure

**Capability ID:** `gui/main-window`
**Type:** GUI Component
**Phase:** 0 (Qt Foundation)
**Status:** Migrated from tasks/00002 (DONE)

---

## ADDED Requirements

### Requirement: Menu Bar

The application SHALL provide a menu bar with File and Edit menus.

**ID:** `gui/main-window/menu-bar`
**Priority:** High
**Phase:** 0

#### Scenario: Menus available

```
GIVEN the application is running
THEN a menu bar SHALL be visible
AND File menu SHALL contain: New, Open, Save, Exit
AND Edit menu SHALL contain: Undo, Redo, Cut, Copy, Paste, Settings
```

---

### Requirement: Toolbar

The application SHALL provide a toolbar with common File actions.

**ID:** `gui/main-window/toolbar`
**Priority:** Medium
**Phase:** 0

#### Scenario: Toolbar functional

```
GIVEN the application is running
THEN a toolbar SHALL be visible below menu bar
AND toolbar SHALL contain: New, Open, Save buttons
```

---

**Specification Version:** 1.0
**Status:** ✅ DONE (2025-11-20)
**Change ID:** `00002-qmainwindow-structure`
