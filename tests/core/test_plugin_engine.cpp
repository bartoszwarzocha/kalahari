// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_plugin_engine.cpp
/// @brief Tests of the plugin engine: Python plugins behind the extension points, their
///        settings, turning them on and off, installing and removing them

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <kalahari/core/extension_points.h>
#include <kalahari/core/plugin_host.h>
#include <kalahari/core/plugin_manager.h>
#include <kalahari/core/plugin_settings.h>
#include <kalahari/core/python_interpreter.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/core/unicode_paths.h>

#include "test_support/plugin_signing.h"

#include <QTemporaryDir>

#include <nlohmann/json.hpp>
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <zip.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

using namespace kalahari::core;
namespace fs = std::filesystem;
namespace py = pybind11;
using nlohmann::json;
using Catch::Matchers::ContainsSubstring;

namespace {

/// A plugin with every extension point
constexpr const char* FULL_PLUGIN = R"PY(
import kalahari_api as k


class Full:
    def __init__(self):
        self.ran = 0
        self.links = []
        self.goals = 0
        self.changed = 0
        self.size = k.Setting("size", "Size", 3, minimum=1, maximum=10)
        self.mode = k.Setting("mode", "Mode", "short",
                              choices=[("short", "Short"), ("long", "Long")])

    def on_init(self):
        pass

    def on_activate(self):
        self.size_at_start = self.size.value

    def run(self):
        self.ran += 1

    def fail(self):
        raise RuntimeError("no luck")

    def get_commands(self):
        return [
            k.Command("count", "&Count", self.run, tooltip="Counts", shortcut="Ctrl+Alt+K",
                      needs_book=True, icon="icon.svg"),
            k.Command("fail", "Fail", self.fail, icon="../outside.svg"),
            k.Command("count", "Twice", self.run),
            "not a command",
        ]

    def get_panels(self):
        return [k.Panel("lengths", "Lengths", self.page, on_link=self.link)]

    def page(self):
        book = k.book()
        if book is None:
            return "<p>No book</p>"
        return "<p>%s: %d</p>" % (book.title, len(book.texts))

    def link(self, address):
        self.links.append(address)

    def get_export_formats(self):
        return [k.ExportFormat("txt", "Plain text", "txt")]

    def export_document(self, format, path, book):
        # Lines end with \n on every system (text mode writes \r\n on Windows)
        with open(path, "w", encoding="utf-8", newline="\n") as file:
            file.write(format + "\n")
            for text in book.texts:
                file.write(text.title + ":" + text.text + "\n")
        return True

    def on_goal_reached(self):
        self.goals += 1

    def get_settings(self):
        return [self.size, self.mode]

    def on_settings_changed(self):
        self.changed += 1
)PY";

constexpr const char* FULL_ID = "org.kalahari.test.full";

/// The manifest of a plugin
std::string manifestOf(const std::string& id, const std::string& version,
                       const std::string& entryPoint) {
    return json{{"id", id},
                {"name", "Plugin " + id},
                {"version", version},
                {"api_version", "1.0.0"},
                {"entry_point", entryPoint}}
        .dump();
}

/// A plugin file at @p path with @p files (name, contents) and a manifest, not signed
void writePlugin(const fs::path& path, const std::string& id, const std::string& version,
                 const std::string& entryPoint,
                 const std::vector<std::pair<std::string, std::string>>& files) {
    int error = 0;
    zip_t* archive = openZipFile(path, ZIP_CREATE | ZIP_TRUNCATE, &error);
    REQUIRE(archive != nullptr);
    std::vector<std::pair<std::string, std::string>> entries = files;
    entries.emplace_back("manifest.json", manifestOf(id, version, entryPoint));
    // libzip reads the contents when the archive is closed: entries keeps them until then
    for (const auto& [name, contents] : entries) {
        zip_source_t* source = zip_source_buffer(archive, contents.data(), contents.size(), 0);
        REQUIRE(zip_file_add(archive, name.c_str(), source, ZIP_FL_ENC_UTF_8) >= 0);
    }
    REQUIRE(zip_close(archive) == 0);
}

/// The full plugin, signed, at @p path
void writeFullPlugin(const fs::path& path, const std::string& version = "1.0.0") {
    writePlugin(path, FULL_ID, version, "plugin:Full",
                {{"plugin.py", FULL_PLUGIN}, {"icon.svg", "<svg/>"}});
    kalahari::test::signPlugin(path);
}

