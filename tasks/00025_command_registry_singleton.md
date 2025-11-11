# Task #00025: CommandRegistry Singleton + Registration

**Status:** 📋 Planned
**Priority:** P1 (HIGH - Architecture Foundation)
**Estimated:** 90 minutes
**Dependencies:** #00024 (Command structures)
**Phase:** Phase 0 - Architecture

---

## Goal

Implement CommandRegistry singleton with command registration, query, and basic storage.

---

## Requirements

### 1. Create `include/kalahari/gui/command_registry.h`

```cpp
class CommandRegistry {
public:
    static CommandRegistry& getInstance();

    // Registration
    void registerCommand(const Command& cmd);
    void unregisterCommand(const std::string& id);
    bool isCommandRegistered(const std::string& id) const;

    // Query
    Command* getCommand(const std::string& id);
    const Command* getCommand(const std::string& id) const;
    std::vector<const Command*> getCommandsByCategory(const std::string& category) const;
    std::vector<const Command*> getAllCommands() const;
    std::vector<std::string> getCategories() const;

private:
    CommandRegistry() = default;
    ~CommandRegistry() = default;
    CommandRegistry(const CommandRegistry&) = delete;
    CommandRegistry& operator=(const CommandRegistry&) = delete;

    std::unordered_map<std::string, Command> m_commands;
    std::map<std::string, std::vector<std::string>> m_categorizedCommands;
};
```

### 2. Implement in `src/gui/command_registry.cpp`

- Singleton pattern with thread-safe getInstance()
- registerCommand: Add to m_commands + categorize
- unregisterCommand: Remove from both maps
- Query methods: Return pointers/vectors from storage

---

## Implementation Notes

**Thread Safety:**
- getInstance() uses Meyer's Singleton (C++11 thread-safe)
- Command registration happens on GUI thread only (no mutex needed for Phase 0)

**Category indexing:**
- When registering command with category "File", add id to m_categorizedCommands["File"]
- getCommandsByCategory returns all commands in that category

**Error handling:**
- registerCommand with duplicate id: Log warning, replace existing
- getCommand with unknown id: Return nullptr

---

## Acceptance Criteria

- [ ] `command_registry.h` created with singleton interface
- [ ] `command_registry.cpp` implements all methods
- [ ] getInstance() is thread-safe (Meyer's Singleton)
- [ ] registerCommand stores command in both maps
- [ ] unregisterCommand removes from both maps
- [ ] getCommand returns nullptr for unknown id
- [ ] getCategories returns unique category list
- [ ] Code compiles, no warnings

---

## Testing

Manual verification in main():
```cpp
auto& registry = CommandRegistry::getInstance();

Command saveCmd;
saveCmd.id = "file.save";
saveCmd.label = "Save";
saveCmd.category = "File";
registry.registerCommand(saveCmd);

assert(registry.isCommandRegistered("file.save"));
assert(registry.getCommand("file.save") != nullptr);
assert(registry.getCommand("unknown") == nullptr);

auto fileCommands = registry.getCommandsByCategory("File");
assert(fileCommands.size() == 1);
```

---

## Related Files

- `include/kalahari/gui/command_registry.h` (new)
- `src/gui/command_registry.cpp` (new)
- `src/CMakeLists.txt` (add command_registry.cpp)

---

## Next Task

Task #00026 - CommandRegistry Execution + Context

---

**Created:** 2025-11-11
**Author:** Architecture Planning
