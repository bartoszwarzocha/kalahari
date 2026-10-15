/// @file grammar_test_utils.h
/// @brief What the tests of the grammar preparation need: the sites of Java and LanguageTool
///        on localhost, their files, a folder of the downloads of their own, the settings
///        given back and the stand-in for Java (kalahari-fake-java)

#pragma once

#include <catch2/catch_test_macros.hpp>

#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/grammar_components.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QHostAddress>
#include <QPair>
#include <QPointer>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUrl>

#include <nlohmann/json.hpp>
#include <zip.h>

#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace kalahari::test {

/// @brief Files served on localhost, as the sites of Java and LanguageTool serve them
class FakeDownloads {
public:
    FakeDownloads() {
        REQUIRE(m_server.listen(QHostAddress::LocalHost));
        QObject::connect(&m_server, &QTcpServer::newConnection, [this]() {
            while (QTcpSocket* socket = m_server.nextPendingConnection()) {
                auto buffer = std::make_shared<QByteArray>();
                QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket, buffer]() {
                    *buffer += socket->readAll();
                    receive(socket, *buffer);
                });
                QObject::connect(socket, &QTcpSocket::disconnected, socket,
                                 &QObject::deleteLater);
            }
        });
    }

    FakeDownloads(const FakeDownloads&) = delete;
    FakeDownloads& operator=(const FakeDownloads&) = delete;

    /// @brief The address of a file
    QUrl url(const QString& path) const {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/%2").arg(m_server.serverPort()).arg(path));
    }

    /// @brief Serve a file
    void add(const QString& path, const QByteArray& content) { m_files[path] = content; }

    /// @brief Answer a file with an HTTP error (0: serve it)
    void setStatus(const QString& path, int status) { m_statuses[path] = status; }

    /// @brief Keep the answers until release()
    void hold() { m_holding = true; }

    /// @brief Send the answers kept, and answer at once from now on
    void release() {
        m_holding = false;
        const auto held = std::exchange(m_held, {});
        for (const auto& [socket, answer] : held) {
            if (socket != nullptr) {
                send(socket, answer);
            }
        }
    }

    /// @brief How many times a file was asked for
    int requests(const QString& path) const { return m_requests.value(path); }

    /// @brief How many answers are kept (hold())
    int heldAnswers() const { return static_cast<int>(m_held.size()); }

private:
    void receive(QTcpSocket* socket, QByteArray& request) {
        const qsizetype headerEnd = request.indexOf("\r\n\r\n");
        if (headerEnd < 0) {
            return;
        }
        // GET /path HTTP/1.1
        const QList<QByteArray> line = request.left(request.indexOf("\r\n")).split(' ');
        request.clear();
        const QString path = line.size() > 1 ? QString::fromUtf8(line.at(1)).mid(1) : QString();
        m_requests[path] += 1;

        QByteArray answer;
        const int status = m_statuses.value(path);
        if (status != 0 || !m_files.contains(path)) {
            answer = "HTTP/1.1 " + QByteArray::number(status != 0 ? status : 404) +
                     " Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        } else {
            const QByteArray content = m_files.value(path);
            answer = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n"
                     "Content-Length: " +
                     QByteArray::number(content.size()) + "\r\nConnection: close\r\n\r\n" + content;
        }
        if (m_holding) {
            m_held.emplace_back(socket, answer);
        } else {
            send(socket, answer);
        }
    }

    static void send(QTcpSocket* socket, const QByteArray& answer) {
        socket->write(answer);
        socket->disconnectFromHost();
    }

    QTcpServer m_server;
    QHash<QString, QByteArray> m_files;
    QHash<QString, int> m_statuses;
    QHash<QString, int> m_requests;
    bool m_holding{false};
    std::vector<QPair<QPointer<QTcpSocket>, QByteArray>> m_held;
};

/// @brief A file in a zip
struct ZipEntry {
    QString name;        ///< Its path in the zip
    QByteArray content;  ///< What it holds
    bool executable{false};
};