fs::path pathOf(const QTemporaryDir& dir) {
    return pathOfText(dir.path().toStdString());
}

std::string textOf(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

/// An attribute of the object of a loaded plugin, as a C++ value
template <typename T>
T attributeOf(const std::string& pluginId, const char* name) {
    const PluginInstance* instance = PluginManager::getInstance().getPluginInstance(pluginId);
    REQUIRE(instance != nullptr);
    REQUIRE(instance->instance != nullptr);
    py::gil_scoped_acquire gil;
    return instance->instance->attr(name).cast<T>();
}

/// Run Python code with the object of a loaded plugin as `plugin`
void runWithPlugin(const std::string& pluginId, const char* code) {
    const PluginInstance* instance = PluginManager::getInstance().getPluginInstance(pluginId);
    REQUIRE(instance != nullptr);
    py::gil_scoped_acquire gil;
    py::dict scope;
    scope["plugin"] = *instance->instance;
    py::exec(code, scope);
}

/// The plugins of a temporary folder (signed with the tests' key), with Python started; the
/// plugins are unloaded at the end
class PluginFolder {
public:
    PluginFolder() {
        REQUIRE(m_dir.isValid());
        kalahari::test::trustTestKey();
        PluginManager::getInstance().setPluginFolders({folder()});
        PythonInterpreter::getInstance().initialize();
    }

    ~PluginFolder() {
        auto& manager = PluginManager::getInstance();
        manager.unloadAll();
        manager.setLicenseCheck(nullptr);
        manager.setPluginFolders({});
        manager.discoverPlugins();
        PluginHost::getInstance().reset();
    }

    PluginFolder(const PluginFolder&) = delete;
    PluginFolder& operator=(const PluginFolder&) = delete;

    [[nodiscard]] fs::path folder() const { return pathOf(m_dir); }

private:
    QTemporaryDir m_dir;
};

/// A book of one text for the plugins
PluginBook oneTextBook() {
    PluginBook book;
    book.title = "Book";
    book.items.push_back({"part", "p1", "Part One", "", ""});
    book.items.push_back({"text", "t1", "One", "abc", "<kml/>"});
    return book;
}

}  // namespace

TEST_CASE("Plugin engine: a signed plugin gives its commands, panels, formats and settings",
          "[plugin][engine]") {
    PluginFolder plugins;
    writeFullPlugin(plugins.folder() / "full.kplugin");
    auto& manager = PluginManager::getInstance();
    REQUIRE(manager.loadEnabledPlugins() == 1);
    REQUIRE(manager.isPluginLoaded(FULL_ID));

    const PluginContributions added = manager.contributionsOf(FULL_ID);
    // The command twice and the one that is not a command are left out
    REQUIRE(added.commands.size() == 2);
    CHECK(added.commands[0].id == "count");
    CHECK(added.commands[0].label == "&Count");
    CHECK(added.commands[0].tooltip == "Counts");
    CHECK(added.commands[0].shortcut == "Ctrl+Alt+K");
    CHECK(added.commands[0].needsBook);
    // An icon of the plugin by its path in the plugin's folder, none outside it
    const PluginInstance* instance = manager.getPluginInstance(FULL_ID);
    REQUIRE(instance != nullptr);
    CHECK(fs::path(added.commands[0].icon) ==
          (instance->archive->getExtractedPath() / "icon.svg").lexically_normal());
    CHECK(added.commands[1].icon.empty());
    REQUIRE(added.panels.size() == 1);
    CHECK(added.panels[0].id == "lengths");
    CHECK(added.panels[0].title == "Lengths");
    REQUIRE(added.exportFormats.size() == 1);
    CHECK(added.exportFormats[0].name == "Plain text");
    REQUIRE(added.settings.size() == 2);
    CHECK(added.settings[0].defaultValue == 3);
    CHECK(added.settings[1].choices.size() == 2);
    CHECK(added.assistant);

    // The plugin is behind every extension point
    auto& registry = ExtensionPointRegistry::getInstance();
    CHECK(registry.getPluginAs<ICommandProvider>(FULL_ID) != nullptr);
    CHECK(registry.getPluginAs<IPanelProvider>(FULL_ID) != nullptr);
    CHECK(registry.getPluginAs<IExporter>(FULL_ID) != nullptr);
    CHECK(registry.getPluginAs<IAssistant>(FULL_ID) != nullptr);

    manager.unloadPlugin(FULL_ID);
    CHECK(registry.getPlugin(FULL_ID) == nullptr);
    CHECK(manager.contributionsOf(FULL_ID).commands.empty());
}

