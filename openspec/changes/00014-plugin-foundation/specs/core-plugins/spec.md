# Specification: Plugin System Foundation

**Capability ID:** `core/plugins`
**Type:** Core Architecture
**Phase:** 0 (Qt Foundation)
**Status:** Migrated from tasks/00014 (DONE)

---

## ADDED Requirements

### Requirement: Qt6-Compatible Plugin System

Plugin system SHALL be compatible with Qt6 (no wxWidgets dependencies).

**ID:** `core/plugins/qt6-compat`
**Priority:** Critical
**Phase:** 0

#### Scenario: Plugin system Qt-ready

```
GIVEN plugin system is implemented
THEN ICommandProvider SHALL allow command registration
AND EventBus SHALL use Qt6 event marshalling
AND IPanelProvider SHALL use QWidget* (not void*)
```

---

**Specification Version:** 1.0
**Status:** ✅ DONE (2025-11-21)
**Change ID:** `00014-plugin-foundation`
