/// @file grammar_coordinator.cpp
/// @brief Grammar as you type: where the text is checked (LanguageTool on this computer or a
///        server in the internet), the preparation of LanguageTool, the language and the
///        command

#include "kalahari/gui/grammar_coordinator.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/grammar_check_service.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/grammar_setup_dialog.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/grammar_components.h"
#include "kalahari/gui/grammar_setup.h"
#include "kalahari/gui/language_tool_server.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/spelling_coordinator.h"

#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolButton>
#include <QUrl>

#include <nlohmann/json.hpp>

#include <string>

namespace kalahari::gui {

namespace {

/// @brief The command that turns the checking on and off
constexpr const char* COMMAND_ID = "tools.grammar";

/// @brief How long the message of the command stays in the status bar (ms), as the
///        messages of the other switches
constexpr int TOGGLE_MESSAGE_MS = 2000;

/// @brief How long a message about the server stays in the status bar (ms)
constexpr int SERVER_MESSAGE_MS = 10000;

/// @brief How long the status bar says the preparation is done (ms)
constexpr int READY_MESSAGE_MS = 5000;

/// @brief The server of LanguageTool, in its folder
constexpr const char* SERVER_JAR = "languagetool-server.jar";

/// @brief The status bar while the server starts
QString startingMessage() {
    return GrammarCoordinator::tr("Starting LanguageTool...");
}

/// @brief The status bar while the server is started again after it stopped by itself
QString restartingMessage() {
    return GrammarCoordinator::tr("Grammar is not checked: LanguageTool stopped answering. The "
                                  "program starts it again.");
}

/// @brief The status bar when the server did not start
QString failedMessage() {
    return GrammarCoordinator::tr("Grammar is not checked: LanguageTool did not start. Details: "
                                  "Edit > Settings..., Editor > Grammar.");
}

/// @brief A text setting
QString textSetting(const char* key) {
    return QString::fromStdString(
        core::SettingsManager::getInstance().get<std::string>(key, std::string()));
}

/// @brief Whether a path is in a folder (or is the folder)
bool isInFolder(const QString& path, const QString& folder) {
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity CASE = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity CASE = Qt::CaseSensitive;
#endif
    if (path.isEmpty() || folder.isEmpty()) {
        return false;
    }
    const QString cleanPath = QDir::cleanPath(QDir::fromNativeSeparators(path));
    const QString cleanFolder = QDir::cleanPath(QDir::fromNativeSeparators(folder));
    return cleanPath.compare(cleanFolder, CASE) == 0 ||
           cleanPath.startsWith(cleanFolder + QLatin1Char('/'), CASE);
}

}  // namespace

GrammarCoordinator::GrammarCoordinator(QTabWidget* centralTabs, QStatusBar* statusBar,
                                       QObject* parent)
    : QObject(parent)
    , m_centralTabs(centralTabs)
    , m_statusBar(statusBar)
    , m_service(new editor::GrammarCheckService(this))
    , m_server(new LanguageToolServer(this))
    , m_setup(new GrammarSetup(m_server, this))
{
    connect(m_service, &editor::GrammarCheckService::serverError, this,
            &GrammarCoordinator::onServerError);

    // A server that did not answer is asked again after a while, and answers again
    connect(m_service, &editor::GrammarCheckService::available, this,
            &GrammarCoordinator::problemChanged);
    connect(m_service, &editor::GrammarCheckService::textChecked, this, [this]() {
        if (!m_serverError.isEmpty()) {
            m_serverError.clear();
            emit problemChanged();
        }
    });

    // A rule ignored in an editor is ignored in every book, also after a restart
    connect(m_service, &editor::GrammarCheckService::ruleIgnored, this,
            [](const QString& ruleId, const QString& description) {
                QMap<QString, QString> rules = ignoredRules();
                if (!rules.contains(ruleId)) {
                    rules.insert(ruleId, description);
                    setIgnoredRules(rules);
                }
            });

    connect(m_server, &LanguageToolServer::stateChanged, this,
            &GrammarCoordinator::onServerState);
    // Another program took the port: the one the server listens on is kept for next time
    connect(m_server, &LanguageToolServer::portChanged, this, [](int port) {
        core::SettingsManager::getInstance().set<int>(PORT_KEY, port);
    });
    connect(m_server, &LanguageToolServer::restarting, this, [this]() {
        m_restarting = true;
        showMessage(restartingMessage(), 0);
        emit problemChanged();
    });

    connect(m_setup, &GrammarSetup::progress, this, &GrammarCoordinator::updateSetupButton);
    connect(m_setup, &GrammarSetup::finished, this, &GrammarCoordinator::onSetupFinished);

    // The editor of a document is checked from the time its tab is shown
    if (m_centralTabs != nullptr) {
        connect(m_centralTabs, &QTabWidget::currentChanged, this,
                &GrammarCoordinator::attachEditors);
    }

    // Another book can be in another language
    auto& projects = core::ProjectManager::getInstance();
    connect(&projects, &core::ProjectManager::projectOpened, this,
            &GrammarCoordinator::updateChecking);
    connect(&projects, &core::ProjectManager::projectClosed, this,
            &GrammarCoordinator::updateChecking);

    // The settings of the grammar and of the language, from the Settings dialog or the
    // command. A listener runs on the thread that changed the setting: the slot is queued.
    m_settingsListener = core::SettingsManager::getInstance().subscribe(
        [this](const std::string& key) {
            if (key.rfind("editor.grammarCheck.", 0) == 0 ||
                key == "editor.spellCheck.language" || key == "ui.language") {
                QMetaObject::invokeMethod(this, "updateChecking", Qt::QueuedConnection);
            }
        });

    updateChecking();
    attachEditors();
}

GrammarCoordinator::~GrammarCoordinator() {
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);

