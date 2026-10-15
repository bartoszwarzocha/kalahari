/// @file grammar_coordinator.h
/// @brief Grammar as you type: where the text is checked (LanguageTool on this computer or a
///        server in the internet), the preparation of LanguageTool, the language and the
///        command

#pragma once

#include <QMap>
#include <QObject>
#include <QString>

class QStatusBar;
class QTabWidget;
class QToolButton;
class QWidget;

namespace kalahari::editor {
class GrammarCheckService;
}

namespace kalahari::gui {

class GrammarSetup;
class LanguageToolServer;

namespace dialogs {
class GrammarSetupDialog;
}

/// @brief Checks the grammar of the documents as they are written
///
/// Owns the checking the editors send their paragraphs to and gives it to the editors of
/// the documents. The text is checked where the settings say:
/// - on this computer (the default): by LanguageTool, which the program downloads and
///   prepares with Java (GrammarSetup) and starts when a document needs it
///   (LanguageToolServer); the text does not leave the computer;
/// - on a server in the internet, chosen by the writer, who agreed to send the text there.
///
/// The language is the one the spelling is checked in (SpellingCoordinator::wantedLanguage()).
/// The rules the writer ignores are kept in the settings. The status bar tells the start of
/// the server, its failures and the preparation going on in the background (a click shows
/// its window).
///
/// Tools > Check Grammar as You Type turns the checking on and off (the setting
/// editor.grammarCheck.enabled, also in the Settings dialog); turned on before anything is
/// prepared, it offers to prepare it.
class GrammarCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief The settings of the grammar
    static constexpr const char* ENABLED_KEY = "editor.grammarCheck.enabled";
    static constexpr const char* MODE_KEY = "editor.grammarCheck.mode";
    static constexpr const char* SERVER_KEY = "editor.grammarCheck.serverUrl";
    static constexpr const char* USERNAME_KEY = "editor.grammarCheck.username";
    static constexpr const char* API_KEY_KEY = "editor.grammarCheck.apiKey";
    static constexpr const char* JAVA_KEY = "editor.grammarCheck.javaPath";
    static constexpr const char* FOLDER_KEY = "editor.grammarCheck.languageToolFolder";
    static constexpr const char* PORT_KEY = "editor.grammarCheck.port";
    static constexpr const char* EXTERNAL_KEY = "editor.grammarCheck.externalServer";
    static constexpr const char* IGNORED_RULES_KEY = "editor.grammarCheck.ignoredRules";

    /// @brief The values of MODE_KEY
    static constexpr const char* LOCAL_MODE = "local";
    static constexpr const char* REMOTE_MODE = "remote";

    /// @brief The servers in the internet the Settings dialog offers
    static constexpr const char* FREE_SERVER = "https://api.languagetool.org";
    static constexpr const char* PREMIUM_SERVER = "https://api.languagetoolplus.com";

    /// @brief What grammar as you type can do now, from the settings
    enum class Readiness {
        NotPrepared,  ///< On this computer, nothing prepared yet
        Missing,      ///< On this computer, the components set are not there any more
        Ready,        ///< On this computer, ready
        Remote        ///< On a server in the internet
    };

    /// @brief Constructor
    /// @param centralTabs The tabs of the documents, whose editors are checked
    /// @param statusBar Where short messages about the grammar are shown (may be null)
    /// @param parent Parent object
    GrammarCoordinator(QTabWidget* centralTabs, QStatusBar* statusBar,
                       QObject* parent = nullptr);

    /// @brief Destructor: the command has no callbacks any more; the server is closed
    ~GrammarCoordinator() override;

    GrammarCoordinator(const GrammarCoordinator&) = delete;
    GrammarCoordinator& operator=(const GrammarCoordinator&) = delete;

    /// @brief Give the command Check Grammar as You Type its callbacks and check mark
    /// @note Call after the commands are registered
    void connectCommands();

    /// @brief The checking the editors send their paragraphs to
    editor::GrammarCheckService* service() const { return m_service; }

    /// @brief The LanguageTool server on this computer
    LanguageToolServer* server() const { return m_server; }

    /// @brief The preparation of Java and LanguageTool
    GrammarSetup* setup() const { return m_setup; }

    /// @brief What grammar as you type can do now
    Readiness readiness() const;

    /// @brief What grammar as you type can do with the values given (those of the settings,
    ///        or those a page of the Settings dialog shows)
    /// @param remote On a server in the internet
    /// @param external On a server started outside the program
    /// @param java The java program of the server the program starts
    /// @param folder The folder of LanguageTool of the server the program starts
    static Readiness readinessOf(bool remote, bool external, const QString& java,
                                 const QString& folder);

    /// @brief Whether the grammar is checked now: on, and ready or on a server in the
    ///        internet
    bool isChecking() const;

    /// @brief Show the window of the preparation: what is downloaded, or how the
    ///        preparation goes, or how it failed
    /// @param parent The window it is shown over (null: the main window)
    void showSetup(QWidget* parent = nullptr);

    /// @brief Ask whether to prepare the grammar now, and show the preparation if so
    /// @param parent The window the question is shown over (null: the main window)
    void askToPrepare(QWidget* parent = nullptr);

    /// @brief Remove Java and LanguageTool the program downloaded; the checking on this
    ///        computer is off until they are prepared again
    /// @return Whether everything was removed
    bool removeDownloaded();

    /// @brief The rules ignored, as the settings keep them: what they look for, by rule
    static QMap<QString, QString> ignoredRules();

    /// @brief Keep the rules ignored in the settings
    static void setIgnoredRules(const QMap<QString, QString>& rules);

public slots:
    /// @brief Check where, and in the language, the settings and the book give now
    void updateChecking();

private:
    /// @brief Give the checking to the editors of the documents; start the server on this
    ///        computer when one needs it
    void attachEditors();

    /// @brief Whether any document is open
    bool hasEditors() const;

    /// @brief Start the server on this computer when the checking needs it
    void startServerWhenNeeded();

    /// @brief The server started, failed or stopped
    void onServerState();

    /// @brief The button in the status bar: the preparation going on, or failed, while its
    ///        window is closed
    void updateSetupButton();

    /// @brief The preparation ended
    void onSetupFinished();

    /// @brief The server did not check a text
    void onServerError(const QString& message);

    /// @brief Turn the checking on or off (the command)
    void toggle();

    /// @brief Show a short message in the status bar
    void showMessage(const QString& message, int timeout);

    /// @brief Take a message away from the status bar, if it is still shown
    void clearMessage(const QString& message);

    /// @brief The button in the status bar for the preparation in the background
    QToolButton* setupButton();

    /// @brief The window dialogs are shown over
    QWidget* mainWindow() const;

    QTabWidget* m_centralTabs;
    QStatusBar* m_statusBar;
    editor::GrammarCheckService* m_service;
    LanguageToolServer* m_server;
    GrammarSetup* m_setup;
    QToolButton* m_setupButton{nullptr};  ///< In the status bar, which may delete it first
    dialogs::GrammarSetupDialog* m_setupDialog{nullptr};  ///< While its window is open
    int m_settingsListener{0};
    bool m_restarting{false};     ///< The server stopped by itself and is started again
    bool m_failureUnseen{false};  ///< The preparation failed while its window was closed
};

}  // namespace kalahari::gui
