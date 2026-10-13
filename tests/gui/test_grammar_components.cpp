/// @file test_grammar_components.cpp
/// @brief Grammar on this computer: what Java and LanguageTool are, the files the program
///        downloads, where it puts them and their removal

#include <catch2/catch_test_macros.hpp>

#include "grammar_test_utils.h"
#include "kalahari/gui/grammar_components.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrlQuery>

using namespace kalahari::gui;
using namespace kalahari::test;

namespace {

/// Write a file, with its folders
void writeFile(const QString& path, const QByteArray& content) {
    REQUIRE(QDir().mkpath(QFileInfo(path).path()));
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write(content);
}

/// What the Adoptium service answers, with a package
QByteArray adoptiumAnswer(const QString& link, const QString& name, const QString& checksum) {
    return QStringLiteral("[{\"binary\":{\"package\":{\"link\":\"%1\",\"name\":\"%2\","
                          "\"checksum\":\"%3\",\"size\":48123456}}}]")
        .arg(link, name, checksum)
        .toUtf8();
}

const QString CHECKSUM =
    QStringLiteral("0123456789abcdef0123456789ABCDEF0123456789abcdef0123456789abcdef");

}  // namespace

TEST_CASE("Grammar components: the version of a Java", "[gui][grammar]") {
    CHECK(GrammarComponents::featureVersion(QStringLiteral("21.0.5")) == 21);
    CHECK(GrammarComponents::featureVersion(QStringLiteral("17")) == 17);
    CHECK(GrammarComponents::featureVersion(QStringLiteral("1.8.0_292")) == 8);
    CHECK(GrammarComponents::featureVersion(QStringLiteral(" 22-ea ")) == 22);
    CHECK(GrammarComponents::featureVersion(QStringLiteral("abc")) == 0);

    CHECK(GrammarComponents::versionFromBanner(
              QStringLiteral("openjdk version \"21.0.5\" 2024-10-15 LTS\nOpenJDK Runtime")) == 21);
    CHECK(GrammarComponents::versionFromBanner(QStringLiteral("java version \"1.8.0_401\"")) == 8);
    CHECK(GrammarComponents::versionFromBanner(QStringLiteral("no Java here")) == 0);

    const GrammarComponents::JavaInfo temurin = GrammarComponents::javaFromRelease(
        "IMPLEMENTOR=\"Eclipse Adoptium\"\nIMPLEMENTOR_VERSION=\"Temurin-21.0.5+11\"\n"
        "JAVA_VERSION=\"21.0.5\"\r\n");
    CHECK(temurin.found);
    CHECK(temurin.feature == 21);
    CHECK(temurin.vendor == QStringLiteral("Eclipse Temurin"));

    const GrammarComponents::JavaInfo other =
        GrammarComponents::javaFromRelease(
            "JAVA_VERSION=\"17.0.2\"\nIMPLEMENTOR=\"Oracle Corporation\"\n");
    CHECK(other.feature == 17);
    CHECK(other.vendor == QStringLiteral("Oracle Corporation"));

    const GrammarComponents::JavaInfo unnamed =
        GrammarComponents::javaFromRelease("JAVA_VERSION=\"11.0.1\"\nIMPLEMENTOR=\"N/A\"\n");
    CHECK(unnamed.feature == 11);
    CHECK(unnamed.vendor.isEmpty());

    CHECK_FALSE(GrammarComponents::javaFromRelease("OS_NAME=\"Linux\"\n").found);
}

