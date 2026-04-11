# Specification: Settings Manager (Qt)

**Capability ID:** `core/settings`
**Type:** Core Component
**Phase:** 0 (Qt Foundation)
**Status:** Migrated from tasks/00004a (DONE)

---

## ADDED Requirements

### Requirement: Qt Type Support

Settings Manager SHALL use Qt6 types for geometry and position data.

**ID:** `core/settings/qt-types`
**Priority:** High
**Phase:** 0

#### Scenario: Qt types used

```
GIVEN SettingsManager is initialized
WHEN reading window geometry
THEN QSize SHALL be used for window size
AND QPoint SHALL be used for window position
```

---

**Specification Version:** 1.0
**Status:** ✅ DONE (2025-11-20)
**Change ID:** `00004a-settings-manager-qt`
