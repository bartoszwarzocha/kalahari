# Specification: Editor Settings

**Capability ID:** `gui/settings-dialog`
**Type:** GUI Component
**Phase:** 0 (Qt Foundation)
**Status:** Migrated from tasks/00006 (DONE)

---

## ADDED Requirements

### Requirement: Editor Controls

Settings dialog SHALL provide editor customization controls.

**ID:** `gui/settings-dialog/editor`
**Priority:** Medium
**Phase:** 0

#### Scenario: Editor settings work

```
GIVEN the user opens Settings → Editor tab
THEN font family selection SHALL be available
AND font size adjustment SHALL be available
AND tab size adjustment SHALL be available
AND line numbers toggle SHALL be available
AND word wrap toggle SHALL be available
```

---

**Specification Version:** 1.0
**Status:** ✅ DONE (2025-11-20)
**Change ID:** `00006-editor-settings`