TEST_CASE("Grammar components: the Java Adoptium tells for this system", "[gui][grammar]") {
    const QUrl info = GrammarComponents::javaInfoUrl();
    CHECK(info.scheme() == QStringLiteral("https"));
    CHECK(info.host() == QStringLiteral("api.adoptium.net"));
    CHECK(info.path() == QStringLiteral("/v3/assets/latest/21/hotspot"));
    const QUrlQuery query(info);
    CHECK(query.queryItemValue(QStringLiteral("image_type")) == QStringLiteral("jre"));
    CHECK(query.queryItemValue(QStringLiteral("vendor")) == QStringLiteral("eclipse"));
    CHECK_FALSE(query.queryItemValue(QStringLiteral("os")).isEmpty());
    CHECK_FALSE(query.queryItemValue(QStringLiteral("architecture")).isEmpty());

    const QString link = QStringLiteral(
        "https://github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.5%2B11/"
        "OpenJDK21U-jre_x64_windows_hotspot_21.0.5_11.zip");
    const QString name = QStringLiteral("OpenJDK21U-jre_x64_windows_hotspot_21.0.5_11.zip");

    SECTION("a file of Temurin over https, with its SHA-256") {
        const GrammarDownload java =
            GrammarComponents::javaDownloadFrom(adoptiumAnswer(link, name, CHECKSUM));
        CHECK(java.url == QUrl(link));
        CHECK(java.fileName == name);
        CHECK(java.sha256 == CHECKSUM.toLower().toLatin1());
        CHECK(java.size == 48123456);
    }

    SECTION("anything else is not downloaded") {
        const QString plain =
            QString(link).replace(QStringLiteral("https:"), QStringLiteral("http:"));
        CHECK(GrammarComponents::javaDownloadFrom(adoptiumAnswer(plain, name, CHECKSUM))
                  .url.isEmpty());
        CHECK(GrammarComponents::javaDownloadFrom(
                  adoptiumAnswer(link, QStringLiteral("setup.exe"), CHECKSUM))
                  .url.isEmpty());
        CHECK(GrammarComponents::javaDownloadFrom(
                  adoptiumAnswer(link, QStringLiteral("OpenJDK21U-jre_x64_windows_hotspot.msi"),
                                 CHECKSUM))
                  .url.isEmpty());
        CHECK(GrammarComponents::javaDownloadFrom(adoptiumAnswer(link, name, QStringLiteral("abc")))
                  .url.isEmpty());
        CHECK(GrammarComponents::javaDownloadFrom("[]").url.isEmpty());
        CHECK(GrammarComponents::javaDownloadFrom("not json").url.isEmpty());
    }

    SECTION("the name of the file only, not a path") {
        const GrammarDownload java = GrammarComponents::javaDownloadFrom(
            adoptiumAnswer(link, QStringLiteral("../../") + name, CHECKSUM));
        CHECK(java.fileName == name);
    }
}

TEST_CASE("Grammar components: the LanguageTool the program downloads", "[gui][grammar]") {
    const GrammarDownload languageTool = GrammarComponents::languageToolDownload();
    CHECK(languageTool.url.scheme() == QStringLiteral("https"));
    CHECK(languageTool.url.host() == QStringLiteral("languagetool.org"));
    CHECK(languageTool.fileName == QStringLiteral("LanguageTool-6.6.zip"));
    CHECK(languageTool.sha256.size() == 64);
    CHECK(languageTool.size == GrammarComponents::LANGUAGE_TOOL_DOWNLOAD_BYTES);
    CHECK(GrammarComponents::downloadSizeText().contains(QStringLiteral("300")));
}

TEST_CASE("Grammar components: what a Java and a LanguageTool folder are", "[gui][grammar]") {
    TemporaryGrammarFolder temporary;
    const QDir root(temporary.root());

    SECTION("Java: its release file, in its folder or under Contents/Home on macOS") {
        const QString java = root.filePath(QStringLiteral("jdk/bin/") + javaProgramName());
        writeFile(java, "program");
        CHECK_FALSE(GrammarComponents::inspectJava(java).found);  // no release file
        writeFile(root.filePath(QStringLiteral("jdk/release")),
                  "JAVA_VERSION=\"21.0.5\"\nIMPLEMENTOR=\"Eclipse Adoptium\"\n");
        const GrammarComponents::JavaInfo info = GrammarComponents::inspectJava(java);
        CHECK(info.found);
        CHECK(info.feature == 21);
        CHECK(info.vendor == QStringLiteral("Eclipse Temurin"));
        CHECK(QDir::fromNativeSeparators(
                  GrammarComponents::javaIn(root.filePath(QStringLiteral("jdk")))) == java);

        const QString mac =
            root.filePath(QStringLiteral("mac/Contents/Home/bin/") + javaProgramName());
        writeFile(mac, "program");
        CHECK(QDir::fromNativeSeparators(
                  GrammarComponents::javaIn(root.filePath(QStringLiteral("mac")))) == mac);
        CHECK(GrammarComponents::javaIn(root.filePath(QStringLiteral("none"))).isEmpty());
        CHECK(GrammarComponents::javaIn(QString()).isEmpty());
    }

    SECTION("not run: a program that is not java, or no file") {
        const QString other = root.filePath(QStringLiteral("tools/bin/notjava"));
        writeFile(other, "#!/bin/sh\necho 'openjdk version \"21\"'\n");
        QFile(other).setPermissions(QFile(other).permissions() | QFileDevice::ExeOwner);
        CHECK_FALSE(GrammarComponents::inspectJava(other).found);
        CHECK_FALSE(GrammarComponents::inspectJava(root.filePath(QStringLiteral("nothing"))).found);
        CHECK_FALSE(GrammarComponents::inspectJava(QString()).found);
    }

    SECTION("LanguageTool: its server, its version from the name of the folder") {
        const QString folder = root.filePath(QStringLiteral("LanguageTool-6.5"));
        CHECK_FALSE(GrammarComponents::inspectLanguageTool(folder).found);
        writeFile(QDir(folder).filePath(QStringLiteral("languagetool-server.jar")), "server");
        const GrammarComponents::LanguageToolInfo info =
            GrammarComponents::inspectLanguageTool(folder);
        CHECK(info.found);
        CHECK(info.version == QStringLiteral("6.5"));

        const QString renamed = root.filePath(QStringLiteral("lt"));
        writeFile(QDir(renamed).filePath(QStringLiteral("languagetool-server.jar")), "not a jar");
        CHECK(GrammarComponents::inspectLanguageTool(renamed).found);
        CHECK(GrammarComponents::inspectLanguageTool(renamed).version.isEmpty());
        CHECK_FALSE(GrammarComponents::inspectLanguageTool(QString()).found);
    }
}

