/// @file grammar_components.h
/// @brief What grammar as you type needs on this computer: Java and LanguageTool, where the
///        program puts them and what the ones found are

#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>

namespace kalahari::gui {

/// @brief A file the program downloads, with what it has to be
struct GrammarDownload {
    QUrl url;           ///< Where it is
    QString fileName;   ///< Its name in the folder of the downloads
    QByteArray sha256;  ///< Its SHA-256, in hex; empty: not known before (it comes with it)
    qint64 size{0};     ///< Its size in bytes; 0: not known before
};

/// @brief Java and LanguageTool: the ones the program downloads, and the ones found
///
/// The program downloads both into a folder of its own (downloadFolder()): no rights of an
/// administrator are needed and nothing else on the computer changes. LanguageTool is the
/// version tested with the program, checked against the SHA-256 it has; Java is Eclipse
/// Temurin 21, whose current file and SHA-256 the Adoptium service gives.
class GrammarComponents {
public:
    /// @brief The oldest Java LanguageTool runs on
    static constexpr int MIN_JAVA_VERSION = 17;

    /// @brief The Java the program downloads (Eclipse Temurin)
    static constexpr int JAVA_VERSION = 21;

    /// @brief The LanguageTool the program downloads
    static constexpr const char* LANGUAGE_TOOL_VERSION = "6.6";

    /// @brief The size of the file of LanguageTool (bytes)
    static constexpr qint64 LANGUAGE_TOOL_DOWNLOAD_BYTES = 251'998'221;

    /// @brief About how much the file of Java takes (bytes), until the Adoptium service tells
    static constexpr qint64 JAVA_DOWNLOAD_BYTES = 49'000'000;

    /// @brief About how much Java and LanguageTool take once unpacked (bytes)
    static constexpr qint64 INSTALLED_BYTES = 450'000'000;

    /// @brief How much free room the preparation needs (bytes): the unpacked components and
    ///        the file of LanguageTool while it is unpacked
    static constexpr qint64 NEEDED_FREE_BYTES = 750'000'000;

    /// @brief What a Java is
    struct JavaInfo {
        bool found{false};  ///< The file is a Java whose version is known
        int feature{0};     ///< Its version (21 for 21.0.5, 8 for 1.8.0)
        QString vendor;     ///< Who made it, as it is known (Eclipse Temurin); may be empty
    };

    /// @brief What a folder of LanguageTool is
    struct LanguageToolInfo {
        bool found{false};  ///< It has languagetool-server.jar
        QString version;    ///< Its version (6.6); may be empty
    };

    /// @brief The folder the program downloads Java and LanguageTool into, its own on this
    ///        computer (on Windows %LOCALAPPDATA%\\Kalahari\\grammar)
    static QString downloadFolder();

    /// @brief The folder of the downloads the program works with (downloadFolder(), or
    ///        another one for the tests)
    static QString folder();

    /// @brief Work with another folder than downloadFolder() (empty: that one again)
    static void setFolder(const QString& folder);

    /// @brief The folder of the Java the program downloads, in folder()
    static QString javaFolder();

    /// @brief The folder of the LanguageTool the program downloads, in folder()
    static QString languageToolFolder();

    /// @brief The java program of a Java folder (bin/java, or on macOS Contents/Home/bin/java)
    /// @return Its path, or empty when the folder has none
    static QString javaIn(const QString& javaFolder);

    /// @brief The java program of the Java the program downloaded, or empty when there is none
    static QString downloadedJava();

    /// @brief Whether the LanguageTool the program downloads is unpacked in folder()
    static bool languageToolDownloaded();

    /// @brief The file of Java downloaded and checked, waiting to be unpacked; empty: none
    static QString javaArchive();

    /// @brief The file of LanguageTool downloaded and checked, waiting to be unpacked; empty:
    ///        none
    static QString languageToolArchive();

    /// @brief Whether the Java the program downloads is there, unpacked or still in its file
    static bool javaDownloaded();

    /// @brief The folder the files are downloaded into, in folder()
    static QString archiveFolder();

    /// @brief Whether anything the program downloaded is in folder() (empty folders are not)
    static bool anythingDownloaded();

    /// @brief How much what the program downloaded takes on the disk (bytes)
    static qint64 downloadedBytes();

    /// @brief A size as people read it, never split between lines: 450 MB
    static QString sizeText(qint64 bytes);

    /// @brief About how much the program downloads, as people read it: 300 MB (to tens of
    ///        megabytes)
    static QString downloadSizeText();

    /// @brief Remove everything the program downloaded (folder())
    /// @return Whether nothing of it is left
    static bool removeDownloaded();

    /// @brief The LanguageTool the program downloads
    static GrammarDownload languageToolDownload();

    /// @brief The address of the Adoptium service that tells the current Java to download
    ///        for this system
    static QUrl javaInfoUrl();

    /// @brief The Java to download, from the answer of the Adoptium service (javaInfoUrl())
    /// @return It, or one with an empty address when the answer does not tell it
    static GrammarDownload javaDownloadFrom(const QByteArray& json);

    /// @brief What a java program is: from the release file of its folder, else from what
    ///        it says of itself (java -version)
    static JavaInfo inspectJava(const QString& java);

    /// @brief What a folder of LanguageTool is: its server and its version (from the name of
    ///        the folder, else from the server's manifest)
    static LanguageToolInfo inspectLanguageTool(const QString& folder);

    /// @brief The version of a Java from the version it gives ("21.0.5", "1.8.0_292", "17")
    static int featureVersion(const QString& version);

    /// @brief The version of a Java and who made it, from the text of its release file
    static JavaInfo javaFromRelease(const QByteArray& release);

    /// @brief The version of a Java from what java -version says ('openjdk version "21.0.5"')
    static int versionFromBanner(const QString& banner);
};

}  // namespace kalahari::gui
