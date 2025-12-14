# Specification: File Operations

**Capability ID:** `gui/file-ops`
**Type:** GUI Feature
**Phase:** 0 (Qt Foundation)
**Status:** Migrated from tasks/00008 (DONE)

---

## ADDED Requirements

### Requirement: File Operations

The application SHALL support New, Open, Save operations.

**ID:** `gui/file-ops/basic`
**Priority:** High
**Phase:** 0

#### Scenario: File operations work

```
GIVEN the application is running
WHEN the user selects File → New/Open/Save
THEN the corresponding operation SHALL execute
```

---

**Specification Version:** 1.0
**Status:** ✅ DONE (2025-11-20)
**Change ID:** `00008-file-operations`
