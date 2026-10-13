/// @file grammar_setup.cpp
/// @brief Preparing grammar as you type: Java and LanguageTool downloaded, checked, unpacked
///        and started once

#include "kalahari/gui/grammar_setup.h"
#include "kalahari/core/logger.h"
#include "kalahari/editor/grammar_check_service.h"
#include "kalahari/gui/language_tool_server.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkProxyFactory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStorageInfo>
#include <QTemporaryFile>
#include <QThread>

#include <zip.h>

#include <algorithm>
#include <utility>

namespace kalahari::gui {

namespace {

/// @brief How long a download may go without data (ms)
constexpr int STALL_TIMEOUT_MS = 60000;

/// @brief Below this much free room (bytes) a failed write is taken for a full disk
constexpr qint64 LOW_ROOM_BYTES = 16 * 1024 * 1024;

/// @brief How much of a file is unpacked at a time (bytes)
constexpr qsizetype UNPACK_CHUNK = 256 * 1024;

/// @brief How often the unpacking looks whether it is broken off (ms)
constexpr int CANCEL_POLL_MS = 200;

/// @brief How long tar may take to start (ms)
constexpr int TAR_START_TIMEOUT_MS = 10000;

/// @brief How many times a folder unpacked is moved to its place (an antivirus may hold it)
constexpr int MOVE_ATTEMPTS = 10;

/// @brief How long between those (ms)
constexpr unsigned long MOVE_RETRY_MS = 300;

/// @brief The text checked at the first run
constexpr const char* TRIAL_TEXT = "Test.";

/// @brief The language of the first run when the server does not check the book's
constexpr const char* TRIAL_FALLBACK_LANGUAGE = "en-US";

/// @brief The bits of a Unix mode in the external attributes of a zip entry
constexpr int UNIX_MODE_SHIFT = 16;
constexpr zip_uint32_t UNIX_TYPE_MASK = 0170000;
constexpr zip_uint32_t UNIX_SYMLINK = 0120000;
constexpr zip_uint32_t UNIX_EXECUTABLE = 0100;

/// @brief The downloads go through the proxy of the system, as in a web browser
class SystemProxyFactory : public QNetworkProxyFactory {
public:
    QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery& query) override {
        return QNetworkProxyFactory::systemProxyForQuery(query);
    }
};

/// @brief What an unpacking ended with
struct UnpackResult {
    GrammarSetup::Failure failure{GrammarSetup::Failure::None};
    QString errorText;
    bool cancelled{false};
};

UnpackResult failed(GrammarSetup::Failure failure, const QString& errorText) {
    UnpackResult result;
    result.failure = failure;
    result.errorText = errorText;
    return result;
}

UnpackResult cancelled() {
    UnpackResult result;
    result.cancelled = true;
    return result;
}

/// @brief A failed write: a full disk or another failure
UnpackResult writeFailed(const QString& folder, const QFile& file) {
    const bool full = QStorageInfo(folder).bytesAvailable() < LOW_ROOM_BYTES ||
                      file.error() == QFileDevice::ResourceError;
    return failed(full ? GrammarSetup::Failure::NoRoom : GrammarSetup::Failure::NotUnpacked,
                  file.fileName() + QStringLiteral(": ") + file.errorString());
}

/// @brief Whether the name of an entry stays in the folder it is unpacked into
bool safeEntry(const QString& name) {
    if (name.isEmpty() || name.startsWith(QLatin1Char('/')) || name.contains(QLatin1Char(':'))) {
        return false;
    }
    const QStringList parts = name.split(QLatin1Char('/'));
    return !parts.contains(QStringLiteral(".."));
}

/// @brief Unpack a zip file into a folder
UnpackResult extractZip(const QString& archive, const QString& target,
                        const std::atomic_bool& cancel) {
    // libzip takes the names in UTF-8, also on Windows
    int error = 0;
    zip_t* zip = zip_open(archive.toUtf8().constData(), ZIP_RDONLY, &error);
    if (zip == nullptr) {
        zip_error_t zipError;
        zip_error_init_with_code(&zipError, error);
        const QString text = QString::fromUtf8(zip_error_strerror(&zipError));
        zip_error_fini(&zipError);
        return failed(GrammarSetup::Failure::NotUnpacked, archive + QStringLiteral(": ") + text);
    }

    const QDir root(target);
    QByteArray buffer(UNPACK_CHUNK, Qt::Uninitialized);
    const zip_int64_t count = zip_get_num_entries(zip, 0);
    UnpackResult result;
    for (zip_int64_t index = 0; index < count && result.failure == GrammarSetup::Failure::None;
         ++index) {
        if (cancel) {
            result = cancelled();
            break;
        }
        QString name = QString::fromUtf8(zip_get_name(zip, index, ZIP_FL_ENC_GUESS));
        name.replace(QLatin1Char('\\'), QLatin1Char('/'));
        if (!safeEntry(name)) {
            result = failed(GrammarSetup::Failure::NotUnpacked,
                            archive + QStringLiteral(": ") + name);
            break;
        }
        const QString path = root.filePath(name);
        if (name.endsWith(QLatin1Char('/'))) {
            QDir().mkpath(path);
            continue;
        }

        zip_uint8_t system = 0;
        zip_uint32_t attributes = 0;
        zip_file_get_external_attributes(zip, static_cast<zip_uint64_t>(index), 0, &system,
                                         &attributes);
        const zip_uint32_t mode = system == ZIP_OPSYS_UNIX ? attributes >> UNIX_MODE_SHIFT : 0;
        if ((mode & UNIX_TYPE_MASK) == UNIX_SYMLINK) {
            continue;  // no links: nothing points out of the folder
        }

        QDir().mkpath(QFileInfo(path).path());
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            result = writeFailed(target, file);
            break;
        }
        zip_file_t* entry = zip_fopen_index(zip, static_cast<zip_uint64_t>(index), 0);
        if (entry == nullptr) {
            result = failed(GrammarSetup::Failure::NotUnpacked,
                            archive + QStringLiteral(": ") +
                                QString::fromUtf8(zip_strerror(zip)));
            break;
        }
        zip_int64_t read = 0;
        while ((read = zip_fread(entry, buffer.data(), static_cast<zip_uint64_t>(buffer.size()))) >
               0) {
            if (file.write(buffer.constData(), read) != read) {
                result = writeFailed(target, file);
                break;
            }
            if (cancel) {
                result = cancelled();
                break;
            }
        }
        if (read < 0 && result.failure == GrammarSetup::Failure::None && !result.cancelled) {
            result = failed(GrammarSetup::Failure::NotUnpacked,
                            name + QStringLiteral(": ") +
                                QString::fromUtf8(zip_file_strerror(entry)));
        }
        zip_fclose(entry);
        if (result.failure == GrammarSetup::Failure::None && !result.cancelled &&
            !file.flush()) {
            result = writeFailed(target, file);
        }
        file.close();
        if (result.cancelled) {
            break;
        }
        if ((mode & UNIX_EXECUTABLE) != 0) {
            file.setPermissions(file.permissions() | QFileDevice::ExeOwner |
                                QFileDevice::ExeUser | QFileDevice::ExeGroup |
                                QFileDevice::ExeOther);
        }
    }
    zip_close(zip);
    return result;
}