    // The command outlives the coordinator
    if (Command* command = CommandRegistry::getInstance().getCommand(COMMAND_ID)) {
        command->execute = nullptr;
        command->isEnabled = nullptr;
        command->isChecked = nullptr;
    }
    delete m_setupButton;
}

void GrammarCoordinator::connectCommands() {
    auto& registry = CommandRegistry::getInstance();
    if (Command* command = registry.getCommand(COMMAND_ID)) {
        command->execute = [this]() { toggle(); };
        // Checked while the grammar is checked: on, and something to check it on
        command->isChecked = [this]() { return isChecking(); };
        registry.updateActionState(COMMAND_ID);
    }
}

// =============================================================================
// What the settings give
// =============================================================================

GrammarCoordinator::Readiness GrammarCoordinator::readiness() const {
    auto& settings = core::SettingsManager::getInstance();
    return readinessOf(settings.get<std::string>(MODE_KEY, LOCAL_MODE) == REMOTE_MODE,
                       settings.get<bool>(EXTERNAL_KEY, false), textSetting(JAVA_KEY),
                       textSetting(FOLDER_KEY));
}

GrammarCoordinator::Readiness GrammarCoordinator::readinessOf(bool remote, bool external,
                                                              const QString& java,
                                                              const QString& folder) {
    if (remote) {
        return Readiness::Remote;
    }
    if (external) {
        return Readiness::Ready;
    }
    if (java.trimmed().isEmpty() || folder.trimmed().isEmpty()) {
        return Readiness::NotPrepared;
    }
    const bool found =
        QFileInfo(java.trimmed()).isFile() &&
        QFileInfo(QDir(folder.trimmed()).filePath(QLatin1String(SERVER_JAR))).isFile();
    return found ? Readiness::Ready : Readiness::Missing;
}

bool GrammarCoordinator::isChecking() const {
    const Readiness ready = readiness();
    return core::SettingsManager::getInstance().get<bool>(ENABLED_KEY, false) &&
           (ready == Readiness::Ready || ready == Readiness::Remote);
}

QString GrammarCoordinator::problem() const {
    if (!core::SettingsManager::getInstance().get<bool>(ENABLED_KEY, false)) {
        return {};
    }
    if (m_setup->isRunning()) {
        return tr("Grammar is not checked yet: the program prepares LanguageTool.");
    }
    switch (readiness()) {
    case Readiness::NotPrepared:
        return tr("Grammar is not checked: it is not prepared yet. Tools > Check Grammar as You "
                  "Type prepares it.");
    case Readiness::Missing:
        return tr("Grammar is not checked: the components it needs were not found. Tools > Check "
                  "Grammar as You Type prepares them again.");
    case Readiness::Ready:
        if (m_restarting) {
            return restartingMessage();
        }
        if (m_server->state() == LanguageToolServer::State::Starting) {
            return startingMessage();
        }
        if (m_server->state() == LanguageToolServer::State::Failed) {
            return failedMessage();
        }
        break;
    case Readiness::Remote:
        break;
    }

    // The server did not answer: it is asked again after a while
    if (!m_serverError.isEmpty() && m_service->isEnabled() && m_service->isConfigured() &&
        !m_service->isActive()) {
        return m_serverError;
    }
    return {};
}

