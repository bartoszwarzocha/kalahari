# Task #00026: CommandRegistry Execution + Context

**Status:** 📋 Planned
**Priority:** P1 (HIGH - Architecture Foundation)
**Estimated:** 75 minutes
**Dependencies:** #00025 (CommandRegistry Singleton)
**Phase:** Phase 0 - Architecture

---

## Goal

Implement command execution, context checking (enabled/checked state), and execution error handling in CommandRegistry.

---

## Requirements

### 1. Extend `include/kalahari/gui/command_registry.h`

Add execution methods:

```cpp
class CommandRegistry {
public:
    // ... existing methods ...

    // Execution
    bool executeCommand(const std::string& id);
    bool canExecute(const std::string& id) const;
    bool isChecked(const std::string& id) const;

    // Error handling
    void setErrorHandler(std::function<void(const std::string&, const std::exception&)> handler);

private:
    std::function<void(const std::string&, const std::exception&)> m_errorHandler;
};
```

### 2. Implement in `src/gui/command_registry.cpp`

**executeCommand(id):**
- Check if command exists (return false if not)
- Check if command is enabled via isEnabled() (return false if disabled)
- Call command.execute() inside try-catch block
- On exception: call m_errorHandler if set, log error, return false
- Return true on success

**canExecute(id):**
- Check if command exists
- If command.isEnabled is set, call and return result
- Otherwise return true (enabled by default)

**isChecked(id):**
- Check if command exists
- If command.isChecked is set, call and return result
- Otherwise return false (not checked by default)

**setErrorHandler(handler):**
- Store handler for command execution errors
- Default behavior (if not set): log to spdlog as error

---

## Implementation Notes

**Error handling pattern:**
```cpp
try {
    cmd.execute();
    return true;
} catch (const std::exception& e) {
    if (m_errorHandler) {
        m_errorHandler(id, e);
    } else {
        KALAHARI_ERROR("Command '{}' failed: {}", id, e.what());
    }
    return false;
}
```

**Thread safety:**
- All methods are const or lock-free (read-only access)
- Command lambdas are responsible for their own thread safety

---

## Acceptance Criteria

- [ ] executeCommand returns false for unknown command
- [ ] executeCommand returns false if isEnabled() returns false
- [ ] executeCommand calls command.execute() and returns true on success
- [ ] executeCommand catches exceptions and calls error handler
- [ ] canExecute returns result of isEnabled() callback
- [ ] isChecked returns result of isChecked() callback
- [ ] setErrorHandler stores custom error handler
- [ ] Default error handler logs to spdlog
- [ ] Code compiles, no warnings

---

## Testing

Manual verification:
```cpp
auto& registry = CommandRegistry::getInstance();

Command testCmd;
testCmd.id = "test.command";
testCmd.execute = []() { throw std::runtime_error("Test error"); };
testCmd.isEnabled = []() { return false; };

registry.registerCommand(testCmd);

// Should return false (command disabled)
assert(!registry.canExecute("test.command"));

// Should return false (execution blocked by isEnabled)
assert(!registry.executeCommand("test.command"));

// Enable and test exception handling
testCmd.isEnabled = []() { return true; };
registry.registerCommand(testCmd);

bool errorHandled = false;
registry.setErrorHandler([&](const std::string& id, const std::exception& e) {
    errorHandled = true;
    assert(id == "test.command");
});

assert(!registry.executeCommand("test.command")); // Should fail with exception
assert(errorHandled);
```

---

## Related Files

- `include/kalahari/gui/command_registry.h` (extend)
- `src/gui/command_registry.cpp` (extend)

---

## Next Task

Task #00027 - Keyboard Shortcut Management

---

**Created:** 2025-11-11
**Author:** Architecture Planning
