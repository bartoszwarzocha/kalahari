/// @file grammar_components.cpp
/// @brief What grammar as you type needs on this computer: Java and LanguageTool, where the
///        program puts them and what the ones found are

#include "kalahari/gui/grammar_components.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrlQuery>

#include <zip.h>

namespace kalahari::gui {

namespace {

/// @brief The folder of the Java the program downloads, in the folder of the downloads
constexpr const char* JAVA_FOLDER_NAME = "jre-21";

/// @brief The folder of the downloaded files, in the folder of the downloads
constexpr const char* ARCHIVE_FOLDER_NAME = "downloads";

/// @brief Where the LanguageTool the program downloads is
constexpr const char* LANGUAGE_TOOL_URL =
    "https://languagetool.org/download/LanguageTool-6.6.zip";

/// @brief The SHA-256 of the file of LanguageTool 6.6
constexpr const char* LANGUAGE_TOOL_SHA256 =
    "53600506b399bb5ffe1e4c8dec794fd378212f14aaf38ccef9b6f89314d11631";

/// @brief How long java -version may take (ms)
constexpr int JAVA_VERSION_TIMEOUT_MS = 15000;

/// @brief How much of a release file or a manifest is read (bytes)
constexpr qint64 MAX_DESCRIPTION_BYTES = 64 * 1024;

/// @brief "About 300 MB": the size of the downloads to tens of megabytes
constexpr qint64 DOWNLOAD_ROUNDING = 10'000'000;

/// @brief The folder of the downloads, when the tests give another one
QString& folderOverride() {
    static QString folder;
    return folder;
}

/// @brief The name of the folder of LanguageTool, as its file unpacks
QString languageToolFolderName() {
    return QStringLiteral("LanguageTool-") +
           QLatin1String(GrammarComponents::LANGUAGE_TOOL_VERSION);
}

/// @brief Who made a Java, as it is known, from its release file (empty when it does not say)
QString vendorOf(const QString& implementor, const QString& implementorVersion) {
    if (implementor == QLatin1String("Eclipse Adoptium") ||
        implementorVersion.startsWith(QLatin1String("Temurin"))) {
        return QStringLiteral("Eclipse Temurin");
    }
    if (implementor.isEmpty() || implementor == QLatin1String("N/A") ||
        implementor == QLatin1String("Private Build")) {
        return QString();
    }
    return implementor;
}

/// @brief Whether a file is a program the system runs. On Windows a file that is not one (no
///        "MZ" at its start) makes the system show a message of its own and wait for it.
bool isProgram(const QString& path) {
#ifdef Q_OS_WIN
    QFile file(path);
    return file.open(QIODevice::ReadOnly) && file.read(2) == "MZ";
#else
    return QFileInfo(path).isExecutable();
#endif
}

/// @brief The version in the manifest of a jar (Implementation-Version); empty: none
QString jarVersion(const QString& jar) {
    // libzip takes the names in UTF-8, also on Windows
    int error = 0;
    zip_t* archive = zip_open(jar.toUtf8().constData(), ZIP_RDONLY, &error);
    if (archive == nullptr) {
        return QString();
    }
    QString version;
    if (zip_file_t* manifest = zip_fopen(archive, "META-INF/MANIFEST.MF", 0)) {
        QByteArray text(static_cast<qsizetype>(MAX_DESCRIPTION_BYTES), '\0');
        const zip_int64_t read = zip_fread(manifest, text.data(), MAX_DESCRIPTION_BYTES);
        zip_fclose(manifest);
        text.truncate(read > 0 ? static_cast<qsizetype>(read) : 0);
        static const QRegularExpression line(
            QStringLiteral("^Implementation-Version:\\s*(\\S+)\\s*$"),
            QRegularExpression::MultilineOption);
        const QRegularExpressionMatch match = line.match(QString::fromUtf8(text));
        if (match.hasMatch()) {
            version = match.captured(1);
        }
    }
    zip_close(archive);
    return version;
}

}  // namespace

QString GrammarComponents::downloadFolder() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
           QStringLiteral("/Kalahari/grammar");
}

QString GrammarComponents::folder() {
    return folderOverride().isEmpty() ? downloadFolder() : folderOverride();
}

void GrammarComponents::setFolder(const QString& folder) {
    folderOverride() = folder;
}

QString GrammarComponents::javaFolder() {
    return QDir(folder()).filePath(QLatin1String(JAVA_FOLDER_NAME));
}

QString GrammarComponents::languageToolFolder() {
    return QDir(folder()).filePath(languageToolFolderName());
}

QString GrammarComponents::archiveFolder() {
    return QDir(folder()).filePath(QLatin1String(ARCHIVE_FOLDER_NAME));
}

QString GrammarComponents::javaIn(const QString& javaFolder) {
    if (javaFolder.isEmpty()) {
        return QString();
    }
#ifdef Q_OS_WIN
    const QString program = QStringLiteral("java.exe");
#else
    const QString program = QStringLiteral("java");
#endif
    const QDir home(javaFolder);
    for (const QString& bin : {QStringLiteral("bin"), QStringLiteral("Contents/Home/bin")}) {
        const QString java = home.filePath(bin + QLatin1Char('/') + program);
        if (QFileInfo(java).isFile()) {
            return QDir::toNativeSeparators(java);
        }
    }
    return QString();
}

