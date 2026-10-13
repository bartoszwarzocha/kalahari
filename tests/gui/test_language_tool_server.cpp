/// @file test_language_tool_server.cpp
/// @brief The LanguageTool server on this computer: the one the program starts (with the
///        stand-in for Java) and the one started outside it (the stand-in for LanguageTool)

#include <catch2/catch_test_macros.hpp>

#include "../editor/editor_test_utils.h"
#include "../editor/fake_language_tool.h"
#include "grammar_test_utils.h"
#include "kalahari/gui/language_tool_server.h"

#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QTcpServer>
#include <QTemporaryDir>

using namespace kalahari::gui;
using namespace kalahari::test;

namespace {

/// How long the stand-in for Java may take to answer (ms)
constexpr int START_MS = 20000;

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

/// Wait until a server is in a state
bool reaches(const LanguageToolServer& server, LanguageToolServer::State state,
             int timeoutMs = START_MS) {
    return waitUntil([&server, state]() { return server.state() == state; }, timeoutMs);
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
        REQUIRE(reaches(server, LanguageToolServer::State::Running));
        REQUIRE(ports.size() == 1);
        CHECK(ports.first() != other.serverPort());
        CHECK(server.port() == ports.first());
    }

    SECTION("taken while Java starts it") {
        const int port = freePort();
        ScopedEnvironment taken("KALAHARI_FAKE_JAVA_TAKEN_PORT", QByteArray::number(port));
        server.setPort(port);
        server.start();
        REQUIRE(reaches(server, LanguageToolServer::State::Running));
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
