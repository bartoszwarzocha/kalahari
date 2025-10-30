# Task #00011: Decyzje i Ustalenia

**Data:** 2025-10-29 (wieczór)
**Status:** Zatwierdzone przez użytkownika, implementacja jutro rano

---

## ✅ Zatwierdzone Decyzje (Q1-Q3)

### Q1: Lokalizacja tymczasowych pluginów
**Decyzja:** Opcja A - `~/.local/share/Kalahari/plugins/temp/`

**Uzasadnienie:**
- Standard XDG Base Directory (Linux/macOS)
- Trwałe dane aplikacji (nie czyszczone automatycznie)
- Backup-friendly dla użytkowników
- Cross-platform implementation:
  - Linux: `~/.local/share/Kalahari/plugins/temp/`
  - macOS: `~/Library/Application Support/Kalahari/plugins/temp/`
  - Windows: `%LOCALAPPDATA%/Kalahari/plugins/temp/`

**Implementacja:**
```cpp
std::filesystem::path getPluginTempDir() {
    #ifdef __linux__
        const char* xdg = std::getenv("XDG_DATA_HOME");
        if (xdg) return std::filesystem::path(xdg) / "Kalahari/plugins/temp";
        return std::filesystem::path(std::getenv("HOME")) / ".local/share/Kalahari/plugins/temp";
    #elif __APPLE__
        return std::filesystem::path(std::getenv("HOME")) / "Library/Application Support/Kalahari/plugins/temp";
    #elif _WIN32
        return std::filesystem::path(std::getenv("LOCALAPPDATA")) / "Kalahari/plugins/temp";
    #endif
}
```

---

### Q2: Strategia ładowania pluginów
**Decyzja:** Opcja A - Auto-discovery przy starcie + lazy-load na żądanie

**Uzasadnienie:**
- Szybki start aplikacji (~0.5s zamiast 5-15s)
- Oszczędność pamięci (nie ładuje nieużywanych pluginów)
- Izolowane błędy (plugin crash nie blokuje startu)
- Standard branżowy (VSCode, Sublime, Chrome extensions)

**Workflow:**
```cpp
// 1. Przy starcie aplikacji (main.cpp):
PluginManager::getInstance().discoverPlugins();  // Szybkie: tylko manifest.json parsing

// 2. Gdy użytkownik włącza plugin (Settings → Plugins):
PluginManager::getInstance().loadPlugin("org.kalahari.exporter.docx");  // Leniwie: extract + import

// 3. Gdy aplikacja potrzebuje funkcjonalności:
auto exporter = PluginManager::getInstance().getExporter("docx");  // Auto-load if needed
```

**Sekwencja:**
1. Discovery: Skanuje `plugins/*.kplugin`, parsuje manifest.json, przechowuje metadane
2. Loading (lazy): Ekstraktuje ZIP, dodaje do sys.path, importuje moduł, wywołuje on_init/on_activate
3. Unloading: Wywołuje on_deactivate, czyści temp files, usuwa z sys.path

---

### Q3: Zakres testowania
**Decyzja:** Opcja B (minimal) → Opcja A (full) - iteracyjnie

