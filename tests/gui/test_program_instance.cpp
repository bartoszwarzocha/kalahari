/// @file test_program_instance.cpp
/// @brief One program for the writer: a later start hands its request to the program that
/// runs

#include <catch2/catch_test_macros.hpp>
#include <kalahari/gui/program_instance.h>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QStringList>
#include <QTemporaryDir>
#include <QThread>
#include <QUuid>

#include <atomic>
#include <thread>
#include <utility>

using kalahari::gui::ProgramInstance;

namespace {

/// A channel of its own for each test, so that no test meets a program that runs. A short
/// one: on macOS the channel is a file in the temporary folder of the test process, which is
/// long (/private/var/folders/.../T/kalahari_tests_<pid>), and the path of such a file has at
/// most 103 bytes there
QString testKey()
{
    return QStringLiteral("kt-") + QUuid::createUuid().toString(QUuid::Id128).left(10);
}

/// A thread joined at the end of the test, also when a REQUIRE ends it before
class JoinedThread {
public:
    template <typename Function>
    explicit JoinedThread(Function function)
        : m_thread(std::move(function))
    {
    }
    ~JoinedThread() { join(); }
    JoinedThread(const JoinedThread&) = delete;
    JoinedThread& operator=(const JoinedThread&) = delete;

    void join()
    {
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

private:
    std::thread m_thread;
};

/// Handle the events of this thread until @p done says so, at most @p timeoutMs
template <typename Done>
void processEventsUntil(Done done, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
}

/// Hand @p path over in another thread, as a later start of the program does, while this
/// thread answers as the program that runs
bool handOverFromLaterStart(const QString& key, const QString& path, int timeoutMs)
{
    std::atomic<bool> done{false};
    bool taken = false;
    std::thread later([&key, &path, &taken, &done, timeoutMs]() {
        ProgramInstance start(key);
        taken = start.handOver(path, timeoutMs);
        done = true;
    });
    processEventsUntil([&done]() { return done.load(); }, timeoutMs + 5000);
    later.join();
    return taken;
}

} // anonymous namespace

TEST_CASE("Later starts hand their requests to the program that runs",
          "[gui][program_instance]")
{
    const QString key = testKey();
    ProgramInstance first(key);
    REQUIRE_FALSE(first.handOver(QString(), 1000));
    REQUIRE(first.listen());
    REQUIRE(first.isListening());

    QStringList requests;
    QObject::connect(&first, &ProgramInstance::requested,
                     [&requests](const QString& path) { requests << path; });

    SECTION("A start with a book: the program that runs opens it")
    {
        const QString book = QStringLiteral("/books/Burza/Burza.klh");
        REQUIRE(handOverFromLaterStart(key, book, 5000));
        processEventsUntil([&requests]() { return !requests.isEmpty(); }, 2000);
        REQUIRE(requests == QStringList{book});
    }

    SECTION("A start without a file: the program that runs only shows its window")
    {
        REQUIRE(handOverFromLaterStart(key, QString(), 5000));
        processEventsUntil([&requests]() { return !requests.isEmpty(); }, 2000);
        REQUIRE(requests == QStringList{QString()});
    }

    SECTION("Two later starts, one after the other")
    {
        REQUIRE(handOverFromLaterStart(key, QStringLiteral("/a.klh"), 5000));
        REQUIRE(handOverFromLaterStart(key, QStringLiteral("/b.txt"), 5000));
        processEventsUntil([&requests]() { return requests.size() == 2; }, 2000);
        REQUIRE(requests == QStringList{QStringLiteral("/a.klh"), QStringLiteral("/b.txt")});
    }
}

TEST_CASE("A start after the program ended runs as the first one", "[gui][program_instance]")
{
    const QString key = testKey();
    {
        ProgramInstance first(key);
        REQUIRE_FALSE(first.handOver(QString(), 1000));
        REQUIRE(first.listen());
    }
    ProgramInstance next(key);
    REQUIRE_FALSE(next.handOver(QString(), 1000));
    REQUIRE(next.listen());
}

TEST_CASE("A start runs on its own when the program that runs does not answer",
          "[gui][program_instance]")
{
    const QString key = testKey();
    ProgramInstance first(key);
    REQUIRE_FALSE(first.handOver(QString(), 1000));

    // The first start does not listen: a later one waits for it, then runs on its own,
    // without the channel
    ProgramInstance later(key);
    QElapsedTimer timer;
    timer.start();
    REQUIRE_FALSE(later.handOver(QString(), 600));
    CHECK(timer.elapsed() >= 500);
    REQUIRE_FALSE(later.listen());
}

TEST_CASE("A later start waits for the program that runs while it is still starting",
          "[gui][program_instance]")
{
    const QString key = testKey();
    ProgramInstance first(key);
    REQUIRE_FALSE(first.handOver(QString(), 1000));
    QStringList requests;
    QObject::connect(&first, &ProgramInstance::requested,
                     [&requests](const QString& path) { requests << path; });

    // The first start still makes its window: it listens some time after the later start came
    std::atomic<bool> done{false};
    bool taken = false;
    JoinedThread later([&key, &taken, &done]() {
        ProgramInstance start(key);
        taken = start.handOver(QStringLiteral("/a.klh"), 10000);
        done = true;
    });
    QThread::msleep(700);
    REQUIRE(first.listen());
    processEventsUntil([&done]() { return done.load(); }, 15000);
    later.join();
    CHECK(taken);
    processEventsUntil([&requests]() { return !requests.isEmpty(); }, 2000);
    CHECK(requests == QStringList{QStringLiteral("/a.klh")});
}

TEST_CASE("A later start is the first one when the program that ran ends while it waits",
          "[gui][program_instance]")
{
    const QString key = testKey();
    std::atomic<bool> locked{false};
    JoinedThread program([&key, &locked]() {
        // A program that does not listen yet, and ends
        ProgramInstance first(key);
        locked = !first.handOver(QString(), 1000);
        QThread::msleep(500);
    });
    QElapsedTimer timer;
    timer.start();
    while (!locked && timer.elapsed() < 5000) {
        QThread::msleep(10);
    }
    REQUIRE(locked);

    ProgramInstance later(key);
    timer.restart();
    CHECK_FALSE(later.handOver(QString(), 10000));
    CHECK(timer.elapsed() < 5000);
    CHECK(later.listen());
    program.join();
}

TEST_CASE("Later starts reach the program that runs also from a long temporary folder",
          "[gui][program_instance]")
{
    // A temporary folder whose path is longer than the one of a local socket can be (on Linux
    // and macOS; on Windows the channel is not a file)
    QTemporaryDir parent;
    REQUIRE(parent.isValid());
    const QString folder = QDir(parent.path()).filePath(QString(120, QLatin1Char('t')));
    REQUIRE(QDir().mkpath(folder));
    struct TemporaryFolder {
        QByteArray before = qgetenv("TMPDIR");
        ~TemporaryFolder() { qputenv("TMPDIR", before); }
    } restore;
    qputenv("TMPDIR", QFile::encodeName(folder));

    const QString key = testKey();
    ProgramInstance first(key);
    REQUIRE_FALSE(first.handOver(QString(), 1000));
    REQUIRE(first.listen());
    QStringList requests;
    QObject::connect(&first, &ProgramInstance::requested,
                     [&requests](const QString& path) { requests << path; });

    REQUIRE(handOverFromLaterStart(key, QStringLiteral("/a.klh"), 5000));
    processEventsUntil([&requests]() { return !requests.isEmpty(); }, 2000);
    CHECK(requests == QStringList{QStringLiteral("/a.klh")});
}

TEST_CASE("The channel is the same for every start of the program", "[gui][program_instance]")
{
    const QString key = ProgramInstance::defaultKey();
    CHECK(key == ProgramInstance::defaultKey());
    CHECK(key.startsWith(QStringLiteral("kalahari-")));
}
