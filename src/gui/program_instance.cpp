/// @file program_instance.cpp
/// @brief One program for the writer: a later start hands its request to the program that
/// runs, and ends

#include "kalahari/gui/program_instance.h"
#include "kalahari/core/logger.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDeadlineTimer>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QThread>

#include <algorithm>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX  // std::min and std::max, not the macros of windows.h
#endif
#include <windows.h>
#endif

namespace kalahari {
namespace gui {

namespace {

constexpr int STEP_MS = 200;                ///< One try to reach the program that runs
constexpr qint64 MAX_REQUEST = 64 * 1024;   ///< The longest request read

const QString REQUEST_KEY = QStringLiteral("request");  ///< What is to be done
const QString PATH_KEY = QStringLiteral("path");        ///< The book or the file to open
const QString SHOW_REQUEST = QStringLiteral("show");    ///< Show the window, open the path
const QByteArray TAKEN = QByteArrayLiteral("ok\n");     ///< The answer to a request taken

/// The lock of the channel @p key
QString lockPath(const QString& key)
{
    return QDir(QDir::tempPath()).filePath(key + QStringLiteral(".lock"));
}

/// The channel @p key as a local socket names it: a file in the temporary folder, or in /tmp
/// when that path is too long for a local socket (a temporary folder with a long path)
QString serverName(const QString& key)
{
#ifdef Q_OS_UNIX
    constexpr qsizetype MAX_SOCKET_PATH = 103;  // the longest path of a local socket on macOS
    if (QFile::encodeName(QDir(QDir::tempPath()).filePath(key)).size() > MAX_SOCKET_PATH) {
        return QStringLiteral("/tmp/") + key;
    }
#endif
    return key;
}

/// The time left before @p deadline, for the functions that wait
int remainingMs(const QDeadlineTimer& deadline)
{
    return static_cast<int>(std::max<qint64>(0, deadline.remainingTime()));
}

} // anonymous namespace

ProgramInstance::ProgramInstance(const QString& key, QObject* parent)
    : QObject(parent)
    , m_key(key)
{
}

ProgramInstance::~ProgramInstance()
{
    if (m_server) {
        m_server->close();
    }
}

QString ProgramInstance::defaultKey()
{
    // The writer (their home folder) and the program file: another build has a channel of
    // its own
    QString file = QCoreApplication::applicationFilePath();
#ifdef Q_OS_WIN
    file = file.toCaseFolded();  // the same file, whatever the case of its letters
#endif
    const QByteArray hash = QCryptographicHash::hash(
        (QDir::homePath() + QLatin1Char('|') + file).toUtf8(), QCryptographicHash::Sha256);
    return QStringLiteral("kalahari-") + QString::fromLatin1(hash.toHex().left(16));
}

bool ProgramInstance::handOver(const QString& path, int timeoutMs)
{
    auto& logger = core::Logger::getInstance();

    // The first start holds the lock. A lock is stale only when its program has ended: the
    // first start may run for days.
    m_lock = std::make_unique<QLockFile>(lockPath(m_key));
    m_lock->setStaleLockTime(0);
    if (m_lock->tryLock(0)) {
        return false;
    }
    if (m_lock->error() != QLockFile::LockFailedError) {
        logger.warn("ProgramInstance: the lock cannot be made (error {}), so this start runs on "
                    "its own",
                    static_cast<int>(m_lock->error()));
        m_lock.reset();
        return false;
    }

#ifdef Q_OS_WIN
    // The writer has just made this start, so it may let the program that runs bring its
    // window to the front
    AllowSetForegroundWindow(ASFW_ANY);
#endif

    // Another start holds the lock: it takes the request once its window can, also when it is
    // still starting. When it ends meanwhile, this start is the first one.
    const QByteArray request =
        QJsonDocument(QJsonObject{{REQUEST_KEY, SHOW_REQUEST}, {PATH_KEY, path}})
            .toJson(QJsonDocument::Compact) +
        '\n';
    const QDeadlineTimer deadline(timeoutMs);
    QLocalSocket socket;
    while (!deadline.hasExpired()) {
        if (socket.state() != QLocalSocket::ConnectedState) {
            if (m_lock->tryLock(0)) {
                logger.info("ProgramInstance: the program that ran has ended, so this start is "
                            "the first one");
                return false;
            }
            socket.connectToServer(serverName(m_key));
            if (!socket.waitForConnected(std::min(STEP_MS, remainingMs(deadline)))) {
                socket.abort();
                QThread::msleep(static_cast<unsigned long>(std::min(STEP_MS / 2,
                                                                    remainingMs(deadline))));
                continue;
            }
            socket.write(request);
            socket.waitForBytesWritten(remainingMs(deadline));
        }
        if (!socket.canReadLine()) {
            socket.waitForReadyRead(std::min(STEP_MS, remainingMs(deadline)));
        }
        if (socket.canReadLine()) {
            if (socket.readLine() == TAKEN) {
                logger.info("ProgramInstance: the program that runs took the request");
                return true;
            }
            break;
        }
    }
    m_lock.reset();
    logger.warn("ProgramInstance: the program that runs did not answer, so this start runs on "
                "its own");
    return false;
}

bool ProgramInstance::listen()
{
    auto& logger = core::Logger::getInstance();
    if (!m_lock || !m_lock->isLocked()) {
        return false;
    }

    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    // A channel left by a program that ended without closing it: the lock says none runs
    const QString name = serverName(m_key);
    QLocalServer::removeServer(name);
    if (!m_server->listen(name)) {
        logger.warn("ProgramInstance: the channel cannot be opened ({}), so later starts run on "
                    "their own",
                    m_server->errorString().toStdString());
        delete m_server;
        m_server = nullptr;
        return false;
    }
    connect(m_server, &QLocalServer::newConnection, this, [this]() {
        while (QLocalSocket* socket = m_server->nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, this,
                    [this, socket]() { readRequest(socket); });
            readRequest(socket);  // what came with the connection
        }
    });
    logger.info("ProgramInstance: later starts hand their requests to this one");
    return true;
}

bool ProgramInstance::isListening() const
{
    return m_server && m_server->isListening();
}

void ProgramInstance::readRequest(QLocalSocket* socket)
{
    if (!socket->canReadLine()) {
        if (socket->bytesAvailable() > MAX_REQUEST) {
            socket->abort();
        }
        return;
    }
    const QJsonDocument document = QJsonDocument::fromJson(socket->readLine(MAX_REQUEST));
    const QJsonObject request = document.object();
    if (request.value(REQUEST_KEY).toString() != SHOW_REQUEST) {
        core::Logger::getInstance().warn("ProgramInstance: a request of an unknown kind");
        socket->abort();
        return;
    }

    // The window comes to the front while the later start still runs, as Windows lets it only
    // then; what the window opens comes later, in its turn. Then the answer.
    emit requested(request.value(PATH_KEY).toString());
    socket->write(TAKEN);
    socket->disconnectFromServer();
}

} // namespace gui
} // namespace kalahari