QMap<QString, QString> GrammarCoordinator::ignoredRules() {
    QMap<QString, QString> rules;
    const nlohmann::json stored = core::SettingsManager::getInstance().get<nlohmann::json>(
        IGNORED_RULES_KEY, nlohmann::json::object());
    if (!stored.is_object()) {
        return rules;
    }
    for (const auto& item : stored.items()) {
        const QString description = item.value().is_string()
                                        ? QString::fromStdString(item.value().get<std::string>())
                                        : QString();
        rules.insert(QString::fromStdString(item.key()), description);
    }
    return rules;
}

void GrammarCoordinator::setIgnoredRules(const QMap<QString, QString>& rules) {
    nlohmann::json stored = nlohmann::json::object();
    for (auto it = rules.cbegin(); it != rules.cend(); ++it) {
        stored[it.key().toStdString()] = it.value().toStdString();
    }
    core::SettingsManager::getInstance().set<nlohmann::json>(IGNORED_RULES_KEY, stored);
}

// =============================================================================
// The checking
// =============================================================================

void GrammarCoordinator::updateChecking() {
    auto& settings = core::SettingsManager::getInstance();
    m_service->setLanguage(SpellingCoordinator::wantedLanguage());
    m_service->setIgnoredRules(ignoredRules());

    const Readiness ready = readiness();
    const bool enabled = settings.get<bool>(ENABLED_KEY, false);
    const bool preparing = m_setup->isRunning();

    // The server on this computer as the settings give it; the preparation sets it itself
    if (!preparing) {
        m_server->setJava(textSetting(JAVA_KEY));
        m_server->setFolder(textSetting(FOLDER_KEY));
        m_server->setPort(settings.get<int>(PORT_KEY, LanguageToolServer::DEFAULT_PORT));
        m_server->setExternal(settings.get<bool>(EXTERNAL_KEY, false));
    }

    if (ready == Readiness::Remote) {
        // Nothing runs on this computer for a server in the internet
        if (!preparing) {
            m_server->stop();
        }
        m_service->setCredentials(textSetting(USERNAME_KEY), textSetting(API_KEY_KEY));
        m_service->setServer(textSetting(SERVER_KEY));
    } else {
        if (!enabled && !preparing) {
            m_server->stop();
        }
        m_service->setCredentials(QString(), QString());
        m_service->setServer(m_server->state() == LanguageToolServer::State::Running
                                 ? m_server->address()
                                 : QString());
    }
    m_service->setEnabled(enabled && (ready == Readiness::Ready || ready == Readiness::Remote));

    startServerWhenNeeded();
    CommandRegistry::getInstance().updateActionState(COMMAND_ID);
    emit problemChanged();
}

void GrammarCoordinator::attachEditors() {
    if (m_centralTabs == nullptr) {
        return;
    }
    for (int index = 0; index < m_centralTabs->count(); ++index) {
        auto* panel = qobject_cast<EditorPanel*>(m_centralTabs->widget(index));
        if (panel != nullptr && panel->getBookEditor() != nullptr) {
            panel->getBookEditor()->setGrammarCheckService(m_service);
        }
    }
    startServerWhenNeeded();
}

bool GrammarCoordinator::hasEditors() const {
    if (m_centralTabs == nullptr) {
        return false;
    }
    for (int index = 0; index < m_centralTabs->count(); ++index) {
        auto* panel = qobject_cast<EditorPanel*>(m_centralTabs->widget(index));
        if (panel != nullptr && panel->getBookEditor() != nullptr) {
            return true;
        }
    }
    return false;
}

void GrammarCoordinator::startServerWhenNeeded() {
    // Java takes memory and time to start: only when a document is checked on this computer.
    // After a failure it waits for another setting or a start from the Settings dialog.
    if (m_setup->isRunning() || !isChecking() || readiness() != Readiness::Ready ||
        !hasEditors()) {
        return;
    }
    if (m_server->state() == LanguageToolServer::State::Stopped) {
        m_server->start();
    }
}

