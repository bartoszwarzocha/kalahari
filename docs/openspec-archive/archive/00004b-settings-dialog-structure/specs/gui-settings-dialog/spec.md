# Specification: Settings Dialog

**Capability ID:** `gui/settings-dialog`
**Type:** GUI Component
**Phase:** 0 (Qt Foundation)
**Status:** Migrated from tasks/00004b (DONE)

---

## ADDED Requirements

### Requirement: Tabbed Settings Dialog

The application SHALL provide a settings dialog with tabbed interface.

**ID:** `gui/settings-dialog/tabbed`
**Priority:** Medium
**Phase:** 0

#### Scenario: Settings dialog opens

```
GIVEN the user selects Edit → Settings
THEN a modal dialog SHALL appear
AND dialog SHALL contain QTabWidget
AND tabs SHALL include: Appearance, Editor
AND buttons SHALL include: Apply, OK, Cancel
```

---

**Specification Version:** 1.0
**Status:** ✅ DONE (2025-11-20)
**Change ID:** `00004b-settings-dialog-structure`
