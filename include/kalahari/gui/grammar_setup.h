/// @file grammar_setup.h
/// @brief Preparing grammar as you type: Java and LanguageTool downloaded, checked, unpacked
///        and started once

#pragma once

#include "kalahari/gui/grammar_components.h"

#include <QCryptographicHash>
#include <QObject>
#include <QString>
#include <QUrl>

#include <array>
#include <atomic>
#include <memory>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;
class QThread;

namespace kalahari::editor {
class GrammarCheckService;
}

namespace kalahari::gui {

class LanguageToolServer;

/// @brief Downloads and prepares Java and LanguageTool, in the steps the window of the
///        preparation shows
///
/// Everything goes into the folder of the downloads (GrammarComponents::folder()): each file
/// is checked against its SHA-256 before it is kept, then unpacked; at the end the server is
/// started and checks a text. What is already there is not downloaded again, so a
/// preparation broken off goes on from where it stopped. The preparation goes on also with
/// its window closed (the status bar shows it).
class GrammarSetup : public QObject {
    Q_OBJECT

public:
    /// @brief The steps, as the window shows them
    enum class Step {
        Java,          ///< Download Java
        LanguageTool,  ///< Download LanguageTool
        Unpack,        ///< Unpack and check the files
        FirstRun       ///< Start the server and check a text
    };
    Q_ENUM(Step)

    /// @brief How many steps there are
    static constexpr int STEP_COUNT = 4;

    /// @brief What the preparation is doing
    enum class State {
        Idle,       ///< Not started
        Running,    ///< In one of the steps
        Finished,   ///< Done: grammar can be checked
        Failed,     ///< Stopped by a failure (failure())
        Cancelled   ///< Broken off
    };
    Q_ENUM(State)

    /// @brief Why the preparation stopped
    enum class Failure {
        None,
        NoFolder,     ///< The folder of the downloads cannot be written
        NoRoom,       ///< Not enough room on the disk
        NoInternet,   ///< No connection: the server's name is not found
        ServerDown,   ///< The server of the file does not answer as it should
        Damaged,      ///< The file downloaded is not the one expected (its SHA-256)
        NotUnpacked,  ///< The file could not be unpacked
        NotStarted    ///< LanguageTool did not start or did not check the text
    };
    Q_ENUM(Failure)

    /// @brief Constructor
    /// @param server The server the preparation starts at the end (the one of the editors)
    /// @param parent Parent object
    explicit GrammarSetup(LanguageToolServer* server, QObject* parent = nullptr);

    /// @brief Destructor: what is being done is broken off
    ~GrammarSetup() override;

    GrammarSetup(const GrammarSetup&) = delete;
    GrammarSetup& operator=(const GrammarSetup&) = delete;

    /// @brief Ask another service which Java to download (the tests)
    void setJavaInfoUrl(const QUrl& url);

    /// @brief Download another LanguageTool (the tests)
    void setLanguageToolDownload(const GrammarDownload& download);

    /// @brief Download this Java instead of asking the Adoptium service which one (the tests)
    void setJavaDownload(const GrammarDownload& download);

    /// @brief The language the text of the first run is checked in (the book's)
    void setLanguage(const QString& language);

    // =========================================================================
    // Before the preparation
    // =========================================================================

    /// @brief The free room where the components go (bytes)
    static qint64 freeBytes();

    /// @brief The disk the components go on, as it is shown (C: on Windows)
    static QString disk();

    /// @brief Whether the folder of the downloads can be written (it is created for that)
    static bool canWrite();

    // =========================================================================
    // The preparation
    // =========================================================================

    /// @brief Start, or go on after a failure; nothing while it runs
    void start();

    /// @brief Break off: the files downloaded whole stay for the next time
    void cancel();

    State state() const { return m_state; }
    bool isRunning() const { return m_state == State::Running; }

    /// @brief The step being done, or the one that failed
    Step step() const { return m_step; }

    /// @brief Why the preparation stopped (State::Failed)
    Failure failure() const { return m_failure; }

    /// @brief Whether a step is done (also one not needed: it was done before)
    bool isDone(Step step) const;

    /// @brief How much of a file is downloaded (Step::Java, Step::LanguageTool; bytes)
    qint64 received(Step step) const;

    /// @brief How big a file is (bytes; for Java an estimate until the service tells)
    qint64 total(Step step) const;

    /// @brief How much of both downloads is done, 0 to 100
    int percent() const;

    /// @brief What went wrong, as the system or the program said it (State::Failed)
    QString errorText() const { return m_errorText; }

    /// @brief The address of the file that failed (empty when no file failed)
    QUrl errorUrl() const { return m_errorUrl; }

signals:
    /// @brief A step or a download went on
    void progress();

    /// @brief The preparation ended: finished, failed or broken off (state())
    void finished();

private:
    /// @brief The folder of the downloads, or the nearest folder above it there is
    static QString existingFolder();

    /// @brief Do the next step not done
    void next();

    /// @brief Ask the Adoptium service which Java to download
    void requestJava();

    /// @brief Download a file into the folder of the downloaded files
    void download(const GrammarDownload& file);

    /// @brief A piece of the file downloaded
    void onData();

    /// @brief The file is downloaded (or not)
    void onDownloaded();

    /// @brief Unpack the files downloaded, on another thread
    void unpack();

    /// @brief Start the server and check a text
    void firstRun();

    /// @brief The server started or failed (the first run)
    void onServerState();

    /// @brief Check a text on the server started
    void trialCheck();

    /// @brief Stop, for a reason
    void fail(Failure failure, const QString& errorText, const QUrl& url = QUrl());

    /// @brief The failure of a download, from its reply
    void failDownload(QNetworkReply* reply);

    /// @brief Close and remove the file being downloaded
    void dropPartFile();

    /// @brief Drop the text checked at the first run
    void dropTrial();

    /// @brief End with a state
    void end(State state);

    LanguageToolServer* m_server;
    QNetworkAccessManager* m_network;
    QUrl m_javaInfoUrl;
    GrammarDownload m_languageToolDownload;
    GrammarDownload m_javaDownload;
    QString m_language;

    State m_state{State::Idle};
    Step m_step{Step::Java};
    Failure m_failure{Failure::None};
    QString m_errorText;
    QUrl m_errorUrl;

    std::array<bool, STEP_COUNT> m_done{};       ///< Steps done
    std::array<qint64, 2> m_received{};          ///< Bytes of Java and LanguageTool
    std::array<qint64, 2> m_total{};             ///< Sizes of Java and LanguageTool

    // The download
    QNetworkReply* m_reply{nullptr};
    GrammarDownload m_download;                  ///< The file being downloaded
    std::unique_ptr<QFile> m_file;               ///< Where it goes, until it is checked
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    QString m_writeError;                        ///< Why the file could not be written

    // The unpacking
    QThread* m_unpacker{nullptr};
    std::shared_ptr<std::atomic_bool> m_cancelUnpack;

    // The first run
    editor::GrammarCheckService* m_trial{nullptr};
};

}  // namespace kalahari::gui