TEST_CASE("Plugin engine: the commands, panel, export and goal of a plugin run in Python",
          "[plugin][engine]") {
    PluginFolder plugins;
    writeFullPlugin(plugins.folder() / "full.kplugin");
    auto& manager = PluginManager::getInstance();
    REQUIRE(manager.loadEnabledPlugins() == 1);
    auto& registry = ExtensionPointRegistry::getInstance();

    SECTION("A command runs, and the plugin's exception becomes an error with its words") {
        const auto commands = registry.getPluginAs<ICommandProvider>(FULL_ID);
        REQUIRE(commands != nullptr);
        commands->runCommand("count");
        commands->runCommand("count");
        CHECK(attributeOf<int>(FULL_ID, "ran") == 2);
        CHECK_THROWS_WITH(commands->runCommand("fail"), ContainsSubstring("no luck"));
        CHECK_THROWS_WITH(commands->runCommand("none"), ContainsSubstring("no command 'none'"));
    }

    SECTION("A panel reads the book of the window in front, and hears of its links") {
        const auto panels = registry.getPluginAs<IPanelProvider>(FULL_ID);
        REQUIRE(panels != nullptr);
        CHECK(panels->panelContent("lengths") == "<p>No book</p>");
        PluginHost::getInstance().setBookSource([]() { return oneTextBook(); });
        CHECK(panels->panelContent("lengths") == "<p>Book: 1</p>");
        panels->onPanelLink("lengths", "chapter:t1");
        CHECK(attributeOf<std::vector<std::string>>(FULL_ID, "links") ==
              std::vector<std::string>{"chapter:t1"});
        CHECK_THROWS(panels->panelContent("none"));
    }

    SECTION("An export writes the book the writer chose") {
        const auto exporter = registry.getPluginAs<IExporter>(FULL_ID);
        REQUIRE(exporter != nullptr);
        const fs::path file = plugins.folder() / "book.txt";
        {
            const PluginHost::ExportScope scope(oneTextBook());
            REQUIRE(exporter->exportDocument("txt", pathText(file)));
        }
        CHECK(textOf(file) == "txt\nOne:abc\n");
        CHECK(PluginHost::getInstance().exportBook() == nullptr);
    }

    SECTION("The daily goal reaches the plugin") {
        const auto assistant = registry.getPluginAs<IAssistant>(FULL_ID);
        REQUIRE(assistant != nullptr);
        assistant->onGoalReached();
        CHECK(attributeOf<int>(FULL_ID, "goals") == 1);
    }
}

TEST_CASE("Plugin engine: the settings of a plugin keep the writer's values within their limits",
          "[plugin][engine][settings]") {
    PluginFolder plugins;
    writeFullPlugin(plugins.folder() / "full.kplugin");
    auto& manager = PluginManager::getInstance();
    REQUIRE(manager.loadEnabledPlugins() == 1);
    const PluginContributions added = manager.contributionsOf(FULL_ID);
    REQUIRE(added.settings.size() == 2);
    const PluginSettingInfo& size = added.settings[0];
    const PluginSettingInfo& mode = added.settings[1];

    // The default until the writer gives a value
    CHECK(pluginSettingValue(FULL_ID, size) == 3);
    runWithPlugin(FULL_ID, "assert plugin.size.value == 3\n"
                           "assert plugin.size.plugin_id == 'org.kalahari.test.full'\n");

    // A value of the settings page reaches the plugin, which hears of the change
    REQUIRE(setPluginSettingValue(FULL_ID, size, 5));
    CHECK(SettingsManager::getInstance().get<json>("plugins.settings")[FULL_ID]["size"] == 5);
    runWithPlugin(FULL_ID, "assert plugin.size.value == 5\n");
    const auto plugin = std::dynamic_pointer_cast<PythonPlugin>(
        ExtensionPointRegistry::getInstance().getPlugin(FULL_ID));
    REQUIRE(plugin != nullptr);
    plugin->settingsChanged();
    CHECK(attributeOf<int>(FULL_ID, "changed") == 1);

    // Values outside the limits, of another kind or not among the choices are not taken
    CHECK_FALSE(setPluginSettingValue(FULL_ID, size, 11));
    CHECK_FALSE(setPluginSettingValue(FULL_ID, size, "5"));
    CHECK_FALSE(setPluginSettingValue(FULL_ID, size, 5.5));
    CHECK_FALSE(setPluginSettingValue(FULL_ID, mode, "medium"));
    CHECK(setPluginSettingValue(FULL_ID, mode, "long"));
    CHECK(pluginSettingValue(FULL_ID, size) == 5);

    // The plugin sets its own values by the same rules
    runWithPlugin(FULL_ID, "plugin.size.value = 7\n"
                           "try:\n"
                           "    plugin.size.value = 0\n"
                           "    raise AssertionError('0 was taken')\n"
                           "except ValueError:\n"
                           "    pass\n"
                           "assert plugin.size.value == 7\n"
                           "assert plugin.mode.value == 'long'\n");

    // A value changed by hand in the file reads as the default
    json all = SettingsManager::getInstance().get<json>("plugins.settings");
    all[FULL_ID]["size"] = "big";
    SettingsManager::getInstance().set<json>("plugins.settings", all);
    CHECK(pluginSettingValue(FULL_ID, size) == 3);

    forgetPluginSettings(FULL_ID);
    CHECK_FALSE(SettingsManager::getInstance().get<json>("plugins.settings").contains(FULL_ID));
}

