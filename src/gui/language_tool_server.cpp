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
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <utility>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(Q_OS_LINUX)
#include <csignal>
#include <sys/prctl.h>
#include <unistd.h>
#elif defined(Q_OS_MACOS)
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <sys/event.h>
#include <unistd.h>
#endif

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

/// @brief The settings the server is started with: what it checks a language with is kept
///        from one text to the next, so a paragraph is checked in some 20 ms instead of 150
constexpr const char* SERVER_SETTINGS = "pipelineCaching=true\n"
                                        "maxPipelinePoolSize=10\n"
                                        "pipelineExpireTimeInSeconds=1800\n";

/// @brief Whether Java says the port is taken
bool portTaken(const QString& output) {
    return output.contains(QLatin1String("Address already in use")) ||
           output.contains(QLatin1String("BindException"));
}

// =============================================================================
// Java ends with the program, also when the program ends abruptly (a crash): it does not
// stay in memory
// =============================================================================

#if defined(Q_OS_WIN)

/// @brief The job the java programs are put in. Its handle is never closed: Windows closes
///        it when the program ends, however it ends, and ends the java programs with it.
HANDLE javaJob() {
    static const HANDLE job = []() -> HANDLE {
        const HANDLE created = CreateJobObjectW(nullptr, nullptr);
        if (created == nullptr) {
            return nullptr;
        }
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (SetInformationJobObject(created, JobObjectExtendedLimitInformation, &limits,
                                    static_cast<DWORD>(sizeof(limits))) == FALSE) {
            CloseHandle(created);
            return nullptr;
        }
        return created;
    }();
    return job;
}

/// @brief Have the java program just started end with the program
/// @return Whether it will
bool endWithProgram(const QProcess& process) {
    const HANDLE job = javaJob();
    const qint64 id = process.processId();
    if (job == nullptr || id <= 0) {
        return false;
    }
    const HANDLE java =
        OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(id));
    if (java == nullptr) {
        return false;
    }
    const bool assigned = AssignProcessToJobObject(job, java) != FALSE;
    CloseHandle(java);
    return assigned;
}

#elif defined(Q_OS_LINUX)

/// @brief Run in the java program before Java starts in it, so only what is safe after
///        fork(): the system ends it when the program ends, however it ends (when the thread
///        that starts it ends: the main one)
/// @param program The program that starts it
void endWithProgram(pid_t program) {
    prctl(PR_SET_PDEATHSIG, static_cast<unsigned long>(SIGKILL));
    if (getppid() != program) {
        _exit(1);  // the program ended before that
    }
}

#elif defined(Q_OS_MACOS)

/// @brief The descriptors the watcher closes: those it got of the program
constexpr long MAX_WATCHER_DESCRIPTORS = 65536;

/// @brief Run in the java program before Java starts in it, so only what is safe after
///        fork(). A watcher (a copy of the program that runs none of its code) waits for the
///        program or Java to end; when the program ends first, however it ends, it ends Java.
/// @param program The program that starts it
void endWithProgram(pid_t program) {
    const pid_t java = getpid();
    if (fork() != 0) {
        return;  // Java (not watched when the watcher could not be made)
    }

    // Nothing of the program kept open: Java's output, the pipe that tells it started
    long descriptors = sysconf(_SC_OPEN_MAX);
    if (descriptors <= 0 || descriptors > MAX_WATCHER_DESCRIPTORS) {
        descriptors = MAX_WATCHER_DESCRIPTORS;
    }
    for (int descriptor = 0; descriptor < descriptors; ++descriptor) {
        close(descriptor);
    }

    const int queue = kqueue();
    struct kevent change {};
    EV_SET(&change, java, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, nullptr);
    if (queue < 0 || kevent(queue, &change, 1, nullptr, 0, nullptr) < 0) {
        _exit(0);
    }
    EV_SET(&change, program, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, nullptr);
    if (kevent(queue, &change, 1, nullptr, 0, nullptr) == 0) {
        struct kevent ended {};
        int count = 0;
        do {
            count = kevent(queue, nullptr, 0, &ended, 1, nullptr);
        } while (count < 0 && errno == EINTR);
        if (count != 1 || ended.ident != static_cast<std::uintptr_t>(program)) {
            _exit(0);  // Java ended first
        }
    } else if (errno != ESRCH) {
        _exit(0);  // the program cannot be watched
    }
    // The program ended: Java with it, while Java is there (the watcher's parent)
    if (getppid() == java) {
        kill(java, SIGKILL);
    }
    _exit(0);
}

#endif

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
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    m_process->setChildProcessModifier([program = getpid()]() { endWithProgram(program); });
#endif
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
    QStringList arguments{QStringLiteral("-cp"), jar,
                          QStringLiteral("org.languagetool.server.HTTPServer"),
                          QStringLiteral("--port"), QString::number(m_port)};
    const QString settings = settingsFile();
    if (!settings.isEmpty()) {
        arguments << QStringLiteral("--config") << settings;
    }
    m_process->start(m_java, arguments);
    if (m_process != nullptr) {
#if defined(Q_OS_WIN)
        if (!endWithProgram(*m_process)) {
            core::Logger::getInstance().warn("LanguageToolServer: Java may not end when the "
                                             "program ends abruptly");
        }
#endif
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

QString LanguageToolServer::settingsFile()
{
    if (m_settings == nullptr) {
        // Java starts in the LanguageTool folder: a relative path would not reach the file, and
        // the temporary folder's path is empty when that folder does not exist
        const QString folder = QDir::tempPath();
        if (!QDir::isAbsolutePath(folder)) {
            core::Logger::getInstance().warn("LanguageToolServer: No settings file, the server "
                                             "checks slower: no temporary folder");
            return QString();
        }
        const QString name = QStringLiteral("kalahari-languagetool-XXXXXX.properties");
        auto* file = new QTemporaryFile(QDir(folder).filePath(name), this);
        if (!file->open() || file->write(SERVER_SETTINGS) < 0) {
            core::Logger::getInstance().warn("LanguageToolServer: No settings file, the server "
                                             "checks slower: {}",
                                             file->errorString().toStdString());
            delete file;
            return QString();
        }
        file->close();
        m_settings = file;
    }
    return m_settings->fileName();
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
    // Every probe on a new connection: one that went wrong is not used again. On macOS the
    // first one can reach itself over IPv6 (a port next to the last one the system gave is
    // the one it gives next); kept, it would try only IPv6, where nothing listens.
    request.setAttribute(QNetworkRequest::ConnectionCacheExpiryTimeoutSecondsAttribute, 0);
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