/// @brief Unpack a tar.gz file into a folder (the tar of the system: Linux, macOS)
UnpackResult extractTarGz(const QString& archive, const QString& target,
                          const std::atomic_bool& cancel) {
    QProcess tar;
    tar.setProcessChannelMode(QProcess::MergedChannels);
    tar.start(QStringLiteral("tar"), {QStringLiteral("-xzf"), archive, QStringLiteral("-C"),
                                      target});
    if (!tar.waitForStarted(TAR_START_TIMEOUT_MS)) {
        return failed(GrammarSetup::Failure::NotUnpacked,
                      QStringLiteral("tar: ") + tar.errorString());
    }
    while (!tar.waitForFinished(CANCEL_POLL_MS)) {
        if (tar.state() == QProcess::NotRunning) {
            break;
        }
        if (cancel) {
            tar.kill();
            tar.waitForFinished();
            return cancelled();
        }
    }
    if (tar.exitStatus() != QProcess::NormalExit || tar.exitCode() != 0) {
        const QString output = QString::fromLocal8Bit(tar.readAll()).trimmed();
        const bool full = QStorageInfo(target).bytesAvailable() < LOW_ROOM_BYTES ||
                          output.contains(QLatin1String("No space left"));
        return failed(full ? GrammarSetup::Failure::NoRoom : GrammarSetup::Failure::NotUnpacked,
                      QStringLiteral("tar: ") + output);
    }
    return {};
}

