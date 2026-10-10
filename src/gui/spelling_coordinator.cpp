/// @file spelling_coordinator.cpp
/// @brief Spelling as you type: the dictionary of the editors, its language and its command

#include "kalahari/gui/spelling_coordinator.h"
#include "kalahari/core/document.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/spell_check_service.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/utils/language_names.h"
#include "kalahari/gui/utils/setting_toggle.h"

#include <QDir>
#include <QMetaObject>
#include <QStatusBar>
#include <QTabWidget>

#include <string>

namespace kalahari::gui {

namespace {

/// @brief The setting that turns the checking on and off
constexpr const char* ENABLED_KEY = "editor.spellCheck.enabled";

/// @brief The setting of the language checked (empty: the book's)
constexpr const char* LANGUAGE_KEY = "editor.spellCheck.language";

/// @brief The command that turns the checking on and off
constexpr const char* COMMAND_ID = "tools.spellcheck";

/// @brief The command that selects the next misspelled word
constexpr const char* NEXT_COMMAND_ID = "tools.nextMisspelling";

/// @brief How long the message of the command stays in the status bar (ms), as the
///        messages of the other switches
constexpr int TOGGLE_MESSAGE_MS = 2000;

/// @brief How long a message about a missing dictionary stays in the status bar (ms)
constexpr int DICTIONARY_MESSAGE_MS = 5000;

/// @brief How long the message that there is no misspelled word stays in the status bar
///        (ms), as the message that there is no to-do
constexpr int NEXT_MESSAGE_MS = 3000;

}  // namespace

SpellingCoordinator::SpellingCoordinator(QTabWidget* centralTabs, QStatusBar* statusBar,
                                         QObject* parent)
    : QObject(parent)
    , m_centralTabs(centralTabs)
    , m_statusBar(statusBar)
    , m_service(new editor::SpellCheckService(this))
{
    m_service->setUserDictionaryFile(userDictionaryFile());
    connect(m_service, &editor::SpellCheckService::dictionaryError, this,
            [this](const QString& error) { showMessage(error, DICTIONARY_MESSAGE_MS); });

    // The editor of a document checks with the dictionary from the time its tab is shown;
    // Next Misspelling is for the document in front
    if (m_centralTabs != nullptr) {
        connect(m_centralTabs, &QTabWidget::currentChanged, this, [this]() {
            attachEditors();
            CommandRegistry::getInstance().updateActionState(NEXT_COMMAND_ID);
        });
    }

    // Another book can be in another language
    auto& projects = core::ProjectManager::getInstance();
    connect(&projects, &core::ProjectManager::projectOpened, this,
            &SpellingCoordinator::updateDictionary);
    connect(&projects, &core::ProjectManager::projectClosed, this,
            &SpellingCoordinator::updateDictionary);

    // The settings of the spelling, from the Settings dialog or the command. A listener
    // runs on the thread that changed the setting: the slot is queued.
    m_settingsListener = core::SettingsManager::getInstance().subscribe(
        [this](const std::string& key) {
            if (key.rfind("editor.spellCheck.", 0) == 0 || key == "ui.language") {
                QMetaObject::invokeMethod(this, "updateDictionary", Qt::QueuedConnection);
            }
        });

    updateDictionary();
    attachEditors();
}

SpellingCoordinator::~SpellingCoordinator() {
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);

    // The commands outlive the coordinator
    auto& registry = CommandRegistry::getInstance();
    for (const char* id : {COMMAND_ID, NEXT_COMMAND_ID}) {
        if (Command* command = registry.getCommand(id)) {
            command->execute = nullptr;
            command->isEnabled = nullptr;
        }
    }
}

void SpellingCoordinator::connectCommands() {
    auto& registry = CommandRegistry::getInstance();
    if (Command* command = registry.getCommand(COMMAND_ID)) {
        command->execute = [this]() { toggle(); };
        registry.updateActionState(COMMAND_ID);
    }

    // Checked while the spelling is checked, also after the Settings dialog changes it
    utils::followSetting(registry.getAction(std::string(COMMAND_ID)), ENABLED_KEY);

    if (Command* next = registry.getCommand(NEXT_COMMAND_ID)) {
        next->execute = [this]() { goToNextMisspelling(); };
        next->isEnabled = [this]() { return currentEditor() != nullptr; };
        registry.updateActionState(NEXT_COMMAND_ID);
    }
}

