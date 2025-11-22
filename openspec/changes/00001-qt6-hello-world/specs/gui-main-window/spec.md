# Specification: Main Window (Qt6 Hello World)

**Capability ID:** `gui/main-window`
**Type:** GUI Component
**Phase:** 0 (Qt Foundation)
**Status:** Migrated from tasks/00001 (DONE)

---

## ADDED Requirements

### Requirement: Minimal QMainWindow

The application SHALL provide a minimal QMainWindow that displays on launch.

**ID:** `gui/main-window/minimal`
**Priority:** Critical
**Phase:** 0

**Rationale:** Foundation for all GUI development. Validates Qt6 setup.

#### Scenario: Application launches

```
GIVEN the application is built successfully
WHEN the user runs the executable
THEN a QMainWindow SHALL appear
AND the window title SHALL be "Kalahari Writer's IDE"
AND the window size SHALL be 1280x720 pixels
AND the window SHALL remain open until closed by user
```

---

### Requirement: Qt Event Loop

The application SHALL run Qt event loop to process GUI events.

**ID:** `gui/main-window/event-loop`
**Priority:** Critical
**Phase:** 0

**Rationale:** Required for responsive GUI (window resizing, closing, etc.).

#### Scenario: Event loop runs

```
GIVEN the application has launched
WHEN the main window is shown
THEN app.exec() SHALL be called
AND the application SHALL remain running until window closed
AND the application SHALL exit cleanly with code 0
```

---

**Specification Version:** 1.0
**Status:** ✅ DONE (2025-11-20)
**Change ID:** `00001-qt6-hello-world`