/// @brief Unpack a file into its folder, in place of what was there, and remove the file
UnpackResult unpackInto(const QString& archive, const QString& target,
                        const std::atomic_bool& cancel) {
    const QString part = target + QStringLiteral(".part");
    QDir(part).removeRecursively();
    if (!QDir().mkpath(part)) {
        return failed(GrammarSetup::Failure::NoFolder, QDir::toNativeSeparators(part));
    }
    UnpackResult result = archive.endsWith(QLatin1String(".zip"))
                              ? extractZip(archive, part, cancel)
                              : extractTarGz(archive, part, cancel);
    if (result.failure != GrammarSetup::Failure::None || result.cancelled) {
        QDir(part).removeRecursively();
        return result;
    }

    // The files are in one folder of the file (LanguageTool-6.6, jdk-21.0.5+11-jre)
    const QDir unpackedRoot(part);
    const QStringList folders = unpackedRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    const QStringList files = unpackedRoot.entryList(QDir::Files | QDir::Hidden);
    const QString unpacked =
        folders.size() == 1 && files.isEmpty() ? unpackedRoot.filePath(folders.first()) : part;
    QDir(target).removeRecursively();
    bool moved = false;
    for (int attempt = 0; attempt < MOVE_ATTEMPTS && !moved; ++attempt) {
        if (attempt > 0) {
            QThread::msleep(MOVE_RETRY_MS);
        }
        moved = QDir().rename(unpacked, target);
    }
    QDir(part).removeRecursively();
    if (!moved) {
        return failed(GrammarSetup::Failure::NotUnpacked,
                      QDir::toNativeSeparators(unpacked) + QStringLiteral(" -> ") +
                          QDir::toNativeSeparators(target));
    }
    QFile::remove(archive);
    return result;
}

}  // namespace

GrammarSetup::GrammarSetup(LanguageToolServer* server, QObject* parent)
    : QObject(parent)
    , m_server(server)
    , m_network(new QNetworkAccessManager(this))
    , m_javaInfoUrl(GrammarComponents::javaInfoUrl())
    , m_languageToolDownload(GrammarComponents::languageToolDownload())
{
    m_network->setProxyFactory(new SystemProxyFactory());
    m_total = {GrammarComponents::JAVA_DOWNLOAD_BYTES, m_languageToolDownload.size};
}

GrammarSetup::~GrammarSetup()
{
    if (m_cancelUnpack) {
        *m_cancelUnpack = true;
    }
    if (m_unpacker != nullptr) {
        disconnect(m_unpacker, nullptr, this, nullptr);
        m_unpacker->wait();
        delete m_unpacker;
    }
    if (m_reply != nullptr) {
        disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
    }
    dropPartFile();
}

void GrammarSetup::setJavaInfoUrl(const QUrl& url)
{
    m_javaInfoUrl = url;
}

void GrammarSetup::setLanguageToolDownload(const GrammarDownload& download)
{
    m_languageToolDownload = download;
    m_total[1] = download.size;
}

void GrammarSetup::setJavaDownload(const GrammarDownload& download)
{
    m_javaDownload = download;
    m_total[0] = download.size;
}

