/// @file fake_java.cpp
/// @brief A stand-in for Java running the LanguageTool server, for the grammar tests
///
/// Started as LanguageToolServer starts Java (-cp languagetool-server.jar
/// org.languagetool.server.HTTPServer --port N --config FILE), it answers on localhost:N as
/// LanguageTool does: GET /v2/languages with its languages, POST /v2/check with no issues.
/// It says the settings of the file given with --config, one line each, and ends at once when
/// it cannot read that file, as LanguageTool does. The environment tells it to do otherwise:
/// - KALAHARI_FAKE_JAVA_FAIL: it says it cannot load the server and ends at once;
/// - KALAHARI_FAKE_JAVA_TAKEN_PORT: started on that port, it says the port is taken and ends;
/// - KALAHARI_FAKE_JAVA_EXIT_MS: it ends that long after its first answer;
/// - KALAHARI_FAKE_JAVA_EXIT_ONCE: with KALAHARI_FAKE_JAVA_EXIT_MS, it ends only when the file
///   named here is not there yet (it creates it): the next start goes on answering.

#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QHostAddress>
#include <QObject>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <cstdio>
#include <memory>
#include <utility>

namespace {

/// How the program ends when it fails, as Java does
constexpr int FAILURE_EXIT_CODE = 1;

/// The port LanguageTool listens on without --port
constexpr int DEFAULT_PORT = 8081;

/// What LanguageTool answers to a request (its first line)
QByteArray answerTo(const QByteArray& requestLine) {
    const QByteArray languages = QByteArrayLiteral(
        "[{\"name\":\"English (US)\",\"code\":\"en\",\"longCode\":\"en-US\"},"
        "{\"name\":\"Polish\",\"code\":\"pl\",\"longCode\":\"pl-PL\"}]");
    const QByteArray noIssues = QByteArrayLiteral(
        "{\"software\":{\"name\":\"LanguageTool\",\"version\":\"6.6\"},\"matches\":[]}");
    const QByteArray body = requestLine.startsWith("GET /v2/languages") ? languages : noIssues;
    return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
           QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
}

/// Answer a request once it came whole
/// @return Whether it was answered
bool receive(QTcpSocket* socket, QByteArray& request) {
    const qsizetype headerEnd = request.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        return false;
    }
    const QByteArray headers = request.left(headerEnd).toLower();
    const QByteArray key = "content-length:";
    const qsizetype keyAt = headers.indexOf(key);
    qsizetype length = 0;
    if (keyAt >= 0) {
        const qsizetype lineEnd = headers.indexOf("\r\n", keyAt);
        const qsizetype valueAt = keyAt + key.size();
        length = headers.mid(valueAt, lineEnd < 0 ? -1 : lineEnd - valueAt).trimmed().toLongLong();
    }
    if (request.size() < headerEnd + 4 + length) {
        return false;
    }
    const QByteArray line = request.left(request.indexOf("\r\n"));
    request.clear();
    socket->write(answerTo(line));
    socket->disconnectFromHost();
    return true;
}

/// Say something as Java does, at once
void say(const char* text) {
    std::puts(text);
    std::fflush(stdout);
}

}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    const QStringList arguments = QCoreApplication::arguments();
    const qsizetype portAt = arguments.indexOf(QStringLiteral("--port"));
    const int port = portAt >= 0 && portAt + 1 < arguments.size()
                         ? arguments.at(portAt + 1).toInt()
                         : DEFAULT_PORT;

    if (qEnvironmentVariableIsSet("KALAHARI_FAKE_JAVA_FAIL")) {
        say("Error: Could not find or load main class org.languagetool.server.HTTPServer");
        return FAILURE_EXIT_CODE;
    }
    // The settings are read before the server starts; LanguageTool ends without them
    const qsizetype configAt = arguments.indexOf(QStringLiteral("--config"));
    if (configAt >= 0 && configAt + 1 < arguments.size()) {
        QFile config(arguments.at(configAt + 1));
        if (!config.open(QIODevice::ReadOnly)) {
            std::printf("Exception in thread \"main\" java.lang.RuntimeException: Could not load "
                        "properties from '%s'\n",
                        qPrintable(config.fileName()));
            std::fflush(stdout);
            return FAILURE_EXIT_CODE;
        }
        for (const QByteArray& line : config.readAll().split('\n')) {
            if (!line.trimmed().isEmpty()) {
                std::printf("Setting: %s\n", line.trimmed().constData());
            }
        }
    }
    QTcpServer server;
    if (qEnvironmentVariableIntValue("KALAHARI_FAKE_JAVA_TAKEN_PORT") == port ||
        !server.listen(QHostAddress::LocalHost, static_cast<quint16>(port))) {
        say("java.net.BindException: Address already in use");
        return FAILURE_EXIT_CODE;
    }
    // Ending by itself after it answered, as a server that stopped
    const int exitMs = qEnvironmentVariableIntValue("KALAHARI_FAKE_JAVA_EXIT_MS");
    const QString once = qEnvironmentVariable("KALAHARI_FAKE_JAVA_EXIT_ONCE");
    bool exits = exitMs > 0;
    if (exits && !once.isEmpty()) {
        QFile mark(once);
        exits = !mark.exists() && mark.open(QIODevice::WriteOnly);
    }
    bool ending = false;
    const auto answered = [&app, &ending, exits, exitMs]() {
        if (exits && !std::exchange(ending, true)) {
            QTimer::singleShot(exitMs, &app, []() {
                say("The server stopped");
                QCoreApplication::exit(FAILURE_EXIT_CODE);
            });
        }
    };

    QObject::connect(&server, &QTcpServer::newConnection, &server, [&server, answered]() {
        while (QTcpSocket* socket = server.nextPendingConnection()) {
            auto request = std::make_shared<QByteArray>();
            QObject::connect(socket, &QTcpSocket::readyRead, socket,
                             [socket, request, answered]() {
                                 *request += socket->readAll();
                                 if (receive(socket, *request)) {
                                     answered();
                                 }
                             });
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    });
    std::printf("Starting LanguageTool 6.6 server on http://localhost:%d\n", port);
    std::fflush(stdout);
    return QCoreApplication::exec();
}