void GrammarCoordinator::onServerState() {
    const bool local = readiness() != Readiness::Remote;
    const bool preparing = m_setup->isRunning();
    const LanguageToolServer::State state = m_server->state();
    if (local) {
        m_service->setServer(state == LanguageToolServer::State::Running ? m_server->address()
                                                                         : QString());
    }

    switch (state) {
    case LanguageToolServer::State::Starting:
        if (local && !preparing && !m_restarting) {
            showMessage(startingMessage(), 0);
        }
        break;
    case LanguageToolServer::State::Running:
        clearMessage(startingMessage());
        clearMessage(restartingMessage());
        m_restarting = false;
        break;
    case LanguageToolServer::State::Failed:
        m_restarting = false;
        clearMessage(startingMessage());
        clearMessage(restartingMessage());
        if (local && !preparing) {
            showMessage(failedMessage(), SERVER_MESSAGE_MS);
        }
        break;
    case LanguageToolServer::State::Stopped:
        m_restarting = false;
        clearMessage(startingMessage());
        clearMessage(restartingMessage());
        break;
    }
    emit problemChanged();
}

void GrammarCoordinator::onServerError(const QString& message) {
    // Kept until the server answers again (problem())
    const auto show = [this](const QString& text) {
        m_serverError = text;
        showMessage(text, SERVER_MESSAGE_MS);
        emit problemChanged();
    };
    if (readiness() == Readiness::Remote) {
        if (m_service->lastFailure() == editor::GrammarCheckService::Failure::Limit) {
            show(tr("The server refused the request because the limit was exceeded. The program "
                    "will try again in a moment."));
            return;
        }
        const QUrl server = QUrl(m_service->endpoint()).adjusted(QUrl::RemovePath);
        show(tr("The LanguageTool server at %1 did not check the text: %2")
                 .arg(server.toString(), message));
        return;
    }
    // While the server starts or starts again, the status bar already says so
    if (m_server->state() == LanguageToolServer::State::Running) {
        show(tr("LanguageTool on this computer did not check the text: %1").arg(message));
    }
}

void GrammarCoordinator::toggle() {
    const Readiness ready = readiness();
    const bool checking = isChecking();
    if (!checking && (ready == Readiness::NotPrepared || ready == Readiness::Missing)) {
        // Not checked until something is prepared: the check mark stays off
        CommandRegistry::getInstance().updateActionState(COMMAND_ID);
        askToPrepare();
        return;
    }

    // A setting of all editors, kept between sessions
    const bool enabled = !checking;
    core::SettingsManager::getInstance().set<bool>(ENABLED_KEY, enabled);
    core::Logger::getInstance().info("Action triggered: Check Grammar as You Type {}",
                                     enabled ? "on" : "off");
    showMessage(enabled ? tr("Grammar as you type: on") : tr("Grammar as you type: off"),
                TOGGLE_MESSAGE_MS);
}

// =============================================================================
// The preparation
// =============================================================================

void GrammarCoordinator::askToPrepare(QWidget* parent) {
    const QString first =
        (readiness() == Readiness::Missing
             ? tr("The components grammar checking needs were not found: the files of "
                  "LanguageTool or Java were removed or moved. The program can download and "
                  "prepare them again - about %1 to download.")
             : tr("Grammar checking is not prepared yet. It needs two free components: "
                  "LanguageTool and Java. The program downloads and prepares them itself - "
                  "about %1 to download."))
            .arg(GrammarComponents::downloadSizeText());
    dialogs::MessageDialog dialog(
        dialogs::MessageDialog::Kind::Question, tr("Prepare grammar checking?"),
        first + QStringLiteral("\n\n") +
            tr("Checking on a server in the internet can be chosen in the settings: Edit > "
               "Settings..., group Editor > Grammar."),
        parent != nullptr ? parent : mainWindow());
    dialog.setWindowTitle(tr("Grammar"));
    dialog.setQuestionButtons(tr("&Prepare Now"), tr("&Not Now"));
    dialog.exec();
    if (dialog.answer() == dialogs::MessageDialog::Answer::Accept) {
        showSetup(parent);
    }
}

void GrammarCoordinator::showSetup(QWidget* parent) {
    if (m_setupDialog != nullptr) {
        m_setupDialog->raise();
        m_setupDialog->activateWindow();
        return;
    }
    if (!m_setup->isRunning()) {
        m_setup->setLanguage(SpellingCoordinator::wantedLanguage());
    }

    // The window shows the progress or the failure: not the status bar meanwhile
    dialogs::GrammarSetupDialog dialog(m_setup, parent != nullptr ? parent : mainWindow());
    m_setupDialog = &dialog;
    m_failureUnseen = false;
    updateSetupButton();
    dialog.exec();
    m_setupDialog = nullptr;
    updateSetupButton();
}