void GrammarSetup::setLanguage(const QString& language)
{
    m_language = language;
}

// =============================================================================
// Before the preparation
// =============================================================================

qint64 GrammarSetup::freeBytes()
{
    return QStorageInfo(existingFolder()).bytesAvailable();
}

QString GrammarSetup::disk()
{
    QString root = QDir::toNativeSeparators(QStorageInfo(existingFolder()).rootPath());
#ifdef Q_OS_WIN
    if (root.endsWith(QLatin1Char('\\'))) {
        root.chop(1);
    }
#endif
    return root;
}

QString GrammarSetup::existingFolder()
{
    // The folder, or the nearest one there is
    QString path = GrammarComponents::folder();
    while (!QFileInfo::exists(path)) {
        const QString parent = QFileInfo(path).path();
        if (parent == path) {
            break;
        }
        path = parent;
    }
    return path;
}

bool GrammarSetup::canWrite()
{
    const QString folder = GrammarComponents::folder();
    const bool existed = QFileInfo::exists(folder);
    if (!QDir().mkpath(folder)) {
        return false;
    }
    bool writable = false;
    {
        // Removed with the object, before the folder (on Linux it never has a name)
        QTemporaryFile file(QDir(folder).filePath(QStringLiteral("write-XXXXXX.tmp")));
        writable = file.open();
    }
    if (!existed) {
        QDir().rmdir(folder);
    }
    return writable;
}

// =============================================================================
// The preparation
// =============================================================================

void GrammarSetup::start()
{
    if (m_state == State::Running) {
        return;
    }
    // An unpacking broken off ends before the files are touched again
    if (m_unpacker != nullptr) {
        disconnect(m_unpacker, nullptr, this, nullptr);
        m_unpacker->wait();
        std::exchange(m_unpacker, nullptr)->deleteLater();
        m_cancelUnpack.reset();
    }
    m_state = State::Running;
    m_failure = Failure::None;
    m_errorText.clear();
    m_errorUrl.clear();
    m_done = {};
    m_received = {};
    core::Logger::getInstance().info("GrammarSetup: Preparing grammar in {}",
                                     GrammarComponents::folder().toStdString());
    if (!QDir().mkpath(GrammarComponents::archiveFolder())) {
        fail(Failure::NoFolder, QDir::toNativeSeparators(GrammarComponents::folder()));
        return;
    }
    next();
}

void GrammarSetup::cancel()
{
    if (m_state != State::Running) {
        return;
    }
    core::Logger::getInstance().info("GrammarSetup: Cancelled");
    end(State::Cancelled);
}

bool GrammarSetup::isDone(Step step) const
{
    return m_done.at(static_cast<std::size_t>(step));
}

qint64 GrammarSetup::received(Step step) const
{
    const auto index = static_cast<std::size_t>(step);
    return index < m_received.size() ? m_received.at(index) : 0;
}

qint64 GrammarSetup::total(Step step) const
{
    const auto index = static_cast<std::size_t>(step);
    return index < m_total.size() ? m_total.at(index) : 0;
}

int GrammarSetup::percent() const
{
    const qint64 total = m_total[0] + m_total[1];
    if (total <= 0) {
        return 0;
    }
    const qint64 received =
        std::min(m_received[0], m_total[0]) + std::min(m_received[1], m_total[1]);
    return static_cast<int>(received * 100 / total);
}

