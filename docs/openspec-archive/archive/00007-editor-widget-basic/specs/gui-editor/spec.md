# Specification: Editor Widget

**Capability ID:** `gui/editor`
**Type:** GUI Component
**Phase:** 0 (Qt Foundation)
**Status:** Migrated from tasks/00007 (DONE)

---

## ADDED Requirements

### Requirement: Text Editor

The application SHALL provide a text editor widget in central area.

**ID:** `gui/editor/basic-text`
**Priority:** High
**Phase:** 0

#### Scenario: Editor functional

```
GIVEN the application is running
THEN a text editor SHALL be visible in central area
AND the editor SHALL support basic text input
AND the editor SHALL support text selection
AND the editor SHALL support copy/paste operations
```

---

**Specification Version:** 1.0
**Status:** ✅ DONE (2025-11-20)
**Change ID:** `00007-editor-widget-basic`
