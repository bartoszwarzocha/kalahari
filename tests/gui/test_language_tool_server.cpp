/// @file test_language_tool_server.cpp
/// @brief The LanguageTool server on this computer: the one the program starts (with the
///        stand-in for Java) and the one started outside it (the stand-in for LanguageTool)

#include <catch2/catch_test_macros.hpp>

#include "../editor/editor_test_utils.h"
#include "../editor/fake_language_tool.h"
#include "grammar_test_utils.h"
#include "kalahari/gui/language_tool_server.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

#include <cstdio>
#include <limits>
#include <string>

using namespace kalahari::gui;
using namespace kalahari::test;

namespace {

/// How long the stand-in for Java may take to answer (ms)
constexpr int START_MS = 20000;

/// What the program started by the test of a program killed says once its server answers,
/// before the port
constexpr const char* PORT_SAID = "LanguageTool server port: ";

/// How long that program waits to be killed before it ends by itself (ms)
constexpr int KILLED_WAIT_MS = 120000;

/// A folder of LanguageTool with its server
class LanguageToolFolder {
public:
    LanguageToolFolder() {
        REQUIRE(m_dir.isValid());
        QFile jar(QDir(m_dir.path()).filePath(QStringLiteral("languagetool-server.jar")));
        REQUIRE(jar.open(QIODevice::WriteOnly));
        jar.write("server");
    }
    QString path() const { return m_dir.path(); }

private:
    QTemporaryDir m_dir;
};

/// A free port on this computer
int freePort() {
    QTcpServer server;
    REQUIRE(server.listen(QHostAddress::LocalHost));
    return server.serverPort();
}

/// The first port nothing listens on here, from the one given, as the server looks for it
int firstFreePort(int from) {
    for (int port = from; port <= std::numeric_limits<quint16>::max(); ++port) {
        QTcpServer server;
        if (server.listen(QHostAddress::LocalHost, static_cast<quint16>(port))) {
            return port;
        }
    }
    return 0;
}

/// Wait until a server is in a state
bool reaches(const LanguageToolServer& server, LanguageToolServer::State state,
             int timeoutMs = START_MS) {
    return waitUntil([&server, state]() { return server.state() == state; }, timeoutMs);
}

/// Where a server got to, for a test that fails
std::string account(const LanguageToolServer& server) {
    return QStringLiteral("state %1, failure %2, port %3: %4")
        .arg(static_cast<int>(server.state()))
        .arg(static_cast<int>(server.failure()))
        .arg(server.port())
        .arg(server.details())
        .toStdString();
}

/// Whether something answers on a port of this computer
bool answers(int port) {
    QTcpSocket socket;
    socket.connectToHost(QHostAddress(QHostAddress::LocalHost), static_cast<quint16>(port));
    return socket.waitForConnected(1000);
}

/// Keep the states a server goes through
void watchStates(LanguageToolServer& server, QList<LanguageToolServer::State>& states) {
    QObject::connect(&server, &LanguageToolServer::stateChanged, &server,
                     [&states](LanguageToolServer::State state) { states.append(state); });
}

}  // namespace

TEST_CASE("LanguageTool server: one started outside the program is connected to",
          "[gui][grammar]") {
    FakeLanguageTool languageTool;
    LanguageToolServer server;
    server.setExternal(true);
    server.setPort(languageTool.port());
    CHECK(server.canStart());
    CHECK(server.address() == QStringLiteral("http://localhost:%1").arg(languageTool.port()));

    server.start();
    CHECK(server.state() == LanguageToolServer::State::Starting);
    REQUIRE(reaches(server, LanguageToolServer::State::Running));
    CHECK(server.languages() == QStringList({"en-US", "pl-PL"}));
    CHECK_FALSE(server.startedByProgram());
    CHECK(languageTool.languageRequests() == 1);

    // Stopped by the program: it is not closed, only not connected to
    server.stop();
    CHECK(server.state() == LanguageToolServer::State::Stopped);
}

TEST_CASE("LanguageTool server: one started outside the program that is not there, or is "
          "another program",
          "[gui][grammar]") {
    LanguageToolServer server;
    server.setExternal(true);
    server.setStartTimeout(1500);

    SECTION("nothing answers on the port") {
        server.setPort(freePort());
        server.start();
        REQUIRE(reaches(server, LanguageToolServer::State::Failed));
        CHECK(server.failure() == LanguageToolServer::Failure::NotRunning);
    }

    SECTION("another program answers") {
        FakeLanguageTool other;
        other.setLanguagesAnswer("<html>Hello</html>");
        server.setPort(other.port());
        server.start();
        REQUIRE(reaches(server, LanguageToolServer::State::Failed));
        CHECK(server.failure() == LanguageToolServer::Failure::OtherProgram);
    }
}

