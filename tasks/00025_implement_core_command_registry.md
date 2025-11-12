# Task #00025: Implement Core Command Registry

**Status:** ✅ COMPLETE
**Priority:** P0 (ARCHITECTURE - Foundation)
**Estimated:** 60-90 minutes
**Actual:** 75 minutes
**Dependencies:** #00024 (Command Structure Implementation)
**Phase:** Phase 1 - Command Registry Architecture
**Completed:** 2025-11-12

---

## Goal

Implement CommandRegistry singleton class for centralized command management, registration, and retrieval.

---

## Requirements

### 1. Create `include/kalahari/gui/command_registry.h`

**CommandRegistry class:**
```cpp
class CommandRegistry {
public:
    static CommandRegistry& getInstance();  // Meyers singleton

    // Registration
    void registerCommand(const Command& command);
    void unregisterCommand(const std::string& commandId);
    bool isCommandRegistered(const std::string& commandId) const;

    // Query
    const Command* getCommand(const std::string& commandId) const;
    Command* getCommand(const std::string& commandId);
    std::vector<Command> getCommandsByCategory(const std::string& category) const;
    std::vector<Command> getAllCommands() const;
    std::vector<std::string> getCategories() const;

    // Utility
    size_t getCommandCount() const;
    void clear();  // For testing

private:
    CommandRegistry() = default;
    ~CommandRegistry() = default;
    CommandRegistry(const CommandRegistry&) = delete;
    CommandRegistry& operator=(const CommandRegistry&) = delete;

    std::unordered_map<std::string, Command> m_commands;
};
```

### 2. Implement in `src/gui/command_registry.cpp`

- **getInstance()** - Meyers singleton (thread-safe C++11+)
- **registerCommand()** - Add/override command in map
- **unregisterCommand()** - Remove command (safe if doesn't exist)
- **isCommandRegistered()** - Check existence
- **getCommand()** - Return pointer (const and non-const versions)
- **getCommandsByCategory()** - Filter by category, return vector
- **getAllCommands()** - Return all commands
- **getCategories()** - Return unique sorted category names
- **getCommandCount()** - Return map size
- **clear()** - Clear all commands (for testing)

### 3. Update `src/CMakeLists.txt`

Add `gui/command_registry.cpp` to KALAHARI_SOURCES.

### 4. Write Unit Tests (Catch2)

Create `tests/gui/test_command_registry.cpp` with test cases:
1. Singleton pattern verification
2. Command registration
3. Command registration with duplicate ID (override)
4. Multiple command registration
5. Command unregistration
6. Unregistration of non-existent command (safety)
7. Command retrieval (const)
8. Command retrieval returns nullptr for non-existent
9. Command retrieval (non-const) allows modification
10. Category filtering
11. Category filtering with non-existent category
12. Get all commands
13. Get all commands when empty
14. Get categories (unique and sorted)
15. Get categories when empty
16. getCommandCount()
17. clear() utility method

Update `tests/CMakeLists.txt` to include test file and source files.

---

## Implementation Notes

- **Thread-safety:** Meyers singleton guarantees thread-safe initialization (C++11+)
- **Command registration:** Override strategy (duplicate ID replaces existing)
- **getCategories():** Use std::set for automatic sorting and uniqueness
- **Storage:** std::unordered_map<std::string, Command> for O(1) lookup
- **No wxWidgets display required:** All tests are pure C++ logic (compatible with CI/CD)

---

## Acceptance Criteria

- [x] `command_registry.h` created with CommandRegistry class
- [x] `command_registry.cpp` implements all methods
- [x] Meyers singleton pattern (thread-safe)
- [x] All registration/query methods working
- [x] 8 test cases implemented (17 test sections total)
- [x] Code compiles without errors
- [x] No new warnings
- [x] Tests pass 100% (40 assertions, 8 test cases)
- [x] Full test suite passes (572 assertions, 76 test cases)

---

## Testing Results

**CommandRegistry-specific tests:**
```
All tests passed (40 assertions in 8 test cases)
Filters: [command] [registry]
```

**Full test suite:**
```
All tests passed (572 assertions in 76 test cases)
```

**Test coverage:**
- Singleton pattern ✅
- Registration/unregistration ✅
- Query operations ✅
- Category filtering ✅
- Utility methods ✅

---

## Files Created/Modified

**Created:**
- `include/kalahari/gui/command_registry.h` (144 LOC)
- `src/gui/command_registry.cpp` (106 LOC)
- `tests/gui/test_command_registry.cpp` (283 LOC)

**Modified:**
- `src/CMakeLists.txt` (+1 LOC) - Added command_registry.cpp
- `tests/CMakeLists.txt` (+3 LOC) - Added test file and source files

---

## Architecture Notes

**CommandRegistry Design:**
- Central singleton for all command management
- Foundation for unified execution path (menu, toolbar, keyboard)
- Plugin-ready (plugins can register custom commands)
- Thread-safe initialization, main-thread registration recommended
- Override strategy allows command updates (e.g., enable/disable state changes)

**Integration Points:**
- Task #00026: Command execution + context
- Task #00027: Keyboard shortcut management
- Task #00028-00030: Core command registration (File, Edit, Help menus)
- Task #00031+: Menu/toolbar builders will query CommandRegistry

---

## Next Task

**Task #00026:** CommandRegistry Execution + Context (75 min estimate)
- executeCommand with error handling
- canExecute (enabled state checking)
- isChecked (for toggle commands)
- Custom error handler support

---

**Created:** 2025-11-12
**Completed:** 2025-11-12
**Duration:** 75 minutes (within 60-90 min estimate)