QString GrammarComponents::downloadedJava() {
    return javaIn(javaFolder());
}

bool GrammarComponents::languageToolDownloaded() {
    return inspectLanguageTool(languageToolFolder()).found;
}

QString GrammarComponents::javaArchive() {
    // Temurin's files: OpenJDK21U-jre_x64_windows_hotspot_21.0.5_11.zip
    const QStringList files = QDir(archiveFolder())
                                  .entryList({QStringLiteral("OpenJDK%1U-jre_*").arg(JAVA_VERSION)},
                                             QDir::Files, QDir::Name);
    for (const QString& file : files) {
        if (file.endsWith(QLatin1String(".zip")) || file.endsWith(QLatin1String(".tar.gz"))) {
            return QDir(archiveFolder()).filePath(file);
        }
    }
    return QString();
}

QString GrammarComponents::languageToolArchive() {
    const QString archive = QDir(archiveFolder()).filePath(languageToolDownload().fileName);
    return QFileInfo(archive).isFile() ? archive : QString();
}

bool GrammarComponents::javaDownloaded() {
    return !downloadedJava().isEmpty() || !javaArchive().isEmpty();
}

bool GrammarComponents::anythingDownloaded() {
    // Folders left empty by a download that did not start hold nothing to remove
    return downloadedBytes() > 0;
}

qint64 GrammarComponents::downloadedBytes() {
    qint64 bytes = 0;
    QDirIterator files(folder(), QDir::Files | QDir::Hidden | QDir::NoSymLinks,
                       QDirIterator::Subdirectories);
    while (files.hasNext()) {
        files.next();
        bytes += files.fileInfo().size();
    }
    return bytes;
}

QString GrammarComponents::sizeText(qint64 bytes) {
    return QLocale()
        .formattedDataSize(bytes, 0, QLocale::DataSizeSIFormat)
        .replace(QLatin1Char(' '), QChar(QChar::Nbsp));
}

QString GrammarComponents::downloadSizeText() {
    const qint64 total = LANGUAGE_TOOL_DOWNLOAD_BYTES + JAVA_DOWNLOAD_BYTES;
    const qint64 rounded = (total + DOWNLOAD_ROUNDING / 2) / DOWNLOAD_ROUNDING * DOWNLOAD_ROUNDING;
    return sizeText(rounded);
}

bool GrammarComponents::removeDownloaded() {
    // Only what the program puts there: the folder itself goes when nothing else is in it
    QDir downloads(folder());
    if (!downloads.exists()) {
        return true;
    }
    QStringList names = {QLatin1String(JAVA_FOLDER_NAME), languageToolFolderName(),
                         QLatin1String(ARCHIVE_FOLDER_NAME)};
    names += downloads.entryList({QStringLiteral("*.part")}, QDir::Dirs | QDir::Files);
    bool removed = true;
    for (const QString& name : names) {
        const QFileInfo item(downloads.filePath(name));
        if (item.isDir()) {
            removed = QDir(item.filePath()).removeRecursively() && removed;
        } else if (item.exists()) {
            removed = QFile::remove(item.filePath()) && removed;
        }
    }
    downloads.rmdir(downloads.absolutePath());
    return removed;
}

GrammarDownload GrammarComponents::languageToolDownload() {
    GrammarDownload download;
    download.url = QUrl(QLatin1String(LANGUAGE_TOOL_URL));
    download.fileName = languageToolFolderName() + QStringLiteral(".zip");
    download.sha256 = QByteArray(LANGUAGE_TOOL_SHA256);
    download.size = LANGUAGE_TOOL_DOWNLOAD_BYTES;
    return download;
}

QUrl GrammarComponents::javaInfoUrl() {
#if defined(Q_OS_WIN)
    const QString os = QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    const QString os = QStringLiteral("mac");
#else
    const QString os = QStringLiteral("linux");
#endif
    const QString cpu = QSysInfo::currentCpuArchitecture();
    const QString architecture =
        cpu.startsWith(QLatin1String("arm")) || cpu.startsWith(QLatin1String("aarch"))
            ? QStringLiteral("aarch64")
            : QStringLiteral("x64");

    QUrl url(QStringLiteral("https://api.adoptium.net/v3/assets/latest/%1/hotspot")
                 .arg(JAVA_VERSION));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("architecture"), architecture);
    query.addQueryItem(QStringLiteral("image_type"), QStringLiteral("jre"));
    query.addQueryItem(QStringLiteral("os"), os);
    query.addQueryItem(QStringLiteral("vendor"), QStringLiteral("eclipse"));
    url.setQuery(query);
    return url;
}

