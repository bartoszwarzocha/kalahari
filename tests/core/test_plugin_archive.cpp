/// @file test_plugin_archive.cpp
/// @brief A plugin archive is unpacked only into its own folder, and says why it was not
///
/// The names of the files of an archive are its author's: "../x", "/x" or "C:x" would write a
/// file outside the plugin's folder, wherever the user may write. Such an archive is not
/// unpacked at all, and neither is one with a file without a name; when a file of the archive
/// cannot be read or written, the writer gets a message, not a half unpacked plugin.

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/plugin_archive.h>
#include <kalahari/core/plugin_manager.h>
#include <kalahari/core/python_interpreter.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/core/unicode_paths.h>

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTranslator>
#include <QUuid>

#include <zip.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

using namespace kalahari::core;
namespace fs = std::filesystem;

namespace {

/// A file of an archive: its name in the archive and its contents
using Entry = std::pair<std::string, std::string>;

/// The folder of a temporary folder @p dir
fs::path pathOf(const QTemporaryDir& dir) {
    return {dir.path().toStdU16String()};
}

/// @p text in a std::string
std::string utf8(const std::u8string& text) {
    return {text.begin(), text.end()};
}

/// The text of file @p path
std::string textOf(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

/// A ZIP archive at @p path with @p entries, their names as given; @p stored keeps the
/// contents uncompressed
void writeArchive(const fs::path& path, const std::vector<Entry>& entries, bool stored = false) {
    int error = 0;
    zip_t* archive = openZipFile(path, ZIP_CREATE | ZIP_TRUNCATE, &error);
    REQUIRE(archive != nullptr);
    for (const auto& [name, contents] : entries) {
        zip_source_t* source = zip_source_buffer(archive, contents.data(), contents.size(), 0);
        const zip_int64_t index = zip_file_add(archive, name.c_str(), source, ZIP_FL_ENC_UTF_8);
        REQUIRE(index >= 0);
        if (stored) {
            REQUIRE(zip_set_file_compression(archive, static_cast<zip_uint64_t>(index),
                                             ZIP_CM_STORE, 0) == 0);
        }
    }
    REQUIRE(zip_close(archive) == 0);
}

/// A name no other test uses
std::string unique() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
}

/// Message @p text with the text of @p path in it, as the archive gives it
std::string withPath(const QString& text, const fs::path& path) {
    return text.arg(QString::fromStdString(pathText(path))).toStdString();
}

const std::string MANIFEST = R"({"id": "org.kalahari.test.archive"})";

/// The message for file @p name of plugin.kplugin, which would be written outside its folder
std::string outsideMessage(const std::string& name) {
    return QStringLiteral("The plugin \"plugin.kplugin\" was not unpacked: its file \"%1\" would "
                          "be written outside the plugin's folder.")
        .arg(QString::fromStdString(name))
        .toStdString();
}

/// The current folder is @p folder until the end of the test
class CurrentFolder {
public:
    explicit CurrentFolder(const fs::path& folder) : m_before(fs::current_path()) {
        fs::current_path(folder);
    }
    ~CurrentFolder() {
        std::error_code ignored;
        fs::current_path(m_before, ignored);
    }
    CurrentFolder(const CurrentFolder&) = delete;
    CurrentFolder& operator=(const CurrentFolder&) = delete;

private:
    fs::path m_before;
};

}  // namespace

TEST_CASE("Plugin archive: a file named to go outside the plugin's folder stops the unpacking",
          "[core][plugin]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const std::string escaped = "escaped-" + unique() + ".txt";
    const fs::path outside = pathOf(dir) / escaped;
    const fs::path archivePath = pathOf(dir) / "plugin.kplugin";

    struct Case {
        const char* label;
        std::string name;
    };
    const std::vector<Case> cases{
        {"../", "../" + escaped},
        {"dane/../../", "dane/../../" + escaped},
        {"..\\", "..\\" + escaped},                     // "\" is a separator on Windows
        {"absolute", utf8(outside.generic_u8string())},  // "/tmp/..." or "C:/Users/..."
        {"\\", "\\" + escaped},                         // the root of the current drive
        {"C:", "C:" + escaped},                           // the current folder of drive C
    };

    for (const Case& c : cases) {
        DYNAMIC_SECTION(c.label) {
            // Files before and after it: none of them is written either
            writeArchive(archivePath, {{"manifest.json", MANIFEST},
                                       {"dane/rozdzial.txt", "abc"},
                                       {c.name, "escaped"},
                                       {"z.txt", "z"}});
            const PluginArchive archive(archivePath);
            // Where the file would be written, joined to the folder as a path; on Windows "\"
            // and "C:" take it out of the folder
            const fs::path landing =
                fs::absolute((archive.getExtractedPath() / pathOfText(c.name)).lexically_normal());
            INFO("The file would be written to " << pathText(landing));

            CHECK_FALSE(archive.isValid());
            CHECK(archive.errorMessage() == outsideMessage(c.name));
            CHECK_FALSE(fs::exists(archive.getExtractedPath()));
            CHECK_FALSE(fs::exists(landing));
            std::error_code ignored;
            fs::remove(landing, ignored);
        }
    }
}

