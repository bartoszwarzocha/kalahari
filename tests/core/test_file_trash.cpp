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
