# Kalahari Test Suite

Unit and integration tests for Kalahari, written with **Catch2 v3**, plus two
Python scripts that test the `kalahari_api` plugin bindings.

## Layout

```
tests/
├── core/            # core:: classes (settings, documents, plugins, Python, EventBus)
├── editor/          # editor:: classes (KML reading and saving, layout, search, services, BookEditor)
├── gui/             # gui:: classes without a window (CommandRegistry)
├── benchmarks/      # benchmark helpers (not part of the default run)
├── sanitizers/      # LeakSanitizer suppressions used by CI
├── test_support/    # shared test helpers (resetSingletons)
├── test_main.cpp    # custom main: QApplication, temp dir, singleton reset
└── test_*.py        # Python binding tests (system-package builds only)
```

`tests/CMakeLists.txt` builds a single `kalahari-tests` executable. Production
sources under test are listed there explicitly; a new test file must be added to
`TEST_SOURCES`, and a production source it needs to the source list below it.

## Running tests

Build with the platform script (`scripts/build_windows.bat Debug`,
`scripts/build_linux.sh`), then from the build directory:

```bash
ctest --output-on-failure --parallel 4    # all tests, each Catch2 test case is a separate CTest test
ctest -R "KML load"                       # tests whose name matches a regex
ctest -E '^python\.'                      # everything except the Python scripts

./bin/kalahari-tests "[kml]"              # Catch2 tag filter
./bin/kalahari-tests "Test case name" -s  # one test case, verbose
./bin/kalahari-tests --order rand         # random order
```

Hidden tests (tags starting with `.`) are not run by default; select them explicitly,
e.g. `./bin/kalahari-tests "[.dll-boundary]"`.

## Test environment

`test_main.cpp` prepares every test process:

- On Linux, `QT_QPA_PLATFORM=offscreen` unless set by the caller, so tests never need a
  display. Windows and macOS use their native platform plugin.
- A private temporary directory per process: `TMPDIR` (and `XDG_DATA_HOME` on Linux)
  or `TMP`/`TEMP` on Windows point to it, so settings, archives and databases written
  by one test process are invisible to the others. This is what makes parallel
  `ctest` runs safe. The directory is removed at exit.
- `KALAHARI_TEST_MODE=1`, so `SettingsManager` uses the temporary directory.
- `kalahari::test::resetSingletons()` before every test case
  (`test_support/reset_singletons.cpp`).

Tests must not depend on execution order or on files left by other tests.

## Writing tests

```cpp
#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>

TEST_CASE("KML load: one paragraph", "[editor][kml][load]") {
    kalahari::editor::BookEditor editor;
    editor.fromKml(QStringLiteral("<p>Text</p>"));
    REQUIRE(editor.plainText() == QStringLiteral("Text"));
}
```

- Tag every test case with its module (`[core]`, `[editor]`, `[gui]`) and class.
- Use `INFO(...)` to attach context that is printed when an assertion fails.
- Asynchronous Qt work (queued connections, `QMetaObject::invokeMethod`) needs
  `QCoreApplication::processEvents()` before the assertion.

## Continuous integration

`.github/workflows/`:

- **Linux** (`ci-linux.yml`): Release build and all tests; a second job builds Debug
  with AddressSanitizer + UndefinedBehaviorSanitizer and coverage, runs the tests
  (leaks are checked, `sanitizers/lsan.supp` lists the known Python/pybind11 ones),
  publishes a gcovr HTML report as the `coverage-report` artifact and, on pull
  requests, runs clang-tidy (`.clang-tidy`) on the changed lines.
- **Windows** (`ci-windows.yml`): Debug and Release builds with vcpkg and all tests.
- **macOS** (`ci-macos.yml`): Debug and Release builds with vcpkg and all tests.