TEST_CASE("LanguageTool server: the program starts it with Java and closes it",
          "[gui][grammar]") {
    LanguageToolFolder folder;
    LanguageToolServer server;
    QList<LanguageToolServer::State> states;
    watchStates(server, states);
    server.setJava(fakeJavaProgram());
    server.setFolder(folder.path());
    server.setPort(freePort());
    CHECK(server.canStart());

    server.start();
    REQUIRE(reaches(server, LanguageToolServer::State::Running));
    CHECK(server.startedByProgram());
    CHECK(server.languages() == QStringList({"en-US", "pl-PL"}));
    CHECK(states == QList<LanguageToolServer::State>({LanguageToolServer::State::Starting,
                                                      LanguageToolServer::State::Running}));
    CHECK(server.details().contains(QStringLiteral("Starting LanguageTool")));
    // What it checks a language with is kept from one text to the next: some 20 ms for a
    // paragraph instead of 150
    CHECK(server.details().contains(QStringLiteral("Setting: pipelineCaching=true")));

    // Started again while it runs: nothing happens
    server.start();
    CHECK(server.state() == LanguageToolServer::State::Running);

    // A change of its setup stops it
    server.setPort(server.port() + 1);
    CHECK(server.state() == LanguageToolServer::State::Stopped);
    server.start();
    REQUIRE(reaches(server, LanguageToolServer::State::Running));
    server.stop();
    CHECK(server.state() == LanguageToolServer::State::Stopped);
}

#if !defined(Q_OS_WIN)
TEST_CASE("LanguageTool server: started also when the temporary folder does not exist",
          "[gui][grammar]") {
    LanguageToolFolder folder;
    // The settings file would land in the working folder of the program, and Qt may give its
    // path as relative, which Java, started in the LanguageTool folder, does not find: the
    // server starts without it and checks slower
    const ScopedEnvironment temporary("TMPDIR",
                                      QDir(folder.path()).filePath(QStringLiteral("no")).toUtf8());
    LanguageToolServer server;
    server.setJava(fakeJavaProgram());
    server.setFolder(folder.path());
    server.setPort(freePort());

    server.start();
    const bool running = reaches(server, LanguageToolServer::State::Running);
    INFO(account(server));
    REQUIRE(running);
    CHECK_FALSE(server.details().contains(QStringLiteral("Setting:")));
}
#endif

TEST_CASE("LanguageTool server: nothing to start, or Java that does not start it",
          "[gui][grammar]") {
    LanguageToolFolder folder;
    LanguageToolServer server;
    server.setPort(freePort());

    SECTION("no java program or no folder: it says what is missing") {
        CHECK_FALSE(server.canStart());
        server.setFolder(folder.path());
        CHECK_FALSE(server.canStart());
        server.start();
        CHECK(server.state() == LanguageToolServer::State::Failed);
        CHECK(server.failure() == LanguageToolServer::Failure::NotStarted);
        CHECK(server.details().contains(QStringLiteral("languagetool-server.jar")));
    }

    SECTION("Java says why") {
        ScopedEnvironment fail("KALAHARI_FAKE_JAVA_FAIL", "1");
        server.setJava(fakeJavaProgram());
        server.setFolder(folder.path());
        server.start();
        REQUIRE(reaches(server, LanguageToolServer::State::Failed));
        CHECK(server.failure() == LanguageToolServer::Failure::NotStarted);
        CHECK(server.details().contains(QStringLiteral("Could not find or load main class")));
    }

    SECTION("it does not answer in time") {
        server.setJava(fakeJavaProgram());
        server.setFolder(folder.path());
        server.setStartTimeout(1);
        server.start();
        REQUIRE(reaches(server, LanguageToolServer::State::Failed));
        CHECK(server.failure() == LanguageToolServer::Failure::NotStarted);
    }
}