bool SpellingCoordinator::goToNextMisspelling() {
    editor::BookEditor* bookEditor = currentEditor();
    if (bookEditor == nullptr) {
        return false;
    }
    if (!m_service->isActive()) {
        showMessage(tr("The spelling is not checked"), NEXT_MESSAGE_MS);
        return false;
    }
    if (!bookEditor->goToNextMisspelling()) {
        showMessage(tr("No misspelled words"), NEXT_MESSAGE_MS);
        return false;
    }

    // What to put in its place, under the word, as the menu key shows it
    bookEditor->setFocus();
    bookEditor->showContextMenuAtCursor();
    return true;
}

QString SpellingCoordinator::wantedLanguage() {
    auto& settings = core::SettingsManager::getInstance();
    QString language =
        QString::fromStdString(settings.get<std::string>(LANGUAGE_KEY, std::string())).trimmed();
    if (!language.isEmpty()) {
        return language;
    }

    auto& projects = core::ProjectManager::getInstance();
    if (const core::Document* document =
            projects.isProjectOpen() ? projects.getDocument() : nullptr) {
        QString bookLanguage = QString::fromStdString(document->getLanguage()).trimmed();
        if (!bookLanguage.isEmpty()) {
            return bookLanguage;
        }
    }
    return QString::fromStdString(settings.getLanguage());
}

QString SpellingCoordinator::userDictionaryFile() {
    const QDir settingsFolder(QString::fromStdU16String(
        core::SettingsManager::getInstance().getSettingsFilePath().parent_path().u16string()));
    return settingsFolder.filePath(QStringLiteral("user_dictionary.txt"));
}

void SpellingCoordinator::updateDictionary() {
    auto& logger = core::Logger::getInstance();
    const bool enabled = core::SettingsManager::getInstance().get<bool>(ENABLED_KEY, true);
    m_service->setEnabled(enabled);
    if (!enabled) {
        // Its dictionary is loaded when the checking is turned on; the writer is told again
        // of a missing one then
        m_missingLanguage.clear();
        return;
    }

    const QString language = wantedLanguage();
    const QString dictionary = editor::SpellCheckService::dictionaryFor(language);
    if (!dictionary.isEmpty()) {
        m_missingLanguage.clear();
        m_service->loadDictionaryInBackground(dictionary);
        return;
    }

    m_service->unloadDictionary();
    if (language != m_missingLanguage) {
        m_missingLanguage = language;
        logger.info("SpellingCoordinator: No dictionary for the language '{}'",
                    language.toStdString());
        //: %1 is the name of the language as in a list of languages (in Polish: francuski)
        showMessage(tr("No spelling dictionary for %1: the spelling is not checked")
                        .arg(utils::languageName(language)),
                    DICTIONARY_MESSAGE_MS);
    }
}

void SpellingCoordinator::attachEditors() {
    if (m_centralTabs == nullptr) {
        return;
    }
    for (int index = 0; index < m_centralTabs->count(); ++index) {
        auto* panel = qobject_cast<EditorPanel*>(m_centralTabs->widget(index));
        if (panel != nullptr && panel->getBookEditor() != nullptr) {
            panel->getBookEditor()->setSpellCheckService(m_service);
        }
    }
}

editor::BookEditor* SpellingCoordinator::currentEditor() const {
    auto* panel = m_centralTabs != nullptr
                      ? qobject_cast<EditorPanel*>(m_centralTabs->currentWidget())
                      : nullptr;
    return panel != nullptr ? panel->getBookEditor() : nullptr;
}

void SpellingCoordinator::toggle() {
    auto& settings = core::SettingsManager::getInstance();

    // A setting of all editors, kept between sessions
    const bool enabled = !settings.get<bool>(ENABLED_KEY, true);
    settings.set<bool>(ENABLED_KEY, enabled);
    core::Logger::getInstance().info("Action triggered: Check Spelling as You Type {}",
                                     enabled ? "on" : "off");
    showMessage(enabled ? tr("Spelling as you type: on") : tr("Spelling as you type: off"),
                TOGGLE_MESSAGE_MS);
}

void SpellingCoordinator::showMessage(const QString& message, int timeout) {
    if (m_statusBar != nullptr) {
        m_statusBar->showMessage(message, timeout);
    }
}

}  // namespace kalahari::gui
