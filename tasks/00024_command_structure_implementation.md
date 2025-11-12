# Task #00024: Command Structure Implementation

**Status:** ✅ COMPLETE
**Priority:** P1 (HIGH - Architecture Foundation)
**Estimated:** 60 minutes
**Actual:** 45 minutes
**Dependencies:** None
**Phase:** Phase 0 - Architecture
**Completed:** 2025-11-12

---

## Goal

Implement core data structures for Command Registry system: `Command`, `IconSet`, and `KeyboardShortcut`.

---

## Requirements

### 1. Create `include/kalahari/gui/command.h`

Define three fundamental structures:

**IconSet:**
```cpp
struct IconSet {
    wxBitmap icon16;  // 16x16 - menus
    wxBitmap icon24;  // 24x24 - default toolbars
    wxBitmap icon32;  // 32x32 - large toolbars

    IconSet() = default;
    IconSet(const wxString& path);  // Load from single SVG (auto-scale)
};
```

**KeyboardShortcut:**
```cpp
struct KeyboardShortcut {
    int keyCode;           // wxKeyCode (e.g., 'S', WXK_F1)
    bool ctrl = false;
    bool alt = false;
    bool shift = false;

    wxString ToString() const;  // "Ctrl+S", "Ctrl+Shift+A"
    static KeyboardShortcut FromString(const wxString& str);

    bool operator==(const KeyboardShortcut& other) const;
};
```

**Command:**
```cpp
struct Command {
    // Identification
    std::string id;              // "file.save", "edit.undo"
    std::string label;           // "Save"
    std::string tooltip;         // "Save current document"
    std::string category;        // "File", "Edit"

    // Visual
    IconSet icons;
    bool showInMenu = true;
    bool showInToolbar = false;

    // Keyboard
    KeyboardShortcut shortcut;
    bool isShortcutCustomizable = true;

    // Execution
    std::function<void()> execute;
    std::function<bool()> isEnabled;
    std::function<bool()> isChecked;  // For toggle commands

    // Metadata
    bool isPluginCommand = false;
    std::string pluginId;
    int apiVersion = 1;
};
```

### 2. Implement in `src/gui/command.cpp`

- IconSet constructor (load from SVG path, scale to 3 sizes)
- KeyboardShortcut::ToString() implementation
- KeyboardShortcut::FromString() implementation
- operator== for KeyboardShortcut

---

## Implementation Notes

- IconSet(path) can use wxImage::Scale() for now (SVG scaling in Phase 2+)
- KeyboardShortcut::ToString() format: "Ctrl+S", "Ctrl+Shift+F1", "Alt+X"
- FromString() should handle common formats (case-insensitive)

---

## Acceptance Criteria

- [x] `command.h` created with 3 structures
- [x] `command.cpp` implements ToString/FromString
- [x] IconSet can load bitmap and create 3 sizes
- [x] KeyboardShortcut parses "Ctrl+S" correctly
- [x] Code compiles without errors
- [x] No new warnings introduced

---

## Testing

Manual verification:
```cpp
KeyboardShortcut sc;
sc.ctrl = true;
sc.keyCode = 'S';
assert(sc.ToString() == "Ctrl+S");

auto parsed = KeyboardShortcut::FromString("Ctrl+Shift+A");
assert(parsed.ctrl == true);
assert(parsed.shift == true);
assert(parsed.keyCode == 'A');
```

---

## Related Files

- `include/kalahari/gui/command.h` (new)
- `src/gui/command.cpp` (new)
- `src/CMakeLists.txt` (add command.cpp)

---

## Next Task

Task #00025 - CommandRegistry Singleton + Registration

---

**Created:** 2025-11-11
**Author:** Architecture Planning