void GrammarSetup::next()
{
    if (m_state != State::Running) {
        return;
    }
    const bool javaUnpacked =
        GrammarComponents::inspectJava(GrammarComponents::downloadedJava()).feature >=
        GrammarComponents::MIN_JAVA_VERSION;
    const bool languageToolUnpacked = GrammarComponents::languageToolDownloaded();

    if (javaUnpacked || !GrammarComponents::javaArchive().isEmpty()) {
        m_done[0] = true;
        m_received[0] = m_total[0];
    }
    if (languageToolUnpacked || !GrammarComponents::languageToolArchive().isEmpty()) {
        m_done[1] = true;
        m_received[1] = m_total[1];
    }
    if (!m_done[0]) {
        m_step = Step::Java;
        emit progress();
        // The service is asked once: the Java it told is downloaded again after a failure
        if (m_javaDownload.url.isEmpty()) {
            requestJava();
        } else {
            download(m_javaDownload);
        }
    } else if (!m_done[1]) {
        m_step = Step::LanguageTool;
        emit progress();
        download(m_languageToolDownload);
    } else if (!javaUnpacked || !languageToolUnpacked) {
        m_step = Step::Unpack;
        emit progress();
        unpack();
    } else {
        m_done[2] = true;
        m_step = Step::FirstRun;
        emit progress();
        firstRun();
    }
}

// =============================================================================
// Downloads
// =============================================================================

void GrammarSetup::requestJava()
{
    QNetworkRequest request(m_javaInfoUrl);
    request.setTransferTimeout(STALL_TIMEOUT_MS);
    QNetworkReply* reply = m_network->get(request);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply != m_reply) {
            return;
        }
        m_reply = nullptr;
        if (m_state != State::Running) {
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            failDownload(reply);
            return;
        }
        m_javaDownload = GrammarComponents::javaDownloadFrom(reply->readAll());
        if (m_javaDownload.url.isEmpty()) {
            fail(Failure::ServerDown, QStringLiteral("No Java %1 for this system")
                                          .arg(GrammarComponents::JAVA_VERSION),
                 m_javaInfoUrl);
            return;
        }
        if (m_javaDownload.size > 0) {
            m_total[0] = m_javaDownload.size;
        }
        download(m_javaDownload);
    });
}

void GrammarSetup::download(const GrammarDownload& file)
{
    m_download = file;
    m_writeError.clear();
    m_hash.reset();
    const QString part = QDir(GrammarComponents::archiveFolder())
                             .filePath(file.fileName + QStringLiteral(".part"));
    m_file = std::make_unique<QFile>(part);
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString error = m_file->fileName() + QStringLiteral(": ") + m_file->errorString();
        m_file.reset();
        fail(Failure::NoFolder, error);
        return;
    }
    core::Logger::getInstance().info("GrammarSetup: Downloading {}",
                                     file.url.toString().toStdString());
    QNetworkRequest request(file.url);
    request.setTransferTimeout(STALL_TIMEOUT_MS);
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, &GrammarSetup::onData);
    connect(m_reply, &QNetworkReply::finished, this, &GrammarSetup::onDownloaded);
}

void GrammarSetup::onData()
{
    if (m_reply == nullptr || m_file == nullptr || !m_writeError.isEmpty()) {
        return;
    }
    const QByteArray data = m_reply->readAll();
    if (m_file->write(data) != data.size()) {
        m_writeError = m_file->fileName() + QStringLiteral(": ") + m_file->errorString();
        m_reply->abort();
        return;
    }
    m_hash.addData(data);
    const auto index = static_cast<std::size_t>(m_step);
    m_received.at(index) += data.size();
    emit progress();
}