TEST_CASE("Plugin archive: the message names the file in Polish too", "[core][plugin]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const fs::path archivePath = pathOf(dir) / "plugin.kplugin";
    const std::string name = "../escaped-" + unique() + ".txt";
    writeArchive(archivePath, {{"manifest.json", MANIFEST}, {name, "escaped"}});

    QTranslator polish;
    REQUIRE(polish.load(QStringLiteral(":/i18n/kalahari_pl.qm")));
    QCoreApplication::installTranslator(&polish);
    const PluginArchive archive(archivePath);
    QCoreApplication::removeTranslator(&polish);

    CHECK_FALSE(archive.isValid());
    CHECK(archive.errorMessage() ==
          QStringLiteral("Nie rozpakowano wtyczki \u201Eplugin.kplugin\u201D: jej plik \u201E%1\u201D "
                         "zosta\u0142by zapisany poza folderem wtyczki.")
              .arg(QString::fromStdString(name))
              .toStdString());
}

TEST_CASE("Plugin archive: a file without a name stops the unpacking", "[core][plugin]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const fs::path archivePath = pathOf(dir) / "plugin.kplugin";
    writeArchive(archivePath, {{"manifest.json", MANIFEST}, {"", "no name"}, {"z.txt", "z"}});

    const PluginArchive archive(archivePath);
    CHECK_FALSE(archive.isValid());
    CHECK(archive.errorMessage() ==
          "The plugin \"plugin.kplugin\" was not unpacked: one of its files has no name.");
    CHECK_FALSE(fs::exists(archive.getExtractedPath()));
}

TEST_CASE("Plugin archive: names with dots that stay in the plugin's folder are unpacked",
          "[core][plugin]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const fs::path archivePath = pathOf(dir) / "plugin.kplugin";
    writeArchive(archivePath, {{"manifest.json", MANIFEST},
                               {"dane/", ""},
                               {"dane/notes..txt", "notes"},
                               {"dane/..config", "config"},
                               {"dane/a..b/c.txt", "c"},
                               {"./readme.txt", "readme"}});

    const PluginArchive archive(archivePath);
    REQUIRE(archive.isValid());
    CHECK(archive.errorMessage().empty());
    const fs::path& folder = archive.getExtractedPath();
    CHECK(fs::is_directory(folder / "dane"));
    CHECK(textOf(folder / "dane" / "notes..txt") == "notes");
    CHECK(textOf(folder / "dane" / "..config") == "config");
    CHECK(textOf(folder / "dane" / "a..b" / "c.txt") == "c");
    CHECK(textOf(folder / "readme.txt") == "readme");
}

TEST_CASE("Plugin archive: a file that is not an archive gives a message", "[core][plugin]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const fs::path archivePath = pathOf(dir) / "plugin.kplugin";
    std::ofstream(archivePath, std::ios::binary) << "not a ZIP archive";

    const PluginArchive archive(archivePath);
    CHECK_FALSE(archive.isValid());
    CHECK(archive.errorMessage() ==
          "The plugin \"plugin.kplugin\" cannot be opened: its file is not a ZIP archive, it is "
          "damaged or it cannot be read.");
}

