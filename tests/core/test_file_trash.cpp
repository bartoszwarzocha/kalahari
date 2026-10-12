/// @file test_file_trash.cpp
/// @brief Files to the Trash of the system, and the Trash of a test in its place

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/file_trash.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTemporaryDir>

#include <functional>
#include <utility>

#if defined(Q_OS_WIN)
#include <QStorageInfo>

#include <string>

#include <windows.h>
#endif

using namespace kalahari::core;

namespace {

void writeFile(const QString& path, const QByteArray& data) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(data);
}

/// @brief An environment variable with a value of the test, given back when the test ends
class EnvironmentVariable {
public:
    EnvironmentVariable(const char* name, const QString& value)
        : m_name(name), m_wasSet(qEnvironmentVariableIsSet(name)), m_value(qgetenv(name)) {
        qputenv(name, QFile::encodeName(value));
    }
    ~EnvironmentVariable() {
        if (m_wasSet) {
            qputenv(m_name, m_value);
        } else {
            qunsetenv(m_name);
        }
    }
    EnvironmentVariable(const EnvironmentVariable&) = delete;
    EnvironmentVariable& operator=(const EnvironmentVariable&) = delete;

private:
    const char* m_name;
    bool m_wasSet;
    QByteArray m_value;
};

/// @brief The Trash of the test in place of the system's until the test ends
struct TestTrashInUse {
    TestTrashInUse(std::function<bool(const QString&)> canMove,
                   std::function<bool(const QString&)> move) {
        setTrashForTests(std::move(canMove), std::move(move));
    }
    ~TestTrashInUse() { setTrashForTests({}, {}); }
    TestTrashInUse(const TestTrashInUse&) = delete;
    TestTrashInUse& operator=(const TestTrashInUse&) = delete;
};

#if defined(Q_OS_WIN)

/// @brief HKEY_CURRENT_USER of the program is a key of the test until the test ends: the test
/// gives it the settings it needs, and the writer's settings stay as they are
///
/// Windows keeps the key of the test in memory only (a volatile key), and the test deletes it
/// when it ends.
class CurrentUserOfTest {
public:
    CurrentUserOfTest()
        : m_name(L"Software\\Kalahari Tests\\File Trash " +
                 std::to_wstring(GetCurrentProcessId())) {
        if (RegCreateKeyExW(HKEY_CURRENT_USER, m_name.c_str(), 0, nullptr, REG_OPTION_VOLATILE,
                            KEY_ALL_ACCESS, nullptr, &m_key, nullptr) == ERROR_SUCCESS) {
            m_ready = RegOverridePredefKey(HKEY_CURRENT_USER, m_key) == ERROR_SUCCESS;
        }
    }
    ~CurrentUserOfTest() {
        if (m_ready) {
            RegOverridePredefKey(HKEY_CURRENT_USER, nullptr);
        }
        if (m_key) {
            RegCloseKey(m_key);
            RegDeleteTreeW(HKEY_CURRENT_USER, m_name.c_str());
            RegDeleteKeyW(HKEY_CURRENT_USER, m_name.c_str());
            RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Kalahari Tests");  // when it is empty
        }
    }
    CurrentUserOfTest(const CurrentUserOfTest&) = delete;
    CurrentUserOfTest& operator=(const CurrentUserOfTest&) = delete;

    /// @brief Whether HKEY_CURRENT_USER of the program is the key of the test
    [[nodiscard]] bool ready() const { return m_ready; }

    /// @brief Set the number @p name in key @p key of the test's HKEY_CURRENT_USER
    [[nodiscard]] bool setNumber(const std::wstring& key, const wchar_t* name,
                                 DWORD value) const {
        HKEY inner = nullptr;
        if (RegCreateKeyExW(m_key, key.c_str(), 0, nullptr, REG_OPTION_VOLATILE, KEY_SET_VALUE,
                            nullptr, &inner, nullptr) != ERROR_SUCCESS) {
            return false;
        }
        const LSTATUS status =
            RegSetValueExW(inner, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value),
                           static_cast<DWORD>(sizeof(value)));
        RegCloseKey(inner);
        return status == ERROR_SUCCESS;
    }

