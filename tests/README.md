# Kalahari Test Suite

This directory contains all unit tests, integration tests, and smoke tests for Kalahari Writer's IDE using **Catch2 v3** (BDD style).

## Test Structure

```
tests/
├── core/           # Core/Model layer tests (unit tests)
├── gui/            # GUI/View layer tests (integration tests)
├── presenters/     # Presenter layer tests (unit tests with mocks)
├── services/       # Service layer tests (integration tests)
└── test_main.cpp   # Catch2 main entry point (DO NOT MODIFY)
```

## Running Tests

```bash
# Build tests
cd build
cmake ..
make kalahari-tests

# Run all tests
ctest

# Or run directly
./bin/kalahari-tests

# Run specific tests
./bin/kalahari-tests "[tag-name]"
./bin/kalahari-tests "Smoke test"

# Verbose output
./bin/kalahari-tests -s
```

## Writing Tests

### Catch2 BDD Style

```cpp
#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/model/document.h>

using namespace kalahari::core::model;

SCENARIO("Document can be created and modified", "[document]")
{
    GIVEN("A new document")
    {
        Document doc("Test Document");

        WHEN("The title is changed")
        {
            doc.setTitle("New Title");

            THEN("The new title is returned")
            {
                REQUIRE(doc.getTitle() == "New Title");
            }

            AND_THEN("The document is marked as modified")
            {
                REQUIRE(doc.isModified() == true);
            }
        }
    }
}
```

### Tags

Use tags to categorize tests:

- `[smoke]` - Smoke tests (quick sanity checks, run first)
- `[unit]` - Unit tests (single class/function)
- `[integration]` - Integration tests (multiple components)
- `[gui]` - GUI tests (require wxWidgets)
- `[slow]` - Slow tests (> 1 second)
- `[model]`, `[view]`, `[presenter]` - Architecture layer
- `[plugin]`, `[event]`, `[command]` - Feature-specific

**Example:**
```cpp
TEST_CASE("Quick sanity check", "[smoke][unit]") { ... }
TEST_CASE("Full workflow test", "[integration][slow]") { ... }
```

## Test Coverage

**Target:** 70%+ code coverage (Phase 5)

**Priority:**
1. **Core/Model:** 90%+ (business logic, critical)
2. **Presenters:** 80%+ (coordination logic)
3. **Services:** 70%+ (infrastructure)
4. **GUI:** 50%+ (integration tests only)

## CI/CD Integration

Tests run automatically on every commit via GitHub Actions:
- **Linux:** GCC + Clang
- **Windows:** MSVC
- **macOS:** Clang

See `.github/workflows/ci-*.yml` for configuration.

## Mocking

For testing Presenters without real Views, use test doubles:

```cpp
class MockDocumentView : public IDocumentView
{
public:
    void displayContent(const std::string& content) override
    {
        m_lastDisplayed = content;
    }

    std::string getLastDisplayed() const { return m_lastDisplayed; }

private:
    std::string m_lastDisplayed;
};
```

## Related Documentation

- **Architecture:** `project_docs/03_architecture.md` (Testing section)
- **Catch2 Docs:** https://github.com/catchorg/Catch2/tree/devel/docs
- **Testing Strategy:** `project_docs/testing_strategy.md` (Phase 2+)

---

**Last Updated:** 2025-10-26
**Phase:** 0 (Foundation)
