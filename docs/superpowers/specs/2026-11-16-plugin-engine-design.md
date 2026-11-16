# Plugin engine (phase 2, "Extension points") – design and state

Status: **stopped, work in progress** (16.11.2026). Phase 2 waits until phase 1 is closed
(the user's decision). Nothing here is compiled or tested yet; the branch only keeps the
work for later.

## What the code on main lacks (checked on 5d31c4ec)

- The program never starts Python or the plugins: `PythonInterpreter::initialize()` and
  `PluginManager::discoverPlugins()` / `loadPlugin()` are called only by tests.
- Only `IExporter` is wired (File > Export lists the plugins' formats,
  `export_book_dialog.cpp:616`); `IPanelProvider`, `ICommandProvider` and `IAssistant` are not.
- Python cannot make a `QWidget` (pybind11 has no caster for it), so a Python panel cannot
  come from `createPanel(QWidget*)`.
- `kalahari_api` is an extension module in `bin/`, which is not on the embedded interpreter's
  `sys.path`.
- Every plugin's entry module is imported by its plain name (`plugin`), so a second plugin
  with `plugin.py` gets the first one's cached module.

## Done on this branch (not compiled)

- `extension_points.h`: the interfaces derive from `IPlugin` virtually, so one object can
  implement several. `ICommandProvider` returns `PluginCommandInfo` (no `gui::Command` in the
  core) and runs commands by id; `IPanelProvider` lists `PluginPanelInfo` and gives either a
  widget or a rich-text page (`panelContent()`, `onPanelLink()`).
- `plugin_host.h/.cpp`: what the program gives the plugins – the book of the window in front,
  the text in front, the book of an export (set while a plugin writes it), messages,
  refreshing a panel, the program's language.
- `plugin_api.h/.cpp`: the module content in one function (`definePluginApi()`), used by the
  extension module (`src/bindings`) and by a module built into the program
  (`plugin_api_embedded.cpp`, `PYBIND11_EMBEDDED_MODULE`, compiled into the program and the
  tests, not into `kalahari_core`, which the extension module loads into a running
  interpreter). New: `Command`, `Panel` (with `refresh()`), `ExportFormat`, `Book`, `Text`,
  `book()`, `front_text()`, `show_message()`, `program_language()`, `program_version()`.
- `python_plugin.h/.cpp`: `PythonPlugin`, one adapter behind every extension point, calling
  the plugin's methods (`get_commands`, `get_panels`, `get_export_formats`,
  `export_document(format, path, book)`, `on_goal_reached`, `show_message`) under the GIL.

## Still to do

Core:
- CMake: the new files in `KALAHARI_CORE_SOURCES`, `plugin_api_embedded.cpp` in the program
  and in `tests/CMakeLists.txt`.
- `PluginManager`: several folders (the program's and `<config dir>/plugins` in the user's
  profile, the user's first), the entry module loaded from its file under a unique name
  (`importlib.util.spec_from_file_location`) and its modules removed from `sys.modules` on
  unload, a `PythonPlugin` registered in `ExtensionPointRegistry` after the load and removed
  before the unload, `loadEnabledPlugins()` (starts Python only when a plugin is to be
  loaded), `unloadAll()`, listeners of the changes (the windows sync their menus and panels).
- Requirements of the add-on manager (`/mnt/project-files/przeglad/menedzer-dodatkow/
  szkic-i-pytania.md`, chapter 4):
  - a remembered enabled/disabled state (`plugins.disabled`, a JSON array), enabling and
    disabling without a restart;
  - installing a `.kplugin` (and its `.sig`) into the profile's folder, replacing an older
    version, removing (only from the profile's folder);
  - what each plugin added (`PluginContributions`: commands, panels, export formats, the
    assistant hook; book types come later), its state and error;
  - settings of a plugin, declared (`kalahari_api.Setting(id, label, default, ...)` from
    `get_settings()`, the value through `setting.value`, stored in `plugins.settings` as
    `{plugin id: {setting id: value}}`), the page in the Settings window with the manager;
  - a place for the licence check before a plugin is enabled
    (`PluginManager::setLicenseCheck()`, allowing all until the decisions on paid add-ons).

Program (`gui`):
- `PluginCoordinator` per window: the plugins' commands (`plugin.<plugin id>.<command id>`)
  added to the menus at run time (default Tools > Plugins > the plugin's name; a menu of the
  program by its technical path), icons (a program icon or the plugin's SVG through
  `IconRegistry`), keys checked by `ShortcutRules` and against the other commands, greyed out
  without a book (`needsBook`), the plugin's error in a `MessageDialog`; the panels as docks on
  the right, closed until opened, with a toggle in View > Panels and a tab in `PanelTabs`, a
  `QTextBrowser` page in the theme's colours (web links open in the browser, the others go to
  `onPanelLink()`).
- `installPluginHost()`: the book from the front window's `BookExportCoordinator::bookExport()`
  (all texts), the text in front, messages in `MessageDialog::information()`, panel refreshes
  in every window; `PluginHost::ExportScope` around `exportWithPlugin()`.
- `WritingAwardsTracker::dailyGoalReached` → `IAssistant::onGoalReached()` (once a day, not
  once per window).
- `main.cpp`: `loadEnabledPlugins()` before the first window (not in the benchmark), and
  `unloadAll()` after the windows are deleted.
- Python calls stay on the main thread, which keeps the GIL (as in the tests).

Tests, sample plugin, docs:
- Core: a `.kplugin` written by the test (libzip) with commands, a panel, a format and a
  goal hook; loading, running, errors, the disabled list, two plugins with `plugin.py`,
  install/replace/remove.
- Program: the commands and panels of a C++ fake plugin in a window's menus and docks.
- A sample plugin (lengths of the chapters in a panel, a command, a plain-text export) and
  `docs/plugin_api_reference.md` rewritten for the new API.
