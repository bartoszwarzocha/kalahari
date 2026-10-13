/// @file language_tool_server.cpp
/// @brief The LanguageTool server on this computer: one the program starts and closes, or one
///        started outside it

#include "kalahari/gui/language_tool_server.h"
#include "kalahari/core/logger.h"

#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QTcpServer>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <utility>

namespace kalahari::gui {

namespace {

/// @brief How often the server is asked whether it answers while it starts (ms)
constexpr int PROBE_INTERVAL_MS = 500;

/// @brief How long an answer to that may take (ms)
constexpr int PROBE_TIMEOUT_MS = 3000;

/// @brief How long a server started outside the program may take to answer (ms)
constexpr int EXTERNAL_TIMEOUT_MS = 10000;

/// @brief How many times the server is started again after it stopped by itself
constexpr int MAX_RESTARTS = 3;

/// @brief How many other ports are tried when Java finds the port taken
constexpr int MAX_PORT_TRIES = 10;

/// @brief How many ports from the one given are tried before the system gives one
constexpr int PORT_RANGE = 100;

/// @brief The highest port
constexpr int MAX_PORT = 65535;

/// @brief How much of what Java writes is kept (characters)
constexpr qsizetype MAX_OUTPUT = 8000;

/// @brief How long the program waits for the server to close (ms)
constexpr int CLOSE_TIMEOUT_MS = 3000;

/// @brief The server of LanguageTool, in its folder
constexpr const char* SERVER_JAR = "languagetool-server.jar";

/// @brief Whether Java says the port is taken
bool portTaken(const QString& output) {
    return output.contains(QLatin1String("Address already in use")) ||
           output.contains(QLatin1String("BindException"));
}

}  // namespace

LanguageToolServer::LanguageToolServer(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_probeTimer(new QTimer(this))
    , m_startTimer(new QTimer(this))
{
    // The server is on this computer: it is reached directly, whatever proxy the system has
    m_network->setProxy(QNetworkProxy(QNetworkProxy::NoProxy));

    m_probeTimer->setSingleShot(true);
    m_probeTimer->setInterval(PROBE_INTERVAL_MS);
    connect(m_probeTimer, &QTimer::timeout, this, &LanguageToolServer::probe);

    m_startTimer->setSingleShot(true);
    connect(m_startTimer, &QTimer::timeout, this, [this]() {
        fail(m_external ? Failure::NotRunning : Failure::NotStarted);
    });
}

LanguageToolServer::~LanguageToolServer()
{
    disconnect(m_network, nullptr, this, nullptr);
    closeProcess();
}

// =============================================================================
// Setup
// =============================================================================

void LanguageToolServer::setJava(const QString& java)
{
    if (java != m_java) {
        m_java = java;
        stop();
    }
}

void LanguageToolServer::setFolder(const QString& folder)
{
    if (folder != m_folder) {
        m_folder = folder;
        stop();
    }
}

void LanguageToolServer::setPort(int port)
{
    if (port != m_port) {
        m_port = port;
        stop();
    }
}

void LanguageToolServer::setExternal(bool external)
{
    if (external != m_external) {
        m_external = external;
        stop();
    }
}

void LanguageToolServer::setStartTimeout(int ms)
{
    m_startTimeoutMs = ms;
}

bool LanguageToolServer::canStart() const
{
    if (m_port <= 0 || m_port > MAX_PORT) {
        return false;
    }
    if (m_external) {
        return true;
    }
    return !m_java.isEmpty() && QFileInfo(m_java).isFile() && !m_folder.isEmpty() &&
           QFileInfo(QDir(m_folder).filePath(QLatin1String(SERVER_JAR))).isFile();
}

// =============================================================================
// State
// =============================================================================

QString LanguageToolServer::details() const
{
    return m_output.trimmed();
}

QString LanguageToolServer::address() const
{
    return QStringLiteral("http://localhost:%1").arg(m_port);
}

void LanguageToolServer::start()
{
    if (m_state == State::Starting || m_state == State::Running) {
        return;
    }
    m_failure = Failure::None;
    m_output.clear();
    m_languages.clear();
    m_portTries = 0;

    if (m_external) {
        core::Logger::getInstance().info("LanguageToolServer: Connecting to {}",
                                         address().toStdString());
        setState(State::Starting);
        m_startTimer->start(std::min(EXTERNAL_TIMEOUT_MS, m_startTimeoutMs));
        probe();
        return;
    }
    if (!canStart()) {
        m_output = QStringLiteral("Java: %1\nLanguageTool: %2")
                       .arg(QDir::toNativeSeparators(m_java),
                            QDir::toNativeSeparators(QDir(m_folder).filePath(
                                QLatin1String(SERVER_JAR))));
        fail(Failure::NotStarted);
        return;
    }
    launch(m_port);
}

void LanguageToolServer::stop()
{
    m_probeTimer->stop();
    m_startTimer->stop();
    if (m_probeReply != nullptr) {
        std::exchange(m_probeReply, nullptr)->abort();
    }
    closeProcess();
    m_restarts = 0;
    m_failure = Failure::None;
    setState(State::Stopped);
}

// =============================================================================
// The java program
// =============================================================================

void LanguageToolServer::launch(int fromPort)
{
    // The port the settings give, or a free one when another program took it
    const int port = freePort(fromPort);
    if (port != m_port) {
        core::Logger::getInstance().info("LanguageToolServer: Port {} is taken, using {}",
                                         m_port, port);
        m_port = port;
        emit portChanged(port);
    }

    closeProcess();
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_process->setWorkingDirectory(m_folder);
    connect(m_process, &QProcess::readyRead, this, [this]() {
        m_output += QString::fromLocal8Bit(m_process->readAll());
        if (m_output.size() > MAX_OUTPUT) {
            m_output = m_output.right(MAX_OUTPUT);
        }
    });
    connect(m_process, &QProcess::finished, this, &LanguageToolServer::onFinished);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_output += m_process->errorString();
            fail(Failure::NotStarted);
        }
    });

    core::Logger::getInstance().info("LanguageToolServer: Starting {} on port {}",
                                     m_folder.toStdString(), m_port);
    setState(State::Starting);
    m_startTimer->start(m_startTimeoutMs);
    const QString jar = QDir(m_folder).filePath(QLatin1String(SERVER_JAR));
    m_process->start(m_java, {QStringLiteral("-cp"), jar,
                              QStringLiteral("org.languagetool.server.HTTPServer"),
                              QStringLiteral("--port"), QString::number(m_port)});
    if (m_process != nullptr) {
        m_probeTimer->start();
    }
}

