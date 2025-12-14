# Specification: Dock Panel System

**Capability ID:** `gui/dock-panels`
**Type:** GUI Component
**Phase:** 0 (Qt Foundation)
**Status:** Migrated from tasks/00003 (DONE)

---

## ADDED Requirements

### Requirement: Dockable Panels

The application SHALL provide 5 dockable panels that can be moved and resized.

**ID:** `gui/dock-panels/dockable`
**Priority:** High
**Phase:** 0

#### Scenario: Panels available

```
GIVEN the application is running
THEN 5 dock panels SHALL be available
AND panels SHALL be: Navigator, Properties, Log, Search, Assistant
AND each panel SHALL be movable and dockable
AND View menu SHALL allow toggling each panel
```

---

### Requirement: Perspective Persistence

The application SHALL save and restore panel layout between sessions.

**ID:** `gui/dock-panels/persistence`
**Priority:** Medium
**Phase:** 0

#### Scenario: Layout persists

```
GIVEN the user has customized panel layout
WHEN the application is closed and reopened
THEN the panel layout SHALL be restored
AND panel visibility states SHALL be preserved
```

---

**Specification Version:** 1.0
**Status:** ✅ DONE (2025-11-20)
**Change ID:** `00003-qdockwidget-system`