void GrammarCoordinator::updateSetupButton() {
    emit problemChanged();  // the preparation started, goes on or ended
    if (m_statusBar == nullptr) {
        return;
    }
    QString text;
    if (m_setupDialog == nullptr && m_setup->isRunning()) {
        QString step;
        switch (m_setup->step()) {
        case GrammarSetup::Step::Java:
            step = tr("downloading Java");
            break;
        case GrammarSetup::Step::LanguageTool:
            step = tr("downloading LanguageTool");
            break;
        case GrammarSetup::Step::Unpack:
        case GrammarSetup::Step::FirstRun:
            step = tr("preparing");
            break;
        }
        text = tr("Grammar: %1 - %2%").arg(step).arg(m_setup->percent());
    } else if (m_setupDialog == nullptr && m_failureUnseen &&
               m_setup->state() == GrammarSetup::State::Failed) {
        text = tr("Grammar checking could not be prepared - click to see the details");
    }

    if (text.isEmpty()) {
        if (m_setupButton != nullptr) {
            m_setupButton->hide();
        }
        return;
    }
    QToolButton* button = setupButton();
    button->setText(text);
    button->show();
}

void GrammarCoordinator::onSetupFinished() {
    if (m_setup->state() == GrammarSetup::State::Finished) {
        // Checked from now on, on this computer, with what the program downloaded
        auto& settings = core::SettingsManager::getInstance();
        settings.set<std::string>(JAVA_KEY, GrammarComponents::downloadedJava().toStdString());
        settings.set<std::string>(FOLDER_KEY,
                                  GrammarComponents::languageToolFolder().toStdString());
        settings.set<bool>(EXTERNAL_KEY, false);
        settings.set<std::string>(MODE_KEY, LOCAL_MODE);
        settings.set<bool>(ENABLED_KEY, true);
        if (m_setupDialog == nullptr) {
            showMessage(tr("Grammar checking is ready"), READY_MESSAGE_MS);
        }
    }
    m_failureUnseen =
        m_setup->state() == GrammarSetup::State::Failed && m_setupDialog == nullptr;
    updateSetupButton();
    updateChecking();
}

bool GrammarCoordinator::removeDownloaded() {
    if (m_setup->isRunning()) {
        return false;
    }
    m_server->stop();
    const bool removed = GrammarComponents::removeDownloaded();
    core::Logger::getInstance().info("GrammarCoordinator: Downloaded components removed: {}",
                                     removed);

    // The settings that pointed at them are emptied: nothing is checked on this computer
    auto& settings = core::SettingsManager::getInstance();
    const QString folder = GrammarComponents::folder();
    bool cleared = false;
    for (const char* key : {JAVA_KEY, FOLDER_KEY}) {
        if (isInFolder(textSetting(key), folder)) {
            settings.set<std::string>(key, std::string());
            cleared = true;
        }
    }
    if (cleared && readiness() != Readiness::Remote) {
        settings.set<bool>(ENABLED_KEY, false);
    }
    m_failureUnseen = false;
    updateSetupButton();
    updateChecking();
    return removed;
}

// =============================================================================
// Helpers
// =============================================================================

void GrammarCoordinator::showMessage(const QString& message, int timeout) {
    if (m_statusBar != nullptr) {
        m_statusBar->showMessage(message, timeout);
    }
}

void GrammarCoordinator::clearMessage(const QString& message) {
    if (m_statusBar != nullptr && m_statusBar->currentMessage() == message) {
        m_statusBar->clearMessage();
    }
}

QToolButton* GrammarCoordinator::setupButton() {
    if (m_setupButton == nullptr) {
        auto* button = new QToolButton(m_statusBar);
        button->setObjectName(QStringLiteral("grammarSetupButton"));
        button->setAutoRaise(true);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setToolTip(tr("Show the preparation of grammar checking"));
        connect(button, &QToolButton::clicked, this, [this]() { showSetup(); });
        // Before the counts and the zoom of the status bar
        m_statusBar->insertPermanentWidget(0, button);
        m_setupButton = button;
        // The status bar may delete it before the coordinator goes
        connect(button, &QObject::destroyed, this, [this]() { m_setupButton = nullptr; });
    }
    return m_setupButton;
}

QWidget* GrammarCoordinator::mainWindow() const {
    if (m_centralTabs != nullptr) {
        return m_centralTabs->window();
    }
    return m_statusBar != nullptr ? m_statusBar->window() : nullptr;
}

}  // namespace kalahari::gui