TEST_CASE("LanguageTool server: a port taken by another program, another one is taken",
          "[gui][grammar]") {
    LanguageToolFolder folder;
    LanguageToolServer server;
    QList<int> ports;
    QObject::connect(&server, &LanguageToolServer::portChanged, &server,
                     [&ports](int port) { ports.append(port); });
    server.setJava(fakeJavaProgram());
    server.setFolder(folder.path());

    SECTION("taken before the start") {
        QTcpServer other;
        REQUIRE(other.listen(QHostAddress::LocalHost));
        server.setPort(other.serverPort());
        server.start();
        const bool running = reaches(server, LanguageToolServer::State::Running);
        INFO(account(server));
        REQUIRE(running);
        // A test running alongside may take the free port before Java binds it: the server
        // then takes the next one
        REQUIRE_FALSE(ports.isEmpty());
        CHECK_FALSE(ports.contains(other.serverPort()));
        CHECK(server.port() == ports.last());
    }

    SECTION("taken before the start, and the next free one before Java binds it") {
        QTcpServer other;
        REQUIRE(other.listen(QHostAddress::LocalHost));
        const int next = firstFreePort(other.serverPort());
        REQUIRE(next > 0);
        ScopedEnvironment taken("KALAHARI_FAKE_JAVA_TAKEN_PORT", QByteArray::number(next));
        server.setPort(other.serverPort());
        server.start();
        const bool running = reaches(server, LanguageToolServer::State::Running);
        INFO(account(server));
        REQUIRE(running);
        REQUIRE_FALSE(ports.isEmpty());
        CHECK_FALSE(ports.contains(other.serverPort()));
        CHECK(server.port() != next);
        CHECK(server.port() == ports.last());
    }

    SECTION("taken while Java starts it") {
        const int port = freePort();
        ScopedEnvironment taken("KALAHARI_FAKE_JAVA_TAKEN_PORT", QByteArray::number(port));
        server.setPort(port);
        server.start();
        const bool running = reaches(server, LanguageToolServer::State::Running);
        INFO(account(server));
        REQUIRE(running);
        REQUIRE_FALSE(ports.isEmpty());
        CHECK(server.port() != port);
    }
    server.stop();
}

TEST_CASE("LanguageTool server: stopped by itself, it is started again a few times",
          "[gui][grammar]") {
    LanguageToolFolder folder;
    LanguageToolServer server;
    int restarts = 0;
    QObject::connect(&server, &LanguageToolServer::restarting, &server,
                     [&restarts]() { ++restarts; });
    server.setJava(fakeJavaProgram());
    server.setFolder(folder.path());
    server.setPort(freePort());
    ScopedEnvironment exits("KALAHARI_FAKE_JAVA_EXIT_MS", "300");

    SECTION("once: it runs again") {
        QTemporaryDir marks;
        const QString mark = QDir(marks.path()).filePath(QStringLiteral("stopped"));
        ScopedEnvironment once("KALAHARI_FAKE_JAVA_EXIT_ONCE", mark.toLocal8Bit());
        server.start();
        REQUIRE(waitUntil([&restarts]() { return restarts == 1; }, START_MS));
        REQUIRE(reaches(server, LanguageToolServer::State::Running));
        runEventLoop(800);
        CHECK(restarts == 1);
        CHECK(server.state() == LanguageToolServer::State::Running);
        server.stop();
    }

    SECTION("every time: it fails at last") {
        server.start();
        REQUIRE(reaches(server, LanguageToolServer::State::Failed, 4 * START_MS));
        CHECK(restarts == 3);
        CHECK(server.failure() == LanguageToolServer::Failure::Stopped);
        CHECK(server.details().contains(QStringLiteral("The server stopped")));
    }
}

TEST_CASE("LanguageTool server: started by the program the test below kills",
          "[.languagetool-killed]") {
    // A program of its own: the test below starts it and kills it once the server answers
    LanguageToolFolder folder;
    LanguageToolServer server;
    server.setJava(fakeJavaProgram());
    server.setFolder(folder.path());
    server.setPort(freePort());
    server.start();
    const bool running = reaches(server, LanguageToolServer::State::Running);
    INFO(account(server));
    REQUIRE(running);
    std::printf("%s%d\n", PORT_SAID, server.port());
    std::fflush(stdout);
    runEventLoop(KILLED_WAIT_MS);
}

TEST_CASE("LanguageTool server: Java ends with the program that started it, also when the "
          "program is killed",
          "[gui][grammar]") {
    QProcess program;
    program.start(QCoreApplication::applicationFilePath(),
                  {QStringLiteral("[.languagetool-killed]")});
    QByteArray said;
    int port = 0;
    waitUntil(
        [&program, &said, &port]() {
            said += program.readAllStandardOutput();
            const qsizetype at = said.indexOf(PORT_SAID);
            const qsizetype end = at < 0 ? -1 : said.indexOf('\n', at);
            if (end >= 0) {
                const qsizetype from = at + qstrlen(PORT_SAID);
                port = said.mid(from, end - from).trimmed().toInt();
            }
            return port > 0 || program.state() == QProcess::NotRunning;
        },
        3 * START_MS);
    INFO(said.toStdString());
    REQUIRE(port > 0);
    REQUIRE(answers(port));

    // As a crash: nothing of the program closes the server
    program.kill();
    REQUIRE(program.waitForFinished(START_MS));
    CHECK(waitUntil([port]() { return !answers(port); }, START_MS));
}
