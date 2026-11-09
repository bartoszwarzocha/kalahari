/// @file test_data_folders.cpp
/// @brief A copy of the program with a data folder of its own (kalahari-data.json next to its
/// executable): the folder it keeps the writer's data in, its name, and the files that cannot
/// be used

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/data_folders.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/core/unicode_paths.h>

#include <QDir>

#include <filesystem>
#include <fstream>
#include <string>

using namespace kalahari::core;
namespace fs = std::filesystem;

namespace {

/// A folder of a program for one test, in the temporary folder of the test process, removed
/// with everything in it afterwards
class ProgramFolder {
public:
    ProgramFolder() {
        static int number = 0;
        m_root = fs::temp_directory_path() / ("kalahari_data_folders_" + std::to_string(++number));
        fs::remove_all(m_root);
        m_program = m_root / "build" / "bin";
        fs::create_directories(m_program);
    }

    ~ProgramFolder() {
        std::error_code error;
        fs::remove_all(m_root, error);
    }

    const fs::path& root() const { return m_root; }
    const fs::path& program() const { return m_program; }

    /// Write @p content (UTF-8) to the file next to the program
    void writeFile(const std::string& content) const {
        std::ofstream file(m_program / DataFolders::FILE_NAME, std::ios::binary);
        file << content;
    }

private:
    fs::path m_root;
    fs::path m_program;
};

/// Path @p path as Qt text with forward slashes, as QDir gives it
QString qtText(const fs::path& path) {
    return QDir::fromNativeSeparators(QString::fromStdU16String(path.u16string()));
}

}  // namespace

TEST_CASE("A copy without the file keeps its data in the folders of the system",
          "[data_folders]") {
    const ProgramFolder folder;
    const DataFolders data = DataFolders::read(folder.program());

    CHECK(data.kind() == DataFolders::Kind::System);
    CHECK_FALSE(data.hasOwnFolder());
    CHECK(data.folder().empty());
    CHECK(data.ownFolder(QStringLiteral("grammar")).isEmpty());
    CHECK(data.programName() == QStringLiteral("Kalahari"));
    CHECK(data.problem() == DataFolders::Problem::None);
    CHECK(data.file() == folder.program() / DataFolders::FILE_NAME);
}

TEST_CASE("Without the folder of the executable there is no file to read", "[data_folders]") {
    const DataFolders data = DataFolders::read(fs::path());
    CHECK(data.kind() == DataFolders::Kind::System);
    CHECK(data.file().empty());
}

TEST_CASE("The tests keep the folders of the system, whatever is next to their executable",
          "[data_folders]") {
    // The tests run with KALAHARI_TEST_MODE (test_main.cpp); their settings stay in their own
    // temporary folder
    REQUIRE(SettingsManager::isTestMode());
    CHECK(DataFolders::current().kind() == DataFolders::Kind::System);
}