private:
    std::wstring m_name;
    HKEY m_key = nullptr;
    bool m_ready = false;
};

#endif

}  // namespace

TEST_CASE("Files go to the Trash of the system", "[file_trash]") {
#if defined(Q_OS_LINUX)
    // The home folder and the folder of the user's data are the test's: so is the Trash
    QTemporaryDir home;
    QTemporaryDir data;
    QTemporaryDir books;
    REQUIRE(home.isValid());
    REQUIRE(data.isValid());
    REQUIRE(books.isValid());
    const EnvironmentVariable homeVariable("HOME", home.path());
    const EnvironmentVariable dataVariable("XDG_DATA_HOME", data.path());

    const QString path = QDir(books.path()).filePath(QStringLiteral("chapter_001.kchapter"));
    writeFile(path, "{}");
    CHECK(canMoveToTrash(books.path()));
    CHECK(canMoveToTrash(path));

    REQUIRE(moveToTrash(path));
    CHECK_FALSE(QFileInfo::exists(path));
    const QDir trash(QDir(data.path()).filePath(QStringLiteral("Trash")));
    CHECK(QFileInfo::exists(trash.filePath(QStringLiteral("files/chapter_001.kchapter"))));
    CHECK(QFileInfo::exists(
        trash.filePath(QStringLiteral("info/chapter_001.kchapter.trashinfo"))));

    // A file that is not there goes nowhere
    CHECK_FALSE(moveToTrash(path));
    CHECK_FALSE(canMoveToTrash(path));
#else
    SKIP("The Trash of the system is checked on Linux, where the test has a Trash of its own");
#endif
}

TEST_CASE("The user's Trash when the folder of the user's data is missing", "[file_trash]") {
#if defined(Q_OS_LINUX)
    QTemporaryDir home;
    QTemporaryDir books;
    REQUIRE(home.isValid());
    REQUIRE(books.isValid());
    const EnvironmentVariable homeVariable("HOME", home.path());
    const QString data = QDir(home.path()).filePath(QStringLiteral(".local/share"));
    const EnvironmentVariable dataVariable("XDG_DATA_HOME", data);
    const QString path = QDir(books.path()).filePath(QStringLiteral("chapter_001.kchapter"));
    writeFile(path, "{}");

    SECTION("The program makes the folder, for the user only, with the Trash in it") {
        REQUIRE(moveToTrash(path));
        CHECK_FALSE(QFileInfo::exists(path));
        CHECK(QFileInfo::exists(
            QDir(data).filePath(QStringLiteral("Trash/files/chapter_001.kchapter"))));
        const QFileDevice::Permissions permissions = QFile::permissions(data);
        CHECK(permissions.testFlag(QFileDevice::WriteOwner));
        CHECK_FALSE(permissions.testFlag(QFileDevice::ReadGroup));
        CHECK_FALSE(permissions.testFlag(QFileDevice::ReadOther));
    }

    SECTION("A Trash that is not a folder takes no files") {
        REQUIRE(QDir().mkpath(data));
        writeFile(QDir(data).filePath(QStringLiteral("Trash")), "not a folder");
        CHECK_FALSE(canMoveToTrash(path));
        CHECK_FALSE(moveToTrash(path));
        CHECK(QFileInfo::exists(path));
    }

    // The file does not go to the working folder of the program
    CHECK_FALSE(
        QFileInfo::exists(QDir::current().filePath(QStringLiteral("files/chapter_001.kchapter"))));
#else
    SKIP("The Trash of the system is checked on Linux, where the test has a Trash of its own");
#endif
}