void GrammarSetup::onDownloaded()
{
    QNetworkReply* reply = std::exchange(m_reply, nullptr);
    if (reply == nullptr) {
        return;
    }
    reply->deleteLater();
    if (m_state != State::Running) {
        dropPartFile();
        return;
    }
    if (!m_writeError.isEmpty()) {
        const bool full = QStorageInfo(GrammarComponents::archiveFolder()).bytesAvailable() <
                              LOW_ROOM_BYTES ||
                          m_file->error() == QFileDevice::ResourceError;
        const QString error = m_writeError;
        dropPartFile();
        fail(full ? Failure::NoRoom : Failure::NoFolder, error, m_download.url);
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        dropPartFile();
        failDownload(reply);
        return;
    }
    const QByteArray rest = reply->readAll();
    m_hash.addData(rest);
    const bool written = m_file->write(rest) == rest.size() && m_file->flush();
    const qint64 size = m_file->size();
    m_file->close();
    const QByteArray sha256 = m_hash.result().toHex();
    if (!written) {
        const QString error = m_file->fileName() + QStringLiteral(": ") + m_file->errorString();
        dropPartFile();
        fail(Failure::NoRoom, error, m_download.url);
        return;
    }
    if (sha256 != m_download.sha256.toLower() || (m_download.size > 0 && size != m_download.size)) {
        core::Logger::getInstance().warn("GrammarSetup: {} is damaged: SHA-256 {}, {} bytes",
                                         m_download.fileName.toStdString(),
                                         sha256.toStdString(), size);
        dropPartFile();
        fail(Failure::Damaged,
             QStringLiteral("SHA-256: %1 (%2)")
                 .arg(QString::fromLatin1(sha256), QString::fromLatin1(m_download.sha256)),
             m_download.url);
        return;
    }

    // Checked: kept under its own name
    const QString archive =
        QDir(GrammarComponents::archiveFolder()).filePath(m_download.fileName);
    QFile::remove(archive);
    const bool kept = m_file->rename(archive);
    m_file.reset();
    if (!kept) {
        fail(Failure::NoFolder, QDir::toNativeSeparators(archive), m_download.url);
        return;
    }
    const auto index = static_cast<std::size_t>(m_step);
    m_done.at(index) = true;
    m_received.at(index) = m_total.at(index);
    next();
}

void GrammarSetup::dropPartFile()
{
    if (m_file != nullptr) {
        m_file->close();
        m_file->remove();
        m_file.reset();
    }
}

void GrammarSetup::failDownload(QNetworkReply* reply)
{
    // No connection at all, or a server that does not answer as it should
    const QNetworkReply::NetworkError error = reply->error();
    const bool noInternet = error == QNetworkReply::HostNotFoundError ||
                            error == QNetworkReply::UnknownNetworkError ||
                            error == QNetworkReply::TemporaryNetworkFailureError ||
                            error == QNetworkReply::NetworkSessionFailedError ||
                            error == QNetworkReply::ProxyNotFoundError;
    fail(noInternet ? Failure::NoInternet : Failure::ServerDown,
         QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(static_cast<int>(error)),
         reply->request().url());
}

// =============================================================================
// Unpacking
// =============================================================================

void GrammarSetup::unpack()
{
    const bool javaUnpacked =
        GrammarComponents::inspectJava(GrammarComponents::downloadedJava()).feature >=
        GrammarComponents::MIN_JAVA_VERSION;
    const QString javaArchive = javaUnpacked ? QString() : GrammarComponents::javaArchive();
    const QString languageToolArchive = GrammarComponents::languageToolDownloaded()
                                            ? QString()
                                            : GrammarComponents::languageToolArchive();
    const QString javaFolder = GrammarComponents::javaFolder();
    const QString languageToolFolder = GrammarComponents::languageToolFolder();

    m_cancelUnpack = std::make_shared<std::atomic_bool>(false);
    auto cancel = m_cancelUnpack;
    auto result = std::make_shared<UnpackResult>();
    QThread* thread = QThread::create([=]() {
        if (!javaArchive.isEmpty()) {
            *result = unpackInto(javaArchive, javaFolder, *cancel);
        }
        if (!languageToolArchive.isEmpty() && result->failure == Failure::None &&
            !result->cancelled) {
            *result = unpackInto(languageToolArchive, languageToolFolder, *cancel);
        }
    });
    m_unpacker = thread;
    connect(thread, &QThread::finished, this, [this, thread, result]() {
        thread->deleteLater();
        m_unpacker = nullptr;
        m_cancelUnpack.reset();
        if (m_state != State::Running) {
            return;
        }
        if (result->failure != Failure::None) {
            fail(result->failure, result->errorText);
            return;
        }
        // What was unpacked runs
        if (GrammarComponents::inspectJava(GrammarComponents::downloadedJava()).feature <
            GrammarComponents::MIN_JAVA_VERSION) {
            fail(Failure::NotUnpacked,
                 QDir::toNativeSeparators(GrammarComponents::javaFolder()));
            return;
        }
        if (!GrammarComponents::languageToolDownloaded()) {
            fail(Failure::NotUnpacked,
                 QDir::toNativeSeparators(GrammarComponents::languageToolFolder()));
            return;
        }
        next();
    });
    thread->start();
}

