# Plugin engine (phase 2, "Extension points") – design and state

Status: **implemented on this branch** (16.11.2026): the engine, its wiring into the book
windows and its tests. The add-on manager window and the plugins' page in the Settings window
are not part of it; they wait for the manager's mockup
(`/mnt/project-files/przeglad/menedzer-dodatkow/szkic-i-pytania.md`).

## The user's decisions it follows

- **Only Kalahari's own add-ons** (16.11.2026, answer 11 of the paid add-ons talk): the
  program loads no add-ons of other publishers and offers no way to make them. So:
  - every plugin is signed with Kalahari's key; an unsigned plugin, one signed with another
    key or one changed after signing is not loaded;
  - the keys are built into the program (`trusted_keys.cpp`); there are no user keys, no
    trust levels and no settings `plugins.trustedKeys` or `plugins.allowUnsigned` any more;
  - there is no public description of the API for plugin authors; this document and the
    comments of the code are the internal description.
- **Paid add-ons** (answers 3 and 5): a licence with the buyer's name and e-mail, checked
  offline, possibly a package the server makes for each buyer. The engine has the place for
  the check (`PluginManager::setLicenseCheck()`); its form waits for the manager's mockup.

## What the program lacked on main (5d31c4ec)

- Python and the plugins were started only by tests.
- Only `IExporter` was wired (File > Export); `IPanelProvider`, `ICommandProvider` and
  `IAssistant` were not.
- Python cannot make a `QWidget`, so a Python panel could not come from `createPanel()`.
- `kalahari_api` was an extension module in `bin/`, not on the embedded interpreter's path.
- Every plugin's entry module was imported by its plain name, so a second `plugin.py` got the
  first one's cached module.

## Design

Core (`kalahari_core`):

- `extension_points.h`: the interfaces derive from `IPlugin` virtually, so one object can
  implement several. `ICommandProvider` lists `PluginCommandInfo` and runs commands by id;
  `IPanelProvider` lists `PluginPanelInfo` and gives a widget (C++) or a rich-text page
  (`panelContent()`, `onPanelLink()`).
- `plugin_host.h`: what the program gives the plugins – the book of the window in front, the
  text in front, the book of an export (`ExportScope`, set while a plugin writes it),
  messages, refreshing a panel, the program's language. The program fills it
  (`gui::installPluginHost()`).
- `plugin_api.h`: the content of the `kalahari_api` module in one function,
  `definePluginApi()`, used by the extension module (`src/bindings`) and by a module built
  into the program (`plugin_api_embedded.cpp`, compiled into the program and the tests, not
  into `kalahari_core`, which the extension module loads into a running interpreter). It
  takes the module as a `PyObject*`: a function with a pybind11 type among its parameters is
  hidden on Linux. Classes: `Command`, `Panel` (with `refresh()`), `ExportFormat`, `Setting`,
  `Book`, `Text`; functions `book()`, `front_text()`, `show_message()`, `program_language()`,
  `program_version()`, `API_VERSION` ("1.0.0").
- `plugin_settings.h`: the settings a plugin declares (`PluginSettingInfo`: a bool, int, float
  or str default, optional choices and limits) and their values in `plugins.settings`
  (`{plugin id: {setting id: value}}`); a value of another kind, outside the limits or not a
  choice is not taken, and reads as the default when the file has it.
- `python_plugin.h`: `PythonPlugin`, one adapter behind every extension point. The plugin's
  class needs no base class; each extension point is a method it may have (`get_commands`,
  `get_panels`, `get_export_formats` + `export_document(format, path, book)`,
  `on_goal_reached`, `show_message`, `get_settings`, `on_settings_changed`). Every call takes
  the GIL; an exception of the plugin becomes a `std::runtime_error` with its words.
- `PluginManager`: several folders (the user's `<config dir>/plugins` first, then the
  program's), the entry module imported from its file under a unique name
  (`kalahari_plugin_<id>`) and its modules removed from `sys.modules` on unload, a
  `PythonPlugin` registered in `ExtensionPointRegistry` after the load and removed before the
  unload, `loadEnabledPlugins()` (Python starts only when a plugin is to be loaded),
  `unloadAll()`, `plugins.disabled` with `setPluginEnabled()` (no restart), `installPlugin()`
  (into the user's folder, in place of an older version), `removePlugin()` (only from the
  user's folder, optionally with its settings), `contributionsOf()` (what a plugin added),
  `setLicenseCheck()`, change listeners.
- `TrustedKeys`: the keys built into the program; `addTestKey()` and `clear()` for the tests.

Program (`gui`):

- `PluginCoordinator` per book window: the plugins' commands (`plugin.<plugin id>.<command
  id>`) in the menus at run time – Tools > Plugins > the plugin's name by default, or a menu of
  the program by its technical path ("TOOLS", "TOOLS/Statistics"); icons (a program icon, the
  plugin's SVG, or the icon of the plugins) through `IconRegistry`; keys checked by
  `ShortcutRules` and against the other commands; greyed out without a book (`needsBook`);
  the plugin's error in a `MessageDialog`. The panels are docks on the right, closed until
  opened, with a toggle in View > Panels (`plugin-panel.<plugin id>.<panel id>`) and a tab in
  `PanelTabs`; the page is a `QTextBrowser` (web links open in the browser, the others go to
  `onPanelLink()`), read when it is shown and when the plugin asks. The window follows the
  plugins turned on and off (`PluginManager` listeners, queued).
- `CommandRegistry::unregisterCommand()` also removes the command's action from the menus.
- `installPluginHost()`: the book from the front window's
  `BookExportCoordinator::bookExport()`, the text in front, messages in
  `MessageDialog::information()`, panel refreshes in every window; `PluginHost::ExportScope`
  around a plugin's export with what the writer chose in the export window.
- `WritingAwardsTracker::dailyGoalReached` → `IAssistant::onGoalReached()`, once a day for all
  windows (`tellPluginsGoalReached()`).
- `main.cpp`: the built-in keys and `loadEnabledPlugins()` before the first window (not in the
  benchmark), `unloadAll()` after the windows are deleted.
- Python calls stay on the main thread, which keeps the GIL.

Tests:

- `tests/test_support/plugin_signing.h`: the tests make an Ed25519 key per run, trust it and
  sign their plugins with it.
- `tests/core/test_plugin_engine.cpp`: a test-made `.kplugin` with commands, a panel, a
  format, settings and a goal hook; running them, errors, settings and their limits, the
  disabled list, two plugins with `plugin.py`, unsigned and wrongly signed plugins, the
  licence check, install/replace/remove.
- `tests/gui/test_plugin_coordinator.cpp`: a C++ plugin's commands and panel in a window's
  menus and docks, keys, greying, removal and return, the book as the plugins see it.

## Still to do (other tasks)

- Kalahari's signing key: the private half stays with the publisher, the public half goes into
  `trusted_keys.cpp`; until then no plugin loads. A signing script may come with it.
- The add-on manager window (list, state and error of each add-on, what it added, turning it
  on and off, installing and removing) and the plugins' page in the Settings window (the
  declared settings), after the manager's mockup.
- The form of the licences of paid add-ons, then a `LicenseCheck` that reads them.
- Book types from plugins (an extension point of their own).
- The old public documents for plugin authors (`docs/plugin_api_reference.md`,
  `docs/plugin_development_guide.md`) are out of date; whether they go is the user's call.
