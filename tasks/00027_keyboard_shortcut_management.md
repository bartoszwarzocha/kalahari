# Task #00027: Keyboard Shortcut Management

**Status:** 📋 Planned
**Priority:** P1 (HIGH - Architecture Foundation)
**Estimated:** 90 minutes
**Dependencies:** #00025 (CommandRegistry), #00024 (KeyboardShortcut structure)
**Phase:** Phase 0 - Architecture

---

## Goal

Implement keyboard shortcut mapping, conflict detection, and persistence in ShortcutManager.

---

## Requirements

### 1. Create `include/kalahari/gui/shortcut_manager.h`

```cpp
class ShortcutManager {
public:
    static ShortcutManager& getInstance();

    // Mapping
    void bindShortcut(const KeyboardShortcut& shortcut, const std::string& commandId);
    void unbindShortcut(const KeyboardShortcut& shortcut);
    std::string getCommandForShortcut(const KeyboardShortcut& shortcut) const;

    // Conflict detection
    bool hasConflict(const KeyboardShortcut& shortcut) const;
    std::vector<std::string> getConflicts(const KeyboardShortcut& shortcut) const;

    // Persistence
    void saveToFile(const std::string& path) const;
    void loadFromFile(const std::string& path);

    // Reset
    void resetToDefaults();

private:
    ShortcutManager() = default;
    ~ShortcutManager() = default;
    ShortcutManager(const ShortcutManager&) = delete;
    ShortcutManager& operator=(const ShortcutManager&) = delete;

    std::map<KeyboardShortcut, std::string> m_shortcutMap;
    std::map<KeyboardShortcut, std::string> m_defaultShortcuts;
};

// Required for std::map
bool operator<(const KeyboardShortcut& lhs, const KeyboardShortcut& rhs);
```

### 2. Implement in `src/gui/shortcut_manager.cpp`

**bindShortcut:**
- Add mapping to m_shortcutMap
- If shortcut already exists, log warning and replace

**unbindShortcut:**
- Remove from m_shortcutMap

**getCommandForShortcut:**
- Lookup in m_shortcutMap
- Return empty string if not found

**hasConflict:**
- Return true if shortcut exists in m_shortcutMap

**getConflicts:**
- If shortcut exists, return vector with single command id
- Otherwise return empty vector
- (Future: scope-aware conflicts)

**saveToFile/loadFromFile:**
- JSON format using nlohmann_json
- Structure: `{"shortcuts": [{"key": "Ctrl+S", "command": "file.save"}, ...]}`

**resetToDefaults:**
- Clear m_shortcutMap
- Copy m_defaultShortcuts to m_shortcutMap

**operator< for KeyboardShortcut:**
- Compare by: ctrl, alt, shift, keyCode (in that order)
- Required for std::map key

---

## Implementation Notes

**JSON Format:**
```json
{
    "shortcuts": [
        {"key": "Ctrl+S", "command": "file.save"},
        {"key": "Ctrl+O", "command": "file.open"},
        {"key": "F1", "command": "help.contents"}
    ]
}
```

**Conflict Detection (Phase 0 - Simple):**
- Only detect exact matches (same key + modifiers)
- Phase 2+: Context-aware conflicts (editor vs navigator shortcuts)

---

## Acceptance Criteria

- [ ] `shortcut_manager.h` created with singleton interface
- [ ] `shortcut_manager.cpp` implements all methods
- [ ] bindShortcut stores mapping correctly
- [ ] getCommandForShortcut returns correct command id
- [ ] hasConflict detects existing shortcuts
- [ ] saveToFile/loadFromFile work with JSON format
- [ ] operator< allows KeyboardShortcut as map key
- [ ] Code compiles, no warnings

---

## Testing

Manual verification:
```cpp
auto& sm = ShortcutManager::getInstance();

KeyboardShortcut ctrlS;
ctrlS.ctrl = true;
ctrlS.keyCode = 'S';

sm.bindShortcut(ctrlS, "file.save");

assert(sm.getCommandForShortcut(ctrlS) == "file.save");
assert(sm.hasConflict(ctrlS));

sm.unbindShortcut(ctrlS);
assert(sm.getCommandForShortcut(ctrlS) == "");

// Test persistence
sm.bindShortcut(ctrlS, "file.save");
sm.saveToFile("test_shortcuts.json");

ShortcutManager& sm2 = ShortcutManager::getInstance();
sm2.loadFromFile("test_shortcuts.json");
assert(sm2.getCommandForShortcut(ctrlS) == "file.save");
```

---

## Related Files

- `include/kalahari/gui/shortcut_manager.h` (new)
- `src/gui/shortcut_manager.cpp` (new)
- `src/CMakeLists.txt` (add shortcut_manager.cpp)

---

## Next Task

Task #00028 - Core Command Registration - File Menu

---

**Created:** 2025-11-11
**Author:** Architecture Planning