TEST_CASE("A copy with the file keeps its data in the folder of the file", "[data_folders]") {
    const ProgramFolder folder;

    SECTION("A folder relative to the folder of the executable, made at the start") {
        folder.writeFile(R"({"folder": "../../program-data", "name": "test copy"})");
        const DataFolders data = DataFolders::read(folder.program());

        REQUIRE(data.kind() == DataFolders::Kind::Own);
        CHECK(data.hasOwnFolder());
        CHECK(data.folder() == folder.root() / "program-data");
        CHECK(data.folder().is_absolute());
        CHECK(fs::is_directory(data.folder()));
        CHECK(data.ownFolder(QStringLiteral("grammar")) ==
              qtText(folder.root() / "program-data") + QStringLiteral("/grammar"));
        CHECK(data.name() == QStringLiteral("test copy"));
        CHECK(data.programName() == QStringLiteral("Kalahari (test copy)"));
        CHECK(data.problem() == DataFolders::Problem::None);
    }

    SECTION("An absolute folder, with a separator at its end") {
        const fs::path own = folder.root() / "elsewhere" / "data";
        // With forward slashes: JSON text needs no escaped backslashes then
        const std::u8string text = own.generic_u8string();
        folder.writeFile("{\"folder\": \"" + std::string(text.begin(), text.end()) + "/\"}");
        const DataFolders data = DataFolders::read(folder.program());

        REQUIRE(data.kind() == DataFolders::Kind::Own);
        CHECK(data.folder() == own);
        CHECK(fs::is_directory(own));
        CHECK(data.name().isEmpty());
        CHECK(data.programName() == QStringLiteral("Kalahari"));
    }

    SECTION("A folder and a name with letters outside the code page of the system") {
        folder.writeFile(
            "{\"folder\": \"dane-\xC5\x9B\xC4\x85\xC5\xBC\", \"name\": \"kopia "
            "\xC5\x9B\xC4\x85\xC5\xBC\"}");
        const DataFolders data = DataFolders::read(folder.program());

        REQUIRE(data.kind() == DataFolders::Kind::Own);
        CHECK(data.folder() == folder.program() / pathOfText("dane-\xC5\x9B\xC4\x85\xC5\xBC"));
        CHECK(fs::is_directory(data.folder()));
        CHECK(data.programName() ==
              QStringLiteral("Kalahari (kopia ") + QString::fromUtf8("\xC5\x9B\xC4\x85\xC5\xBC") +
                  QLatin1Char(')'));
    }

    SECTION("An existing folder is taken as it is") {
        fs::create_directories(folder.root() / "ready");
        {
            std::ofstream kept(folder.root() / "ready" / "settings.json");
            kept << "{}";
        }
        folder.writeFile(R"({"folder": "../../ready"})");
        const DataFolders data = DataFolders::read(folder.program());

        REQUIRE(data.kind() == DataFolders::Kind::Own);
        CHECK(fs::is_regular_file(folder.root() / "ready" / "settings.json"));
    }
}

TEST_CASE("A copy with a file that cannot be used does not take the folders of the system",
          "[data_folders]") {
    const ProgramFolder folder;
    DataFolders::Problem expected = DataFolders::Problem::None;

    SECTION("Not JSON") {
        folder.writeFile("folder = ../../data");
        expected = DataFolders::Problem::NotJson;
    }
    SECTION("JSON, but not an object") {
        folder.writeFile(R"(["../../data"])");
        expected = DataFolders::Problem::NotJson;
    }
    SECTION("No folder") {
        folder.writeFile(R"({"name": "test copy"})");
        expected = DataFolders::Problem::NoFolder;
    }
    SECTION("An empty folder") {
        folder.writeFile(R"({"folder": "  "})");
        expected = DataFolders::Problem::NoFolder;
    }
    SECTION("A folder that is not text") {
        folder.writeFile(R"({"folder": 7})");
        expected = DataFolders::Problem::NoFolder;
    }
    SECTION("A name that is not text") {
        folder.writeFile(R"({"folder": "../../data", "name": true})");
        expected = DataFolders::Problem::NameNotText;
    }
    SECTION("A folder that is a file") {
        {
            std::ofstream file(folder.root() / "data");
            file << "not a folder";
        }
        folder.writeFile(R"({"folder": "../../data"})");
        expected = DataFolders::Problem::FolderNotMade;
    }
    SECTION("A folder in place of the file") {
        fs::create_directories(folder.program() / DataFolders::FILE_NAME);
        expected = DataFolders::Problem::Unreadable;
    }

    const DataFolders data = DataFolders::read(folder.program());
    CHECK(data.kind() == DataFolders::Kind::Broken);
    CHECK(data.problem() == expected);
    CHECK_FALSE(data.hasOwnFolder());
    CHECK(data.folder().empty());
    CHECK(data.ownFolder(QStringLiteral("grammar")).isEmpty());
    CHECK(data.programName() == QStringLiteral("Kalahari"));
    CHECK_FALSE(data.problemText().empty());
}

TEST_CASE("Settings without a file are neither read nor saved", "[data_folders][settings]") {
    // A copy whose data folder cannot be used has no settings file until it ends (main())
    auto& settings = SettingsManager::getInstance();
    const fs::path before = settings.setSettingsFilePathForTests(fs::path());

    CHECK(settings.load());
    settings.set<int>("dashboard.maxItems", 7);
    CHECK_FALSE(settings.save());
    CHECK(settings.getSettingsFilePath().empty());

    settings.setSettingsFilePathForTests(before);
    settings.load();
}