TEST_CASE("Plugin engine: the settings of a plugin have the writer's values when it starts",
          "[plugin][engine][settings]") {
    PluginFolder plugins;
    writeFullPlugin(plugins.folder() / "full.kplugin");
    json all = SettingsManager::getInstance().get<json>("plugins.settings");
    all[FULL_ID] = {{"size", 6}};
    SettingsManager::getInstance().set<json>("plugins.settings", all);

    REQUIRE(PluginManager::getInstance().loadEnabledPlugins() == 1);
    CHECK(attributeOf<int>(FULL_ID, "size_at_start") == 6);
    forgetPluginSettings(FULL_ID);
}

TEST_CASE("Plugin engine: a declaration of a setting that cannot be kept is refused",
          "[plugin][engine][settings]") {
    PythonInterpreter::getInstance().initialize();
    py::gil_scoped_acquire gil;
    py::dict scope;
    py::exec(R"PY(
import kalahari_api as k
for wrong in (lambda: k.Setting("", "No id", 1),
              lambda: k.Setting("a", "List", [1]),
              lambda: k.Setting("a", "Out", 20, maximum=10),
              lambda: k.Setting("a", "Not a choice", "x", choices=[("y", "Y")]),
              lambda: k.Setting("a", "Pair", "x", choices=[("x",)])):
    try:
        wrong()
    except ValueError:
        continue
    except TypeError:
        continue
    raise AssertionError("taken")
setting = k.Setting("a", "Flag", True)
assert setting.value is True
try:
    setting.value = False
    raise AssertionError("set before the program read it")
except ValueError:
    pass
)PY",
             scope);
}

TEST_CASE("Plugin engine: a plugin turned off stays off, and on again without a restart",
          "[plugin][engine]") {
    PluginFolder plugins;
    writeFullPlugin(plugins.folder() / "full.kplugin");
    auto& manager = PluginManager::getInstance();
    std::vector<std::string> changes;
    const int listener =
        manager.addChangeListener([&changes](const std::string& id) { changes.push_back(id); });
    REQUIRE(manager.loadEnabledPlugins() == 1);
    CHECK(changes == std::vector<std::string>{FULL_ID});

    REQUIRE(manager.setPluginEnabled(FULL_ID, false));
    CHECK_FALSE(manager.isPluginLoaded(FULL_ID));
    CHECK(ExtensionPointRegistry::getInstance().getPlugin(FULL_ID) == nullptr);
    CHECK_FALSE(PluginManager::isPluginEnabled(FULL_ID));
    CHECK(SettingsManager::getInstance().get<json>("plugins.disabled") == json::array({FULL_ID}));
    CHECK(changes.size() == 2);

    // At the next start it is not loaded
    CHECK(manager.loadEnabledPlugins() == 0);
    CHECK_FALSE(manager.isPluginLoaded(FULL_ID));

    REQUIRE(manager.setPluginEnabled(FULL_ID, true));
    CHECK(manager.isPluginLoaded(FULL_ID));
    CHECK(PluginManager::isPluginEnabled(FULL_ID));
    CHECK(SettingsManager::getInstance().get<json>("plugins.disabled").empty());
    manager.removeChangeListener(listener);
}