TEST_CASE("Grammar components: the files downloaded, in a folder of the program, and their "
          "removal",
          "[gui][grammar]") {
    TemporaryGrammarFolder temporary;
    const QDir folder(temporary.path());
    CHECK(GrammarComponents::folder() == temporary.path());
    CHECK(GrammarComponents::javaFolder() == folder.filePath(QStringLiteral("jre-21")));
    CHECK(GrammarComponents::languageToolFolder() ==
          folder.filePath(QStringLiteral("LanguageTool-6.6")));
    CHECK(GrammarComponents::archiveFolder() == folder.filePath(QStringLiteral("downloads")));
    CHECK(GrammarComponents::downloadFolder().endsWith(QStringLiteral("/Kalahari/grammar")));

    // Nothing yet
    CHECK_FALSE(GrammarComponents::anythingDownloaded());
    CHECK_FALSE(GrammarComponents::javaDownloaded());
    CHECK_FALSE(GrammarComponents::languageToolDownloaded());
    CHECK(GrammarComponents::downloadedJava().isEmpty());
    CHECK(GrammarComponents::downloadedBytes() == 0);
    CHECK(GrammarComponents::removeDownloaded());

    // The files downloaded, waiting to be unpacked
    writeFile(folder.filePath(
                  QStringLiteral("downloads/OpenJDK21U-jre_x64_linux_hotspot_21.0.5_11.tar.gz")),
              QByteArray(1000, 'j'));
    writeFile(folder.filePath(QStringLiteral("downloads/LanguageTool-6.6.zip")),
              QByteArray(500, 'l'));
    CHECK(GrammarComponents::anythingDownloaded());
    CHECK(GrammarComponents::javaDownloaded());
    CHECK(GrammarComponents::javaArchive().endsWith(QStringLiteral(".tar.gz")));
    CHECK(GrammarComponents::languageToolArchive().endsWith(
        QStringLiteral("LanguageTool-6.6.zip")));
    CHECK(GrammarComponents::downloadedBytes() == 1500);

    // Unpacked
    writeFile(folder.filePath(QStringLiteral("jre-21/bin/") + javaProgramName()), "program");
    writeFile(folder.filePath(QStringLiteral("LanguageTool-6.6/languagetool-server.jar")),
              "server");
    CHECK_FALSE(GrammarComponents::downloadedJava().isEmpty());
    CHECK(GrammarComponents::languageToolDownloaded());

    // Removed, with the folder; what else is in it stays
    writeFile(folder.filePath(QStringLiteral("notes.txt")), "the writer's");
    CHECK(GrammarComponents::removeDownloaded());
    CHECK_FALSE(GrammarComponents::javaDownloaded());
    CHECK_FALSE(GrammarComponents::languageToolDownloaded());
    CHECK(QFileInfo::exists(folder.filePath(QStringLiteral("notes.txt"))));
    QFile::remove(folder.filePath(QStringLiteral("notes.txt")));
    writeFile(folder.filePath(QStringLiteral("downloads/LanguageTool-6.6.zip.part")), "part");
    CHECK(GrammarComponents::removeDownloaded());
    CHECK_FALSE(folder.exists());
}
