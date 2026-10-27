/// @file spelling_coordinator.cpp
/// @brief Spelling as you type: the dictionaries of the editors, their languages and the
///        commands

#include "kalahari/gui/spelling_coordinator.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/grammar_check_service.h"
#include "kalahari/editor/spell_check_service.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/utils/language_names.h"
#include "kalahari/gui/utils/setting_toggle.h"

#include <QDir>
#include <QMetaObject>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QVariant>

#include <string>
#include <utility>

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

/// @brief The setting of where the command shows the next issue: "panel" (the handler of the
///        next issue) or "menu" (its context menu at the word)
constexpr const char* NEXT_ISSUE_KEY = "spellingPanel.nextIssue";

/// @brief How long the message of the command stays in the status bar (ms), as the
///        messages of the other switches
constexpr int TOGGLE_MESSAGE_MS = 2000;

/// @brief How long a message about a missing dictionary stays in the status bar (ms)
constexpr int DICTIONARY_MESSAGE_MS = 5000;

/// @brief How long the message that there is no misspelled word stays in the status bar
///        (ms), as the message that there is no to-do
constexpr int NEXT_MESSAGE_MS = 3000;

/// @brief The language chosen in the settings (empty: the book's)
QString chosenLanguage() {
    return QString::fromStdString(core::SettingsManager::getInstance().get<std::string>(
                                      LANGUAGE_KEY, std::string()))
        .trimmed();
}

/// @brief The language of the program
QString programLanguage() {
    return QString::fromStdString(core::SettingsManager::getInstance().getLanguage());
}

/// @brief Whether an editor shows a chapter of the book, which NavigatorCoordinator tags
///        with its element, not a document outside the book
bool isChapter(const EditorPanel& panel) {
    return !panel.property("elementId").toString().isEmpty();
}

}  // namespace

