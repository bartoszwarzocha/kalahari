# Specification: Command Registry

**Capability ID:** `gui/command-system`
**Type:** GUI Architecture
**Phase:** 0 (Qt Foundation)
**Status:** Migrated from tasks/00013 (DONE)

---

## ADDED Requirements

### Requirement: Command Registry System

The application SHALL provide a centralized command registry for all UI actions.

**ID:** `gui/command-system/registry`
**Priority:** High
**Phase:** 0

#### Scenario: Command registry works

```
GIVEN commands are registered in CommandRegistry
WHEN menus or toolbars are built
THEN commands SHALL be accessible
AND commands SHALL have consistent behavior
```

---

**Specification Version:** 1.0
**Status:** ✅ DONE (2025-11-21)
**Change ID:** `00013-command-registry`