TEST_CASE("Plugin engine: two plugins with entry modules of the same name are both themselves",
          "[plugin][engine]") {
    PluginFolder plugins;
    const auto write = [&plugins](const std::string& id, const std::string& word) {
        const std::string code = "WORD = '" + word +
                                 "'\n"
                                 "class Plugin:\n"
                                 "    def on_init(self): pass\n"
                                 "    def on_activate(self): pass\n"
                                 "    def word(self): return WORD\n";
        const fs::path file = plugins.folder() / (word + ".kplugin");
        writePlugin(file, id, "1.0.0", "plugin:Plugin", {{"plugin.py", code}});
        kalahari::test::signPlugin(file);
    };
    // Ids that would give one module name if every other character became a plain _
    const std::string first = "org.kalahari.test.first-one";
    const std::string second = "org.kalahari.test.first_one";
    write(first, "first");
    write(second, "second");
    auto& manager = PluginManager::getInstance();
    REQUIRE(manager.loadEnabledPlugins() == 2);

    const auto wordOf = [&manager](const std::string& id) {
        const PluginInstance* instance = manager.getPluginInstance(id);
        REQUIRE(instance != nullptr);
        py::gil_scoped_acquire gil;
        return instance->instance->attr("word")().cast<std::string>();
    };
    CHECK(wordOf(first) == "first");
    CHECK(wordOf(second) == "second");
    const std::string firstModule = manager.getPluginInstance(first)->moduleName;
    CHECK(firstModule == "kalahari_plugin_org_2ekalahari_2etest_2efirst_2done");
    CHECK(firstModule != manager.getPluginInstance(second)->moduleName);

    // Unloaded, its modules are forgotten: no plain "plugin" module is left behind
    manager.unloadAll();
    py::gil_scoped_acquire gil;
    const py::dict modules = py::module_::import("sys").attr("modules");
    CHECK_FALSE(modules.contains("plugin"));
    CHECK_FALSE(modules.contains(firstModule));
}

TEST_CASE("Plugin engine: only plugins signed by Kalahari are loaded", "[plugin][engine]") {
    PluginFolder plugins;
    auto& manager = PluginManager::getInstance();
    const fs::path file = plugins.folder() / "full.kplugin";

    SECTION("Not signed") {
        writePlugin(file, FULL_ID, "1.0.0", "plugin:Full", {{"plugin.py", FULL_PLUGIN}});
        CHECK(manager.discoverPlugins() == 0);
    }

    SECTION("Signed by a key the program does not trust") {
        writePlugin(file, FULL_ID, "1.0.0", "plugin:Full", {{"plugin.py", FULL_PLUGIN}});
        kalahari::test::signPluginWithStrangerKey(file);
        CHECK(manager.discoverPlugins() == 0);
    }

    SECTION("Changed after it was signed") {
        writeFullPlugin(file);
        const fs::path signature = fs::path(file) += ".sig";
        const std::string kept = textOf(signature);
        writePlugin(file, FULL_ID, "1.0.0", "plugin:Full",
                    {{"plugin.py", std::string(FULL_PLUGIN) + "\n# changed\n"}});
        std::ofstream(signature, std::ios::binary | std::ios::trunc) << kept;
        CHECK(manager.discoverPlugins() == 0);
    }

    SECTION("Signed") {
        writeFullPlugin(file);
        CHECK(manager.discoverPlugins() == 1);
    }
    CHECK(manager.loadEnabledPlugins() == manager.getDiscoveredPlugins().size());
}

TEST_CASE("Plugin engine: a plugin changed after it was found is not loaded when turned on",
          "[plugin][engine]") {
    PluginFolder plugins;
    auto& manager = PluginManager::getInstance();
    const fs::path file = plugins.folder() / "full.kplugin";
    writeFullPlugin(file);
    REQUIRE(manager.setPluginEnabled(FULL_ID, false));
    REQUIRE(manager.discoverPlugins() == 1);

    // Its file changed, with the signature of the one found
    const fs::path signature = fs::path(file) += ".sig";
    const std::string kept = textOf(signature);
    writePlugin(file, FULL_ID, "1.0.0", "plugin:Full",
                {{"plugin.py", std::string(FULL_PLUGIN) + "\n# changed\n"}});
    std::ofstream(signature, std::ios::binary | std::ios::trunc) << kept;

    CHECK_FALSE(manager.setPluginEnabled(FULL_ID, true));
    CHECK_FALSE(manager.isPluginLoaded(FULL_ID));
    const PluginInstance* refused = manager.getPluginInstance(FULL_ID);
    REQUIRE(refused != nullptr);
    CHECK(refused->state == PluginState::Error);
    CHECK(refused->error_message.find("Signature") != std::string::npos);
    CHECK(ExtensionPointRegistry::getInstance().getPlugin(FULL_ID) == nullptr);
}