TEST_CASE("A drive whose Recycle Bin is switched off has no Trash", "[file_trash]") {
#if defined(Q_OS_WIN)
    QTemporaryDir books;
    REQUIRE(books.isValid());
    const QString path = QDir(books.path()).filePath(QStringLiteral("chapter_001.kchapter"));
    writeFile(path, "{}");

    // The drive of the test's folder and the GUID of its volume, under which Windows keeps the
    // writer's settings of the drive's Recycle Bin
    std::wstring root =
        QDir::toNativeSeparators(QStorageInfo(books.path()).rootPath()).toStdWString();
    if (!root.ends_with(L'\\')) {
        root += L'\\';
    }
    wchar_t volume[MAX_PATH] = {};
    if (GetDriveTypeW(root.c_str()) != DRIVE_FIXED ||
        !GetVolumeNameForVolumeMountPointW(root.c_str(), volume, MAX_PATH)) {
        SKIP("The folder of the test is not on a fixed drive with a volume");
    }
    const std::wstring volumeName(volume);
    const std::size_t open = volumeName.find(L'{');
    const std::size_t close = volumeName.find(L'}', open);
    REQUIRE(open != std::wstring::npos);
    REQUIRE(close != std::wstring::npos);
    const std::wstring drive =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\BitBucket\\Volume\\" +
        volumeName.substr(open, close - open + 1);
    const std::wstring policies =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer";

    // A policy of the computer switches the Recycle Bin off for every user
    DWORD machinePolicy = 0;
    DWORD size = static_cast<DWORD>(sizeof(machinePolicy));
    if (RegGetValueW(HKEY_LOCAL_MACHINE, policies.c_str(), L"NoRecycleFiles", RRF_RT_REG_DWORD,
                     nullptr, &machinePolicy, &size) == ERROR_SUCCESS &&
        machinePolicy != 0) {
        SKIP("A policy of this computer switches the Recycle Bin off");
    }

    const CurrentUserOfTest user;
    REQUIRE(user.ready());
    CHECK(canMoveToTrash(path));

    SECTION("The writer switched the Recycle Bin of the drive off, and on again") {
        REQUIRE(user.setNumber(drive, L"NukeOnDelete", 1));
        CHECK_FALSE(canMoveToTrash(path));
        CHECK_FALSE(canMoveToTrash(books.path()));

        REQUIRE(user.setNumber(drive, L"NukeOnDelete", 0));
        CHECK(canMoveToTrash(path));
    }

    SECTION("A policy of the user switches the Recycle Bin off") {
        REQUIRE(user.setNumber(policies, L"NoRecycleFiles", 1));
        CHECK_FALSE(canMoveToTrash(path));
    }

    CHECK(QFileInfo::exists(path));
#else
    SKIP("The Recycle Bin of a drive is a setting of Windows");
#endif
}

TEST_CASE("A test puts its own Trash in place of the system's", "[file_trash]") {
    QTemporaryDir books;
    REQUIRE(books.isValid());
    const QString path = QDir(books.path()).filePath(QStringLiteral("chapter_001.kchapter"));
    writeFile(path, "{}");
    const QString missing = QDir(books.path()).filePath(QStringLiteral("missing.kchapter"));

    SECTION("A Trash that takes the files") {
        QStringList asked;
        QStringList moved;
        const TestTrashInUse trash(
            [&asked](const QString& file) {
                asked << file;
                return true;
            },
            [&moved](const QString& file) {
                moved << file;
                return QFile::remove(file);
            });
        CHECK(canMoveToTrash(path));
        CHECK_FALSE(canMoveToTrash(missing));
        CHECK(asked == QStringList{path});

        CHECK(moveToTrash(path));
        CHECK_FALSE(QFileInfo::exists(path));
        CHECK_FALSE(moveToTrash(missing));
        CHECK(moved == QStringList{path});
    }

    SECTION("A drive without a Trash, and a Trash that refuses a file") {
        const TestTrashInUse trash([](const QString&) { return false; },
                                   [](const QString&) { return false; });
        CHECK_FALSE(canMoveToTrash(path));
        CHECK_FALSE(moveToTrash(path));
        CHECK(QFileInfo::exists(path));
    }

    SECTION("A Trash that says yes, but leaves the file") {
        const TestTrashInUse trash([](const QString&) { return true; },
                                   [](const QString&) { return true; });
        CHECK_FALSE(moveToTrash(path));
        CHECK(QFileInfo::exists(path));
    }
}