// =============================================================================
// The first run
// =============================================================================

void GrammarSetup::firstRun()
{
    m_server->setExternal(false);
    m_server->setJava(GrammarComponents::downloadedJava());
    m_server->setFolder(GrammarComponents::languageToolFolder());
    connect(m_server, &LanguageToolServer::stateChanged, this, &GrammarSetup::onServerState,
            Qt::UniqueConnection);
    if (m_server->state() == LanguageToolServer::State::Running) {
        trialCheck();
    } else {
        m_server->start();
    }
}

void GrammarSetup::onServerState()
{
    if (m_state != State::Running || m_step != Step::FirstRun) {
        return;
    }
    switch (m_server->state()) {
    case LanguageToolServer::State::Running:
        trialCheck();
        break;
    case LanguageToolServer::State::Failed:
        fail(Failure::NotStarted, m_server->details());
        break;
    default:
        break;
    }
}

void GrammarSetup::trialCheck()
{
    if (m_trial != nullptr) {
        return;
    }
    // The book's language loads its rules now; another one when the server does not check it
    QString language = editor::GrammarCheckService::languageFor(m_language);
    const QStringList languages = m_server->languages();
    const bool checked = std::any_of(languages.begin(), languages.end(),
                                     [&language](const QString& code) {
                                         return code.compare(language, Qt::CaseInsensitive) == 0 ||
                                                code.startsWith(language + QLatin1Char('-'),
                                                                Qt::CaseInsensitive);
                                     });
    if (language.isEmpty() || !checked) {
        language = QLatin1String(TRIAL_FALLBACK_LANGUAGE);
    }

    m_trial = new editor::GrammarCheckService(this);
    m_trial->setServer(m_server->address());
    m_trial->setLanguage(language);
    connect(m_trial, &editor::GrammarCheckService::textChecked, this, [this]() {
        dropTrial();
        core::Logger::getInstance().info("GrammarSetup: Grammar is ready");
        m_done[3] = true;
        end(State::Finished);
    });
    connect(m_trial, &editor::GrammarCheckService::serverError, this,
            [this](const QString& message) {
                dropTrial();
                fail(Failure::NotStarted, message);
            });
    m_trial->check(QLatin1String(TRIAL_TEXT));
}

void GrammarSetup::dropTrial()
{
    if (m_trial != nullptr) {
        disconnect(m_trial, nullptr, this, nullptr);
        std::exchange(m_trial, nullptr)->deleteLater();
    }
}

// =============================================================================
// The end
// =============================================================================

void GrammarSetup::fail(Failure failure, const QString& errorText, const QUrl& url)
{
    if (m_state != State::Running) {
        return;
    }
    m_failure = failure;
    m_errorText = errorText;
    m_errorUrl = url;
    core::Logger::getInstance().warn("GrammarSetup: Failed at step {} ({}): {}",
                                     static_cast<int>(m_step), static_cast<int>(failure),
                                     errorText.toStdString());
    end(State::Failed);
}

void GrammarSetup::end(State state)
{
    m_state = state;
    if (m_reply != nullptr) {
        QNetworkReply* reply = std::exchange(m_reply, nullptr);
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }
    dropPartFile();
    if (m_cancelUnpack) {
        *m_cancelUnpack = true;
    }
    dropTrial();
    disconnect(m_server, &LanguageToolServer::stateChanged, this, &GrammarSetup::onServerState);
    if (state != State::Finished && m_step == Step::FirstRun) {
        m_server->stop();
    }
    emit progress();
    emit finished();
}

}  // namespace kalahari::gui
