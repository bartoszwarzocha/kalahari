/// @file grammar_coordinator.cpp
/// @brief Grammar as you type: the LanguageTool server of the editors, the language and the
///        command

#include "kalahari/gui/grammar_coordinator.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/grammar_check_service.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/spelling_coordinator.h"
#include "kalahari/gui/utils/setting_toggle.h"

#include <QMetaObject>
#include <QStatusBar>
#include <QTabWidget>
#include <QUrl>

#include <string>

namespace kalahari::gui {

namespace {

/// @brief The setting that turns the checking on and off
constexpr const char* ENABLED_KEY = "editor.grammarCheck.enabled";

/// @brief The setting of the LanguageTool server (empty: none)
constexpr const char* SERVER_KEY = "editor.grammarCheck.serverUrl";

/// @brief The command that turns the checking on and off
constexpr const char* COMMAND_ID = "tools.grammar";

/// @brief How long the message of the command stays in the status bar (ms), as the
///        messages of the other switches
constexpr int TOGGLE_MESSAGE_MS = 2000;

/// @brief How long a message about the server stays in the status bar (ms)
constexpr int SERVER_MESSAGE_MS = 10000;

}  // namespace

GrammarCoordinator::GrammarCoordinator(QTabWidget* centralTabs, QStatusBar* statusBar,
                                       QObject* parent)
    : QObject(parent)
    , m_centralTabs(centralTabs)
    , m_statusBar(statusBar)
    , m_service(new editor::GrammarCheckService(this))
{
    connect(m_service, &editor::GrammarCheckService::serverError, this,
            [this](const QString& message) {
                const QUrl server = QUrl(m_service->endpoint()).adjusted(QUrl::RemovePath);
                showMessage(tr("The LanguageTool server at %1 did not check the text: %2")
                                .arg(server.toString(), message),
                            SERVER_MESSAGE_MS);
            });

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
    }
}

void GrammarCoordinator::connectCommands() {
    auto& registry = CommandRegistry::getInstance();
    if (Command* command = registry.getCommand(COMMAND_ID)) {
        command->execute = [this]() { toggle(); };
        command->isEnabled = [this]() { return m_service->isConfigured(); };
        registry.updateActionState(COMMAND_ID);
    }

    // Checked while the grammar is checked, also after the Settings dialog changes it
    utils::followSetting(registry.getAction(std::string(COMMAND_ID)), ENABLED_KEY);
}

void GrammarCoordinator::updateChecking() {
    auto& settings = core::SettingsManager::getInstance();
    m_service->setEnabled(settings.get<bool>(ENABLED_KEY, true));
    m_service->setServer(
        QString::fromStdString(settings.get<std::string>(SERVER_KEY, std::string())));
    m_service->setLanguage(SpellingCoordinator::wantedLanguage());

    // Greyed out while no server is set
    CommandRegistry::getInstance().updateActionState(COMMAND_ID);
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
}

void GrammarCoordinator::toggle() {
    auto& settings = core::SettingsManager::getInstance();

    // A setting of all editors, kept between sessions
    const bool enabled = !settings.get<bool>(ENABLED_KEY, true);
    settings.set<bool>(ENABLED_KEY, enabled);
    core::Logger::getInstance().info("Action triggered: Check Grammar as You Type {}",
                                     enabled ? "on" : "off");
    showMessage(enabled ? tr("Grammar as you type: on") : tr("Grammar as you type: off"),
                TOGGLE_MESSAGE_MS);
}

void GrammarCoordinator::showMessage(const QString& message, int timeout) {
    if (m_statusBar != nullptr) {
        m_statusBar->showMessage(message, timeout);
    }
}

}  // namespace kalahari::gui
