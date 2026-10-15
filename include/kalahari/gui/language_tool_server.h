/// @file language_tool_server.h
/// @brief The LanguageTool server on this computer: one the program starts and closes, or one
///        started outside it

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;
class QProcess;
class QTemporaryFile;
class QTimer;

namespace kalahari::gui {

/// @brief The LanguageTool server grammar as you type checks the text on, on this computer
///
/// The program starts the server itself (Java runs languagetool-server.jar of the folder of
/// LanguageTool) and closes it with itself; the system ends Java also when the program ends
/// abruptly (a crash). The server answers only on this computer. When
/// the port is taken by another program, it takes a free one (portChanged()). When the server
/// stops by itself, it is started again, a few times at most.
///
/// A server started outside the program (setExternal()) is only connected to: the program
/// does not start or close it.
class LanguageToolServer : public QObject {
    Q_OBJECT

public:
    /// @brief What the server is doing
    enum class State {
        Stopped,   ///< Not running: not started yet, or closed
        Starting,  ///< Started, not answering yet
        Running,   ///< Answers
        Failed     ///< Did not start, or does not answer (failure() says why)
    };
    Q_ENUM(State)

    /// @brief Why the server is not running (State::Failed)
    enum class Failure {
        None,          ///< It did not fail
        NotStarted,    ///< The server the program starts did not start (details())
        NotRunning,    ///< No server answers on the port (a server started outside)
        OtherProgram,  ///< Another program answers on the port (a server started outside)
        Stopped        ///< It stopped by itself and did not start again (details())
    };

    /// @brief The port LanguageTool usually listens on
    static constexpr int DEFAULT_PORT = 8081;

    /// @brief How long the server may take to start at first (ms): Java loads its rules
    static constexpr int START_TIMEOUT_MS = 120'000;

    /// @brief Constructor: nothing is started
    explicit LanguageToolServer(QObject* parent = nullptr);

    /// @brief Destructor: the server the program started is closed
    ~LanguageToolServer() override;

    LanguageToolServer(const LanguageToolServer&) = delete;
    LanguageToolServer& operator=(const LanguageToolServer&) = delete;

    // =========================================================================
    // Setup: a change stops the server; start() starts it with the new one
    // =========================================================================

    /// @brief The java program that runs the server
    void setJava(const QString& java);

    /// @brief The folder of LanguageTool (with languagetool-server.jar)
    void setFolder(const QString& folder);

    /// @brief The port the server listens on
    void setPort(int port);

    /// @brief Connect to a server started outside the program instead of starting one
    void setExternal(bool external);

    /// @brief How long the server may take to start (ms; START_TIMEOUT_MS at first), and a
    ///        server started outside to answer (at most 10 s)
    void setStartTimeout(int ms);

    QString java() const { return m_java; }
    QString folder() const { return m_folder; }
    int port() const { return m_port; }
    bool isExternal() const { return m_external; }

    /// @brief Whether there is something to start or connect to: a server started outside,
    ///        or a java program and a folder with the server
    bool canStart() const;

    // =========================================================================
    // State
    // =========================================================================

    State state() const { return m_state; }

    /// @brief Why the server is not running (State::Failed)
    Failure failure() const { return m_failure; }

    /// @brief What the server or Java said last (when it did not start or stopped), for the
    ///        details of the failure
    QString details() const;

    /// @brief Whether the program started the server (else it was started outside)
    bool startedByProgram() const { return !m_external; }

    /// @brief The address of the server (http://localhost:port)
    QString address() const;

    /// @brief The languages the server checks (pl-PL, en-US...), once it answers
    QStringList languages() const { return m_languages; }

public slots:
    /// @brief Start the server, or connect to the one started outside; nothing while it runs
    ///        or starts
    void start();

    /// @brief Close the server the program started (a server started outside is left)
    void stop();

signals:
    /// @brief The server started, stopped, failed...
    void stateChanged(kalahari::gui::LanguageToolServer::State state);

    /// @brief The port was taken by another program: the server listens on another one
    void portChanged(int port);

    /// @brief The server stopped by itself and is started again
    void restarting();

private:
    /// @brief Start the java program on the port given, or on the next free one
    void launch(int fromPort);

    /// @brief Ask the server whether it answers
    void probe();

    /// @brief An answer to probe()
    void onProbeReply(QNetworkReply* reply);

    /// @brief The java program ended
    void onFinished();

    /// @brief Not running any more, for a reason
    void fail(Failure failure);

    /// @brief Set the state, telling it when it changes
    void setState(State state);

    /// @brief Close the java program, without telling it
    void closeProcess();

    /// @brief The file of the settings the server is started with (written once, removed with
    ///        this); empty when it cannot be written: the server checks slower then
    QString settingsFile();

    /// @brief A port nothing listens on here, from the one given
    static int freePort(int from);

    QNetworkAccessManager* m_network;
    QTimer* m_probeTimer;   ///< Asks the server again while it starts
    QTimer* m_startTimer;   ///< How long it may take to start
    QProcess* m_process{nullptr};
    QNetworkReply* m_probeReply{nullptr};
    QTemporaryFile* m_settings{nullptr};  ///< The settings the server is started with

    QString m_java;
    QString m_folder;
    int m_port{DEFAULT_PORT};
    bool m_external{false};

    State m_state{State::Stopped};
    Failure m_failure{Failure::None};
    QString m_output;           ///< The last of what the java program wrote
    QStringList m_languages;
    int m_restarts{0};          ///< Starts after the server stopped by itself
    int m_portTries{0};         ///< Other ports tried because the port was taken
    int m_startTimeoutMs{START_TIMEOUT_MS};  ///< How long the server may take to start
};

}  // namespace kalahari::gui