TEST_CASE("Plugin engine: the licence check can keep a plugin from loading, saying why",
          "[plugin][engine]") {
    PluginFolder plugins;
    writeFullPlugin(plugins.folder() / "full.kplugin");
    auto& manager = PluginManager::getInstance();
    manager.setLicenseCheck([](const PluginManifest& manifest) -> std::optional<std::string> {
        return "No licence for " + manifest.id;
    });
    CHECK(manager.loadEnabledPlugins() == 0);
    const PluginInstance* instance = manager.getPluginInstance(FULL_ID);
    REQUIRE(instance != nullptr);
    CHECK(instance->state == PluginState::Error);
    CHECK(instance->error_message == "No licence for org.kalahari.test.full");
    CHECK(ExtensionPointRegistry::getInstance().getPlugin(FULL_ID) == nullptr);

    manager.setLicenseCheck(nullptr);
    CHECK(manager.setPluginEnabled(FULL_ID, true));
    CHECK(manager.isPluginLoaded(FULL_ID));
}

TEST_CASE("Plugin engine: a plugin is installed into the user's folder, replaced and removed",
          "[plugin][engine]") {
    QTemporaryDir profile;
    REQUIRE(profile.isValid());
    auto& settings = SettingsManager::getInstance();
    const fs::path before = settings.setSettingsFilePathForTests(pathOf(profile) / "settings.json");
    PluginFolder downloads;
    auto& manager = PluginManager::getInstance();
    manager.setPluginFolders({PluginManager::userPluginsFolder()});

    // The first version
    const fs::path first = downloads.folder() / "full-1.kplugin";
    writeFullPlugin(first, "1.0.0");
    std::string error;
    REQUIRE(manager.installPlugin(first, &error) == std::optional<std::string>(FULL_ID));
    CHECK(error.empty());
    CHECK(manager.isPluginLoaded(FULL_ID));
    CHECK(manager.isUserPlugin(FULL_ID));
    const fs::path installed = PluginManager::userPluginsFolder() / (std::string(FULL_ID) + ".kplugin");
    CHECK(fs::is_regular_file(installed));
    CHECK(fs::is_regular_file(fs::path(installed) += ".sig"));

    // A newer one in its place, loaded at once
    const fs::path second = downloads.folder() / "full-2.kplugin";
    writeFullPlugin(second, "2.0.0");
    REQUIRE(manager.installPlugin(second, &error) == std::optional<std::string>(FULL_ID));
    REQUIRE(manager.getPluginMetadata(FULL_ID) != nullptr);
    CHECK(manager.getPluginMetadata(FULL_ID)->version == "2.0.0");
    CHECK(manager.isPluginLoaded(FULL_ID));
    CHECK(std::distance(fs::directory_iterator(PluginManager::userPluginsFolder()),
                        fs::directory_iterator()) == 2);  // The plugin and its signature

    // A file that is not signed is not installed
    const fs::path unsigned_ = downloads.folder() / "other.kplugin";
    writePlugin(unsigned_, "org.kalahari.test.other", "1.0.0", "plugin:Full",
                {{"plugin.py", FULL_PLUGIN}});
    CHECK_FALSE(manager.installPlugin(unsigned_, &error).has_value());
    CHECK_FALSE(error.empty());

    // Removed with its settings
    REQUIRE(setPluginSettingValue(FULL_ID, manager.contributionsOf(FULL_ID).settings.at(0), 4));
    REQUIRE(manager.removePlugin(FULL_ID, true, &error));
    CHECK_FALSE(manager.isPluginLoaded(FULL_ID));
    CHECK(manager.getPluginMetadata(FULL_ID) == nullptr);
    CHECK_FALSE(fs::exists(installed));
    CHECK_FALSE(fs::exists(fs::path(installed) += ".sig"));
    CHECK_FALSE(settings.get<json>("plugins.settings").contains(FULL_ID));

    manager.setPluginFolders({});
    settings.setSettingsFilePathForTests(before);
}