void LanguageToolServer::onFinished()
{
    m_output += QString::fromLocal8Bit(m_process->readAll());
    m_output = m_output.right(MAX_OUTPUT);
    const bool wasRunning = m_state == State::Running;
    std::exchange(m_process, nullptr)->deleteLater();
    m_probeTimer->stop();
    if (m_probeReply != nullptr) {
        std::exchange(m_probeReply, nullptr)->abort();
    }

    // Another program took the port meanwhile: another one
    if (!wasRunning && portTaken(m_output) && m_portTries < MAX_PORT_TRIES) {
        ++m_portTries;
        m_output.clear();
        launch(m_port + 1);
        return;
    }
    if (wasRunning && m_restarts < MAX_RESTARTS) {
        ++m_restarts;
        core::Logger::getInstance().warn("LanguageToolServer: The server stopped, starting it "
                                         "again: {}",
                                         m_output.right(MAX_OUTPUT / 8).toStdString());
        emit restarting();
        launch(m_port);
        return;
    }
    fail(wasRunning ? Failure::Stopped : Failure::NotStarted);
}

void LanguageToolServer::closeProcess()
{
    if (m_process == nullptr) {
        return;
    }
    QProcess* process = std::exchange(m_process, nullptr);
    disconnect(process, nullptr, this, nullptr);
    // The server keeps nothing: it is closed at once
    process->kill();
    process->waitForFinished(CLOSE_TIMEOUT_MS);
    delete process;
}

int LanguageToolServer::freePort(int from)
{
    for (int port = from; port > 0 && port < from + PORT_RANGE && port <= MAX_PORT; ++port) {
        QTcpServer server;
        if (server.listen(QHostAddress::LocalHost, static_cast<quint16>(port))) {
            return port;
        }
    }
    QTcpServer server;
    return server.listen(QHostAddress::LocalHost, 0) ? static_cast<int>(server.serverPort())
                                                     : from;
}

// =============================================================================
// Whether it answers
// =============================================================================

void LanguageToolServer::probe()
{
    if (m_probeReply != nullptr || m_state != State::Starting) {
        return;
    }
    QNetworkRequest request(QUrl(address() + QStringLiteral("/v2/languages")));
    request.setTransferTimeout(PROBE_TIMEOUT_MS);
    QNetworkReply* reply = m_network->get(request);
    m_probeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onProbeReply(reply); });
}

void LanguageToolServer::onProbeReply(QNetworkReply* reply)
{
    reply->deleteLater();
    if (reply != m_probeReply) {
        return;  // dropped
    }
    m_probeReply = nullptr;
    if (m_state != State::Starting) {
        return;
    }

    if (reply->error() == QNetworkReply::NoError) {
        QStringList languages;
        const QJsonArray list = QJsonDocument::fromJson(reply->readAll()).array();
        for (const auto& language : list) {
            const QString code =
                language.toObject().value(QLatin1String("longCode")).toString();
            if (!code.isEmpty()) {
                languages.append(code);
            }
        }
        if (!languages.isEmpty()) {
            m_languages = languages;
            m_startTimer->stop();
            core::Logger::getInstance().info("LanguageToolServer: Running on port {}, {} "
                                             "languages",
                                             m_port, languages.size());
            setState(State::Running);
            return;
        }
    }

    // Something answers that is not LanguageTool: the port is another program's
    const bool answered = reply->error() == QNetworkReply::NoError ||
                          reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid();
    if (answered && m_external) {
        fail(Failure::OtherProgram);
        return;
    }
    // Not answering yet (the server the program starts finds the port taken by itself)
    m_probeTimer->start();
}

// =============================================================================
// Failures
// =============================================================================

void LanguageToolServer::fail(Failure failure)
{
    m_probeTimer->stop();
    m_startTimer->stop();
    if (m_probeReply != nullptr) {
        std::exchange(m_probeReply, nullptr)->abort();
    }
    closeProcess();
    m_failure = failure;
    core::Logger::getInstance().warn("LanguageToolServer: Not running on port {} ({}): {}",
                                     m_port, static_cast<int>(failure),
                                     m_output.right(MAX_OUTPUT / 8).toStdString());
    setState(State::Failed);
}

void LanguageToolServer::setState(State state)
{
    if (state != m_state) {
        m_state = state;
        emit stateChanged(state);
    }
}

}  // namespace kalahari::gui