/// @brief A zip of files, made with libzip (executables keep their Unix mode)
inline QByteArray zipOf(const std::vector<ZipEntry>& entries) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("files.zip"));
    int error = 0;
    zip_t* zip = zip_open(path.toUtf8().constData(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    REQUIRE(zip != nullptr);
    for (const ZipEntry& entry : entries) {
        zip_source_t* source =
            zip_source_buffer(zip, entry.content.constData(),
                              static_cast<zip_uint64_t>(entry.content.size()), 0);
        REQUIRE(source != nullptr);
        const zip_int64_t index =
            zip_file_add(zip, entry.name.toUtf8().constData(), source, ZIP_FL_ENC_UTF_8);
        REQUIRE(index >= 0);
        const zip_uint32_t mode = entry.executable ? 0100755U : 0100644U;
        zip_file_set_external_attributes(zip, static_cast<zip_uint64_t>(index), 0,
                                         ZIP_OPSYS_UNIX, mode << 16U);
    }
    REQUIRE(zip_close(zip) == 0);  // the buffers are read here
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

/// @brief The SHA-256 of a file, in hex
inline QByteArray sha256Of(const QByteArray& content) {
    return QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex();
}

/// @brief The stand-in for Java, next to the tests
inline QString fakeJavaProgram() {
#ifdef Q_OS_WIN
    const QString name = QStringLiteral("kalahari-fake-java.exe");
#else
    const QString name = QStringLiteral("kalahari-fake-java");
#endif
    return QDir(QCoreApplication::applicationDirPath()).filePath(name);
}

/// @brief The java program's name in a Java folder
inline QString javaProgramName() {
#ifdef Q_OS_WIN
    return QStringLiteral("java.exe");
#else
    return QStringLiteral("java");
#endif
}

/// @brief A file of Temurin as Adoptium serves it: its release file and the stand-in for
///        Java in its folder
inline QByteArray javaZip() {
    QFile program(fakeJavaProgram());
    REQUIRE(program.open(QIODevice::ReadOnly));
    const QString root = QStringLiteral("jdk-21.0.5+11-jre/");
    return zipOf({{root + QStringLiteral("release"),
                   "IMPLEMENTOR=\"Eclipse Adoptium\"\nJAVA_VERSION=\"21.0.5\"\n", false},
                  {root + QStringLiteral("bin/") + javaProgramName(), program.readAll(), true}});
}

/// @brief A file of LanguageTool: its folder with the server
inline QByteArray languageToolZip() {
    const QString root = QStringLiteral("LanguageTool-%1/")
                             .arg(QLatin1String(gui::GrammarComponents::LANGUAGE_TOOL_VERSION));
    return zipOf({{root + QStringLiteral("languagetool-server.jar"), "server", false},
                  {root + QStringLiteral("languagetool.jar"), "core", false}});
}

/// @brief What the preparation downloads for a file served
inline gui::GrammarDownload downloadOf(const FakeDownloads& site, const QString& fileName,
                                       const QByteArray& content) {
    gui::GrammarDownload download;
    download.url = site.url(fileName);
    download.fileName = fileName;
    download.sha256 = sha256Of(content);
    download.size = content.size();
    return download;
}

/// @brief The name of Temurin's file for this system, as the preparation keeps it
inline QString javaFileName() {
    return QStringLiteral("OpenJDK21U-jre_x64_test_hotspot_21.0.5_11.zip");
}

/// @brief The name of LanguageTool's file, as the preparation keeps it
inline QString languageToolFileName() {
    return QStringLiteral("LanguageTool-%1.zip")
        .arg(QLatin1String(gui::GrammarComponents::LANGUAGE_TOOL_VERSION));
}

/// @brief The folder of the downloads, of the test alone, while it lives
class TemporaryGrammarFolder {
public:
    TemporaryGrammarFolder() {
        REQUIRE(m_dir.isValid());
        gui::GrammarComponents::setFolder(path());
    }
    ~TemporaryGrammarFolder() { gui::GrammarComponents::setFolder(QString()); }

    TemporaryGrammarFolder(const TemporaryGrammarFolder&) = delete;
    TemporaryGrammarFolder& operator=(const TemporaryGrammarFolder&) = delete;

    /// @brief The folder of the downloads (in a folder of the test)
    QString path() const { return m_dir.filePath(QStringLiteral("grammar")); }

    /// @brief The folder of the test
    QString root() const { return m_dir.path(); }

private:
    QTemporaryDir m_dir;
};

/// @brief Gives the grammar settings, and the language of the spelling, back as they were
class KeptGrammarSettings {
public:
    KeptGrammarSettings() {
        for (const char* key : keys()) {
            m_values[key] = settings().get<nlohmann::json>(key, nlohmann::json());
        }
    }
    ~KeptGrammarSettings() {
        try {
            for (const auto& [key, value] : m_values) {
                if (!value.is_null()) {
                    settings().set<nlohmann::json>(key, value);
                }
            }
        } catch (...) {
            // Left as the test made them; the next tests set what they need
            std::fputs("The grammar settings could not be given back\n", stderr);
        }
    }
    KeptGrammarSettings(const KeptGrammarSettings&) = delete;
    KeptGrammarSettings& operator=(const KeptGrammarSettings&) = delete;

    static core::SettingsManager& settings() { return core::SettingsManager::getInstance(); }

private:
    static std::vector<const char*> keys() {
        return {"editor.grammarCheck.enabled",      "editor.grammarCheck.mode",
                "editor.grammarCheck.serverUrl",    "editor.grammarCheck.username",
                "editor.grammarCheck.apiKey",       "editor.grammarCheck.javaPath",
                "editor.grammarCheck.languageToolFolder", "editor.grammarCheck.port",
                "editor.grammarCheck.externalServer", "editor.grammarCheck.ignoredRules",
                "editor.spellCheck.language"};
    }

    std::map<std::string, nlohmann::json> m_values;
};

/// @brief On Windows the stand-in for Java, copied out of the folder of the tests, finds the
///        libraries of Qt there through PATH, while this lives
class FakeJavaLibraries {
public:
    FakeJavaLibraries() : m_path(qgetenv("PATH")) {
#ifdef Q_OS_WIN
        const QString tests = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
        qputenv("PATH", tests.toLocal8Bit() + ';' + m_path);
#endif
    }
    ~FakeJavaLibraries() {
#ifdef Q_OS_WIN
        qputenv("PATH", m_path);
#endif
    }
    FakeJavaLibraries(const FakeJavaLibraries&) = delete;
    FakeJavaLibraries& operator=(const FakeJavaLibraries&) = delete;

private:
    QByteArray m_path;
};

/// @brief An environment variable set while this lives (for the stand-in for Java)
class ScopedEnvironment {
public:
    ScopedEnvironment(const char* name, const QByteArray& value)
        : m_name(name), m_wasSet(qEnvironmentVariableIsSet(name)), m_value(qgetenv(name)) {
        qputenv(name, value);
    }
    ~ScopedEnvironment() {
        if (m_wasSet) {
            qputenv(m_name, m_value);
        } else {
            qunsetenv(m_name);
        }
    }
    ScopedEnvironment(const ScopedEnvironment&) = delete;
    ScopedEnvironment& operator=(const ScopedEnvironment&) = delete;

private:
    const char* m_name;
    bool m_wasSet;
    QByteArray m_value;
};

}  // namespace kalahari::test
