/// @file test_unicode_paths.cpp
/// @brief Paths with characters outside the code page of the system: the settings, the log,
/// the plugins and the archives work in a folder of any name
///
/// On Windows, std::filesystem::path::string() threw for such a path ("No mapping for the
/// Unicode character exists in the target multi-byte code page"), and zip_open() did not find
/// the archive. The folders of the tests have a character of each script, so that no single
/// code page has all of them. The names are written with universal character names: the
/// compiler reads the sources in the code page of the system on Windows.

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/logger.h>
#include <kalahari/core/plugin_archive.h>
#include <kalahari/core/plugin_signature.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/core/unicode_paths.h>

#include <QTemporaryDir>
#include <QUuid>

#include <zip.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace kalahari::core;
namespace fs = std::filesystem;

namespace {

/// "ś Ж 日本": Polish, Cyrillic and Japanese
const std::u8string UNUSUAL = u8"ś Ж 日本";

/// @p text in a std::string, as pathText() gives it
std::string utf8(const std::u8string& text) {
    return {text.begin(), text.end()};
}

/// The folder of a temporary folder @p dir
fs::path pathOf(const QTemporaryDir& dir) {
    return {dir.path().toStdU16String()};
}

/// The text of file @p path
std::string textOf(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

/// A plugin archive at @p path: its manifest and a file whose name has the characters
void writePlugin(const fs::path& path, const std::u8string& dataFile) {
    static const std::string manifest = R"({"id": "org.kalahari.test.paths"})";
    static const std::string data = "abc";
    int error = 0;
    zip_t* archive = openZipFile(path, ZIP_CREATE | ZIP_TRUNCATE, &error);
    REQUIRE(archive != nullptr);
    zip_source_t* manifestSource = zip_source_buffer(archive, manifest.data(), manifest.size(), 0);
    REQUIRE(zip_file_add(archive, "manifest.json", manifestSource, ZIP_FL_ENC_UTF_8) >= 0);
    zip_source_t* dataSource = zip_source_buffer(archive, data.data(), data.size(), 0);
    REQUIRE(zip_file_add(archive, utf8(dataFile).c_str(), dataSource, ZIP_FL_ENC_UTF_8) >= 0);
    REQUIRE(zip_close(archive) == 0);
}

/// The temporary folder of the system is @p folder until the end of the test
class TempFolder {
public:
    explicit TempFolder(const fs::path& folder) {
#ifdef _WIN32
        for (const wchar_t* name : {L"TMP", L"TEMP"}) {
            std::optional<std::wstring> value;
            const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
            if (size > 0) {
                value.emplace(size, L'\0');
                value->resize(GetEnvironmentVariableW(name, value->data(), size));
            }
            m_before.emplace_back(name, value);
            SetEnvironmentVariableW(name, folder.c_str());
        }
#else
        const char* value = std::getenv("TMPDIR");
        m_before = value ? std::optional<std::string>(value) : std::nullopt;
        setenv("TMPDIR", folder.c_str(), 1);
#endif
    }
    ~TempFolder() {
#ifdef _WIN32
        for (const auto& [name, value] : m_before) {
            SetEnvironmentVariableW(name.c_str(), value ? value->c_str() : nullptr);
        }
#else
        if (m_before) {
            setenv("TMPDIR", m_before->c_str(), 1);
        } else {
            unsetenv("TMPDIR");
        }
#endif
    }
    TempFolder(const TempFolder&) = delete;
    TempFolder& operator=(const TempFolder&) = delete;

private:
#ifdef _WIN32
    std::vector<std::pair<std::wstring, std::optional<std::wstring>>> m_before;
#else
    std::optional<std::string> m_before;
#endif
};

}  // namespace

TEST_CASE("Paths: their text is UTF-8, whatever the code page of the system",
          "[core][paths]") {
    const std::u8string text = u8"Księga " + UNUSUAL + u8"/rozdział.txt";
    const fs::path path(text);
    CHECK(pathText(path) == utf8(text));
    CHECK(pathOfText(utf8(text)) == path);
    CHECK(pathText(pathOfText(utf8(text))) == utf8(text));
}

TEST_CASE("Paths: an archive in a folder whose name is outside the code page opens",
          "[core][paths]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const fs::path folder = pathOf(dir) / UNUSUAL;
    REQUIRE(fs::create_directories(folder));
    const fs::path path = folder / (UNUSUAL + u8".zip");
    const std::u8string dataFile = u8"dane/słownik " + UNUSUAL + u8".txt";
    writePlugin(path, dataFile);
    REQUIRE(fs::is_regular_file(path));

    int error = 0;
    zip_t* archive = openZipFile(path, ZIP_RDONLY, &error);
    REQUIRE(archive != nullptr);
    CHECK(zip_get_num_entries(archive, 0) == 2);
    CHECK(zip_name_locate(archive, utf8(dataFile).c_str(), 0) >= 0);
    zip_discard(archive);

    // An archive that is not there
    CHECK(openZipFile(folder / u8"brak.zip", ZIP_RDONLY, &error) == nullptr);
    CHECK(error != ZIP_ER_OK);
}

TEST_CASE("Paths: a plugin in a folder whose name is outside the code page is unpacked, "
          "hashed and checked",
          "[core][paths][plugin]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const std::u8string dataFile = u8"dane/słownik " + UNUSUAL + u8".txt";
    const fs::path plain = pathOf(dir) / "plain.kplugin";
    writePlugin(plain, dataFile);
    const fs::path folder = pathOf(dir) / (u8"Wtyczki " + UNUSUAL);
    REQUIRE(fs::create_directories(folder));
    const fs::path path = folder / (u8"próba " + UNUSUAL + u8".kplugin");
    REQUIRE(fs::copy_file(plain, path));

    // Unpacked, with the names of its files and of the archive
    {
        const PluginArchive archive(path);
        REQUIRE(archive.isValid());
        CHECK(archive.hasManifest());
        const fs::path unpacked = archive.getExtractedPath() / pathOfText(utf8(dataFile));
        CHECK(fs::is_regular_file(unpacked));
        CHECK(textOf(unpacked) == "abc");
        const std::string name = pathText(archive.getExtractedPath().filename());
        CHECK(name.ends_with(utf8(u8"próba " + UNUSUAL)));
    }

    // The hash of its contents, as of the same archive elsewhere
    const std::string hash = PluginSignature::computeArchiveHash(path);
    CHECK_FALSE(hash.empty());
    CHECK(hash == PluginSignature::computeArchiveHash(plain));

    // Without a signature, which the message names by its path
    std::string signedBy;
    std::string message;
    CHECK(PluginSignature::verify(path, signedBy, message) ==
          PluginSignature::VerifyResult::NotFound);
    CHECK(message == "Signature file not found: " + pathText(path) + ".sig");
}

TEST_CASE("Paths: the settings in a folder whose name is outside the code page",
          "[core][paths][settings]") {
    auto& settings = SettingsManager::getInstance();
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const fs::path file = pathOf(dir) / (u8"Ustawienia " + UNUSUAL) / "settings.json";
    const fs::path before = settings.setSettingsFilePathForTests(file);

    // No file yet: the defaults; then the file is written and read again
    CHECK(settings.load());
    settings.set<int>("editor.fontSize", 15);
    CHECK(settings.save());
    CHECK(fs::is_regular_file(file));
    settings.set<int>("editor.fontSize", 16);
    CHECK(settings.load());
    CHECK(settings.get<int>("editor.fontSize", 14) == 15);

    // A broken file goes aside, and the defaults take its place
    std::ofstream(file) << "{ broken";
    CHECK_FALSE(settings.load());
    fs::path aside = file;
    aside += ".bak";
    CHECK(fs::is_regular_file(aside));

    settings.resetToDefaults();
    CHECK_FALSE(fs::exists(file));

    settings.setSettingsFilePathForTests(before);
    settings.load();
}

TEST_CASE("Paths: the log in a temp directory whose name is outside the code page",
          "[core][paths][logger]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const fs::path temp = pathOf(dir) / (u8"Temp " + UNUSUAL);
    REQUIRE(fs::create_directories(temp));

    // The log file asked for cannot be written: its folder is a file
    const fs::path blocker = pathOf(dir) / "blocker";
    std::ofstream(blocker) << "not a directory";
    const std::string unwritable = pathText(blocker / "kalahari.log");

    const TempFolder tempFolder(temp);
    REQUIRE(fs::equivalent(fs::temp_directory_path(), temp));
    std::string opened;
    spdlog::sink_ptr sink;
    REQUIRE_NOTHROW(sink = Logger::openFileSink(unwritable, opened));
#ifndef _WIN32
    // Elsewhere than on Windows the narrow name of the file is UTF-8, so the log goes there
    CHECK(sink != nullptr);
    CHECK(opened == pathText(temp / "kalahari.log"));
#endif
}