TEST_CASE("Plugin archive: a damaged file of the archive stops the unpacking, with a message",
          "[core][plugin]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const fs::path archivePath = pathOf(dir) / "plugin.kplugin";
    const std::string contents = "the contents of the chapter, " + unique();
    writeArchive(archivePath, {{"manifest.json", MANIFEST}, {"dane/rozdzial.txt", contents}},
                 true);

    // One character of the stored contents changed: its checksum no longer agrees
    std::string bytes = textOf(archivePath);
    const std::size_t at = bytes.find(contents);
    REQUIRE(at != std::string::npos);
    bytes[at] = 'T';
    std::ofstream(archivePath, std::ios::binary | std::ios::trunc) << bytes;

    const PluginArchive archive(archivePath);
    CHECK_FALSE(archive.isValid());
    CHECK(archive.errorMessage() ==
          "The plugin \"plugin.kplugin\" cannot be unpacked: its file \"dane/rozdzial.txt\" cannot "
          "be read from the archive, which may be damaged.");
}

TEST_CASE("Plugin archive: a folder that cannot be made stops the unpacking, with a message",
          "[core][plugin]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const fs::path archivePath = pathOf(dir) / "plugin.kplugin";
    // "dane" is a file, so the folder "dane" of the next file cannot be made
    writeArchive(archivePath, {{"manifest.json", MANIFEST},
                               {"dane", "a file"},
                               {"dane/rozdzial.txt", "abc"}});

    const PluginArchive archive(archivePath);
    CHECK_FALSE(archive.isValid());
    CHECK(archive.errorMessage() ==
          withPath(QStringLiteral("The plugin \"plugin.kplugin\" cannot be unpacked: the folder "
                                  "\"%1\" cannot be created."),
                   archive.getExtractedPath() / "dane"));
}

TEST_CASE("Plugin archive: a file that cannot be written stops the unpacking, with a message",
          "[core][plugin]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const fs::path archivePath = pathOf(dir) / "plugin.kplugin";
    // "dane" is a folder, so the file "dane" cannot be written
    writeArchive(archivePath, {{"manifest.json", MANIFEST},
                               {"dane/rozdzial.txt", "abc"},
                               {"dane", "a file"}});

    const PluginArchive archive(archivePath);
    CHECK_FALSE(archive.isValid());
    CHECK(archive.errorMessage() ==
          withPath(QStringLiteral("The plugin \"plugin.kplugin\" cannot be unpacked: the file "
                                  "\"%1\" cannot be written."),
                   archive.getExtractedPath() / "dane"));
}

TEST_CASE("Plugin Manager: a plugin that is not unpacked is not loaded, and says why",
          "[plugin]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    REQUIRE(fs::create_directories(pathOf(dir) / "plugins"));
    const std::string id = "org.kalahari.test.outside";
    const std::string name = "../escaped-" + unique() + ".txt";
    const std::string manifest = R"({"id": ")" + id + R"(", "name": "Outside", )"
                                 R"("version": "1.0.0", "api_version": "1.0.0", )"
                                 R"("entry_point": "outside:Outside"})";
    writeArchive(pathOf(dir) / "plugins" / "outside.kplugin",
                 {{"manifest.json", manifest}, {name, "escaped"}});

    PythonInterpreter::getInstance().initialize();
    auto& settings = SettingsManager::getInstance();
    const bool allowedBefore = settings.get<bool>("plugins.allowUnsigned", false);
    settings.set<bool>("plugins.allowUnsigned", true);
    auto& manager = PluginManager::getInstance();
    {
        // The plugins are looked for in the current folder first
        const CurrentFolder current(pathOf(dir));
        manager.discoverPlugins();
        REQUIRE(manager.getPluginMetadata(id) != nullptr);

        CHECK_FALSE(manager.loadPlugin(id));
        const PluginInstance* instance = manager.getPluginInstance(id);
        REQUIRE(instance != nullptr);
        CHECK(instance->state == PluginState::Error);
        CHECK(instance->error_message ==
              QStringLiteral("The plugin \"outside.kplugin\" was not unpacked: its file \"%1\" "
                             "would be written outside the plugin's folder.")
                  .arg(QString::fromStdString(name))
                  .toStdString());
        if (instance->archive) {
            std::error_code ignored;
            fs::remove(instance->archive->getExtractedPath().parent_path() /
                           fs::path(name).filename(),
                       ignored);
        }
    }
    settings.set<bool>("plugins.allowUnsigned", allowedBefore);
    manager.discoverPlugins();
}