**Plan:**
- **Week 6 (Task #00011):** Minimal testing (1h)
  - 1 sample plugin: `hello_plugin.kplugin`
  - 4 basic C++ tests: discovery, loading, unloading, error handling
  - 1 Python integration test: lifecycle check

- **Phase 1 (Weeks 9-20):** Full testing (3h)
  - Podczas pisania MVP pluginów (Lion, Meerkat, Elephant, Cheetah)
  - Rozbudowa testów o edge cases
  - Performance tests
  - Security tests (malicious manifest)

**Uzasadnienie:**
- MVP mindset: najpierw działająca funkcjonalność
- Time-to-value: szybciej dostarczenie plugin system
- Iteracja: basic tests teraz, refinement przy MVP plugins

---

## 📋 Task #00011 - Implementation Plan

**Obiekt:** .kplugin Format Handler + Actual Plugin Loading
**Źródło:** `tasks/00011_kplugin_format_handler.md` (440 linii)
**Szacowany czas:** 12-15 godzin
**Priorytet:** 🟠 HIGH (Required for Phase 1 MVP)

### Fazy Implementacji

**Phase 1: Enhanced Plugin Discovery (2-3h)**
- PluginMetadata struct
- discoverPlugins() - scan plugins/ directory
- libzip detection of .kplugin files
- Manifest.json parsing

**Phase 2: ZIP Extraction (2-3h)**
- PluginArchive class (RAII wrapper for libzip)
- Extract to temp directory
- Cleanup on destruction

**Phase 3: Plugin Loading (3-4h)**
- loadPlugin(id) implementation
- Python sys.path management
- Dynamic module import
- on_init() + on_activate() calls
- Error handling (isolated failures)

**Phase 4: Plugin Instance Management (2h)**
- PluginInstance storage (map: id → instance)
- Lifecycle states (Discovered, Loaded, Activated, Error)
- ExtensionPointRegistry integration

**Phase 5: Unloading & Cleanup (1-2h)**
- unloadPlugin(id) implementation
- on_deactivate() calls
- Temp file cleanup
- sys.path removal

**Phase 6: Testing (1h - minimal)**
- Sample hello_plugin.kplugin
- 4 C++ test cases
- 1 Python integration test

**Phase 7: Integration & Documentation (1-2h)**
- Build verification
- docs/plugin_development_guide.md
- CHANGELOG.md update

### Nowe Pliki (7)

1. `include/kalahari/core/plugin_manifest.h/cpp` - Manifest parsing
2. `include/kalahari/core/plugin_archive.h/cpp` - ZIP extraction
3. `tests/plugins/hello_plugin.kplugin` - Sample plugin
4. `tests/core/test_plugin_loading.cpp` - C++ tests
5. `docs/plugin_development_guide.md` - Developer docs

### Modyfikowane Pliki (4)

1. `include/kalahari/core/plugin_manager.h` - Enhanced interface
2. `src/core/plugin_manager.cpp` - Full implementation
3. `src/CMakeLists.txt` - Already links libzip ✅
4. `CHANGELOG.md` - Task completion notes

---

## 🎯 Acceptance Criteria

✅ **Discovery:**
- Scans plugins/ directory
- Finds .kplugin files
- Parses manifest.json
- Validates metadata

✅ **Loading:**
- Extracts .kplugin to temp dir
- Imports Python module
- Calls on_init() and on_activate()
- Registers with ExtensionPointRegistry

✅ **Unloading:**
- Calls on_deactivate()
- Cleans up temp files
- Removes from sys.path

✅ **Error Handling:**
- Plugin failures don't crash app
- Errors logged appropriately
- Plugin marked as broken

✅ **Testing:**
- 4 C++ unit tests pass
- 1 Python integration test passes
- Build succeeds on all 3 platforms

---

## 📊 Context from Session

**Task #00010 Status:** ✅ COMPLETED
- Extension Points + Event Bus implemented
- 23 test cases (all pass)
- CI/CD: 6/6 builds SUCCESS (Linux, macOS, Windows - Debug + Release)
- Commits: 69e35ea (feature), 667d114 + 30059d6 + 6df7a1b (CI/CD fixes)

**Current State:**
- Plugin infrastructure ready (Task #00009 + #00010)
- libzip already in dependencies ✅
- pybind11 bindings working ✅
- Ready to implement actual plugin loading

**Quality Focus:**
- Thread-safe patterns
- Comprehensive error handling
- Full test coverage
- Cross-platform CI/CD

---

## 🚀 Next Steps (Jutro Rano)

1. **Rozpoczęcie implementacji Task #00011**
   - Start with Phase 1: Plugin Discovery
   - Follow plan z tasks/00011_kplugin_format_handler.md
   - Use approved decisions Q1-Q3

2. **Approach:**
   - Używaj Serena MCP do eksploracji kodu
   - Używaj context7 dla libzip documentation
   - Incremental commits (po każdej fazie)
   - Test after each phase

3. **Expected Duration:** 12-15h (1-2 dni pracy)

---

**Last Updated:** 2025-10-29 22:00 UTC
**Approved By:** User (Bartosz)
**Status:** Ready for implementation tomorrow morning
