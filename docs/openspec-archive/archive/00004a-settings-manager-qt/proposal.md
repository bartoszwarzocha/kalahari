# Proposal: Settings Manager Qt Migration

**Change ID:** `00004a-settings-manager-qt`
**Type:** Migration
**Phase:** 0 (Qt Foundation)
**Task Number:** #00004a

---

## Why

Migrate SettingsManager and CmdLineParser from wxWidgets types (wxSize, wxPoint) to Qt6 types (QSize, QPoint).

---

## What Changes

- Replace wxSize with QSize in SettingsManager
- Replace wxPoint with QPoint in SettingsManager
- Update all usages in codebase

---

## Status

**Status:** ✅ DONE
**Completed:** 2025-11-20