GrammarDownload GrammarComponents::javaDownloadFrom(const QByteArray& json) {
    GrammarDownload download;
    const QJsonArray releases = QJsonDocument::fromJson(json).array();
    if (releases.isEmpty()) {
        return download;
    }
    const QJsonObject package = releases.first()
                                    .toObject()
                                    .value(QLatin1String("binary"))
                                    .toObject()
                                    .value(QLatin1String("package"))
                                    .toObject();
    const QUrl url(package.value(QLatin1String("link")).toString());
    const QString name = QFileInfo(package.value(QLatin1String("name")).toString()).fileName();
    const QByteArray checksum = package.value(QLatin1String("checksum")).toString().toLatin1();
    static const QRegularExpression sha256(QStringLiteral("^[0-9a-fA-F]{64}$"));

    // Only a file of Temurin over https, with its SHA-256: anything else is not downloaded
    const bool known = name.startsWith(QStringLiteral("OpenJDK%1U-jre_").arg(JAVA_VERSION)) &&
                       (name.endsWith(QLatin1String(".zip")) ||
                        name.endsWith(QLatin1String(".tar.gz")));
    if (url.scheme() != QLatin1String("https") || !known ||
        !sha256.match(QString::fromLatin1(checksum)).hasMatch()) {
        return download;
    }
    download.url = url;
    download.fileName = name;
    download.sha256 = checksum.toLower();
    download.size = package.value(QLatin1String("size")).toInteger();
    return download;
}

GrammarComponents::JavaInfo GrammarComponents::inspectJava(const QString& java) {
    JavaInfo info;
    const QFileInfo file(java);
    if (java.isEmpty() || !file.isFile()) {
        return info;
    }

    // The release file of its folder (bin/java -> release), also through a link
    const QString program = file.canonicalFilePath();
    QDir home = QFileInfo(program).dir();
    home.cdUp();
    QFile release(home.filePath(QStringLiteral("release")));
    if (release.open(QIODevice::ReadOnly)) {
        info = javaFromRelease(release.read(MAX_DESCRIPTION_BYTES));
        if (info.found) {
            return info;
        }
    }

    // What it says of itself: only a java program is run
    const QString name = QFileInfo(program).completeBaseName().toLower();
    const bool javaName = name == QLatin1String("java") || name == QLatin1String("javaw");
    if (!javaName || !isProgram(program)) {
        return info;
    }
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, {QStringLiteral("-version")});
    if (!process.waitForFinished(JAVA_VERSION_TIMEOUT_MS)) {
        process.kill();
        process.waitForFinished();
        return info;
    }
    const QString banner = QString::fromLocal8Bit(process.readAll());
    info.feature = versionFromBanner(banner);
    info.found = info.feature > 0;
    if (banner.contains(QLatin1String("Temurin"))) {
        info.vendor = QStringLiteral("Eclipse Temurin");
    }
    return info;
}

GrammarComponents::LanguageToolInfo GrammarComponents::inspectLanguageTool(
    const QString& folder) {
    LanguageToolInfo info;
    if (folder.isEmpty()) {
        return info;
    }
    const QDir dir(folder);
    const QString server = dir.filePath(QStringLiteral("languagetool-server.jar"));
    info.found = QFileInfo(server).isFile();
    if (!info.found) {
        return info;
    }
    static const QRegularExpression name(QStringLiteral("^LanguageTool-(\\d+(?:\\.\\d+)+)"));
    const QRegularExpressionMatch match = name.match(dir.dirName());
    info.version = match.hasMatch() ? match.captured(1) : jarVersion(server);
    return info;
}

int GrammarComponents::featureVersion(const QString& version) {
    static const QRegularExpression numbers(QStringLiteral("^(\\d+)(?:\\.(\\d+))?"));
    const QRegularExpressionMatch match = numbers.match(version.trimmed());
    if (!match.hasMatch()) {
        return 0;
    }
    const int first = match.captured(1).toInt();
    // Before Java 9 the versions were 1.8.0_292
    return first == 1 && !match.captured(2).isEmpty() ? match.captured(2).toInt() : first;
}

GrammarComponents::JavaInfo GrammarComponents::javaFromRelease(const QByteArray& release) {
    QString version;
    QString implementor;
    QString implementorVersion;
    static const QRegularExpression line(QStringLiteral("^([A-Z_]+)=\"?([^\"\\r\\n]*)\"?\\s*$"),
                                         QRegularExpression::MultilineOption);
    auto lines = line.globalMatch(QString::fromUtf8(release));
    while (lines.hasNext()) {
        const QRegularExpressionMatch match = lines.next();
        const QString key = match.captured(1);
        if (key == QLatin1String("JAVA_VERSION")) {
            version = match.captured(2);
        } else if (key == QLatin1String("IMPLEMENTOR")) {
            implementor = match.captured(2);
        } else if (key == QLatin1String("IMPLEMENTOR_VERSION")) {
            implementorVersion = match.captured(2);
        }
    }
    JavaInfo info;
    info.feature = featureVersion(version);
    info.found = info.feature > 0;
    info.vendor = vendorOf(implementor, implementorVersion);
    return info;
}

int GrammarComponents::versionFromBanner(const QString& banner) {
    static const QRegularExpression version(QStringLiteral("version\\s+\"([^\"]+)\""));
    const QRegularExpressionMatch match = version.match(banner);
    return match.hasMatch() ? featureVersion(match.captured(1)) : 0;
}

}  // namespace kalahari::gui