SpellingCoordinator::SpellingCoordinator(QTabWidget* centralTabs, QStatusBar* statusBar,
                                         QObject* parent)
    : QObject(parent)
    , m_centralTabs(centralTabs)
    , m_statusBar(statusBar)
    , m_service(new editor::SpellCheckService(this))
    , m_otherService(new editor::SpellCheckService(this))
    , m_editorsTimer(new QTimer(this))
{
    // The writer's own words are the same in every language
    m_service->setUserDictionaryFile(userDictionaryFile());
    m_otherService->shareOwnWordsWith(*m_service);
    for (editor::SpellCheckService* service : {m_service, m_otherService}) {
        connect(service, &editor::SpellCheckService::dictionaryError, this,
                [this](const QString& error) { showMessage(error, DICTIONARY_MESSAGE_MS); });
        connect(service, &editor::SpellCheckService::dictionaryLoaded, this,
                &SpellingCoordinator::problemChanged);
        connect(service, &editor::SpellCheckService::dictionaryError, this,
                &SpellingCoordinator::problemChanged);
    }

    // The editor of a document checks with its dictionary from the time its tab is shown;
    // Next Spelling or Grammar Issue is for the document in front. A chapter is told from a
    // document outside the book only after its tab is shown (NavigatorCoordinator tags it
    // then), so the editors get their dictionaries when the event loop runs next.
    m_editorsTimer->setSingleShot(true);
    m_editorsTimer->setInterval(0);
    connect(m_editorsTimer, &QTimer::timeout, this, &SpellingCoordinator::updateEditors);
    if (m_centralTabs != nullptr) {
        connect(&DocumentWindows::of(*m_centralTabs), &DocumentWindows::frontChanged, this,
                [this]() {
            m_editorsTimer->start();
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
        next->execute = [this]() { goToNextIssue(); };
        next->isEnabled = [this]() { return currentEditor() != nullptr; };
        registry.updateActionState(NEXT_COMMAND_ID);
    }
}

bool SpellingCoordinator::goToNextIssue() {
    editor::BookEditor* bookEditor = currentEditor();
    if (bookEditor == nullptr) {
        return false;
    }
    if (m_editorsTimer->isActive()) {
        updateEditors();  // the document shown just now gets its dictionary first
    }

    // The grammar counts when its checking is on, also while its server does not answer:
    // the issues found stay
    const editor::SpellCheckService* spellService = bookEditor->spellCheckService();
    const editor::GrammarCheckService* grammarService = bookEditor->grammarCheckService();
    const bool spelling = spellService != nullptr && spellService->isActive();
    const bool grammar = grammarService != nullptr && grammarService->isEnabled() &&
                         grammarService->isConfigured();
    // The handler of the next issue (the Spelling and Grammar panel) shows it, as the
    // setting says, instead of its context menu
    const auto toHandler = [this]() {
        return m_nextIssueHandler &&
               core::SettingsManager::getInstance().get<std::string>(NEXT_ISSUE_KEY, "panel") ==
                   "panel";
    };
    if (!spelling && !grammar) {
        showMessage(tr("The spelling is not checked"), NEXT_MESSAGE_MS);
        if (toHandler()) {
            m_nextIssueHandler(bookEditor, false);
        }
        return false;
    }
    using IssueSearch = editor::BookEditor::IssueSearch;
    const auto showResult = [this, bookEditor, spelling, grammar, toHandler](IssueSearch result,
                                                                               bool waited) {
        if (waited && m_statusBar != nullptr) {
            m_statusBar->clearMessage();  // the grammar was being checked
        }
        // Not when another document came to the front while the grammar was checked
        if (result == IssueSearch::Cancelled || bookEditor == nullptr ||
            bookEditor != currentEditor()) {
            return;
        }
        if (result != IssueSearch::Found) {
            QString none = tr("No misspelled words");
            if (grammar) {
                none = spelling ? tr("No misspelled words or grammar issues")
                                : tr("No grammar issues");
            }
            showMessage(none, NEXT_MESSAGE_MS);
            if (toHandler()) {
                m_nextIssueHandler(bookEditor, false);
            }
            return;
        }
        if (toHandler()) {
            m_nextIssueHandler(bookEditor, true);
            return;
        }
        // What to put in its place, under the word, as the menu key shows it
        bookEditor->setFocus();
        bookEditor->showContextMenuAtCursor();
    };

    const IssueSearch result = bookEditor->goToNextIssue();
    if (result != IssueSearch::Searching) {
        showResult(result, false);
        return result == IssueSearch::Found;
    }
    // The grammar of the paragraphs further on is checked first
    showMessage(tr("Checking the grammar further on..."), 0);
    connect(
        bookEditor, &editor::BookEditor::issueSearchFinished, this,
        [showResult](IssueSearch finished) { showResult(finished, true); },
        Qt::SingleShotConnection);
    return true;
}

void SpellingCoordinator::setNextIssueHandler(NextIssueHandler handler) {
    m_nextIssueHandler = std::move(handler);
}

QString SpellingCoordinator::problem(const editor::BookEditor* editor) const {
    const editor::SpellCheckService* service =
        editor != nullptr ? editor->spellCheckService() : nullptr;
    if (service == nullptr || !service->isEnabled()) {
        return {};
    }
    QString missing;
    if (service == m_service) {
        missing = m_missingLanguage;
    } else if (service == m_otherService) {
        missing = m_otherMissingLanguage;
    }
    if (missing.isEmpty()) {
        return {};
    }
    //: %1 is the name of the language as in a list of languages (in Polish: francuski)
    return tr("No spelling dictionary for %1: the spelling is not checked.")
        .arg(utils::languageName(missing));
}

QString SpellingCoordinator::wantedLanguage() {
    QString language = chosenLanguage();
    if (!language.isEmpty()) {
        return language;
    }

    const auto& projects = core::ProjectManager::getInstance();
    if (const core::ProjectBook* book = projects.isProjectOpen() ? projects.book() : nullptr) {
        QString bookLanguage = book->language.trimmed();
        if (!bookLanguage.isEmpty()) {
            return bookLanguage;
        }
    }
    return programLanguage();
}

QString SpellingCoordinator::defaultLanguage() {
    QString language = chosenLanguage();
    return language.isEmpty() ? programLanguage() : language;
}

QString SpellingCoordinator::userDictionaryFile() {
    const QDir settingsFolder(QString::fromStdU16String(
        core::SettingsManager::getInstance().getSettingsFilePath().parent_path().u16string()));
    return settingsFolder.filePath(QStringLiteral("user_dictionary.txt"));
}

void SpellingCoordinator::updateDictionary() {
    const bool enabled = core::SettingsManager::getInstance().get<bool>(ENABLED_KEY, true);
    m_service->setEnabled(enabled);
    m_otherService->setEnabled(enabled);
    if (enabled) {
        const QString language = wantedLanguage();
        const QString dictionary = editor::SpellCheckService::dictionaryFor(language);
        useDictionary(*m_service, language, dictionary, m_missingLanguage);

        // The documents outside the book: a dictionary of their own when the book's is not
        // the one of their language
        const QString otherLanguage = defaultLanguage();
        const QString otherDictionary = editor::SpellCheckService::dictionaryFor(otherLanguage);
        const bool ownDictionary = otherDictionary != dictionary;
        m_otherLanguage = ownDictionary ? otherLanguage : QString();
        m_otherDictionary = ownDictionary ? otherDictionary : QString();
    } else {
        // The dictionaries are loaded when the checking is turned on; the writer is told
        // again of a missing one then
        m_missingLanguage.clear();
        m_otherMissingLanguage.clear();
    }
    updateEditors();
    emit problemChanged();
}

void SpellingCoordinator::updateEditors() {
    m_editorsTimer->stop();
    bool otherDocuments = false;
    if (m_centralTabs != nullptr) {
        for (QWidget* page : DocumentWindows::of(*m_centralTabs).pages()) {
            auto* panel = qobject_cast<EditorPanel*>(page);
            if (panel == nullptr || panel->getBookEditor() == nullptr) {
                continue;
            }
            const bool chapter = isChapter(*panel);
            otherDocuments = otherDocuments || !chapter;
            panel->getBookEditor()->setSpellCheckService(
                chapter || m_otherLanguage.isEmpty() ? m_service : m_otherService);
        }
    }

    if (m_otherLanguage.isEmpty()) {
        // The book's dictionary checks every document: the other one does not take memory
        m_otherService->releaseDictionary();
        m_otherMissingLanguage.clear();
    } else if (otherDocuments && m_otherService->isEnabled()) {
        // Loaded for the first document that needs it, and kept for the next ones
        useDictionary(*m_otherService, m_otherLanguage, m_otherDictionary,
                      m_otherMissingLanguage);
    }
    emit problemChanged();  // the documents may have other dictionaries now
}

void SpellingCoordinator::useDictionary(editor::SpellCheckService& service,
                                        const QString& language, const QString& dictionary,
                                        QString& missingLanguage) {
    if (!dictionary.isEmpty()) {
        missingLanguage.clear();
        service.loadDictionaryInBackground(dictionary);
        return;
    }

    service.unloadDictionary();
    if (language != missingLanguage) {
        missingLanguage = language;
        core::Logger::getInstance().info("SpellingCoordinator: No dictionary for the language '{}'",
                                         language.toStdString());
        //: %1 is the name of the language as in a list of languages (in Polish: francuski)
        showMessage(tr("No spelling dictionary for %1: the spelling is not checked")
                        .arg(utils::languageName(language)),
                    DICTIONARY_MESSAGE_MS);
    }
}

editor::BookEditor* SpellingCoordinator::currentEditor() const {
    auto* panel = m_centralTabs != nullptr
                      ? qobject_cast<EditorPanel*>(DocumentWindows::of(*m_centralTabs).frontPage())
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
