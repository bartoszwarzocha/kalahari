/// @file spelling_panel_coordinator.cpp
/// @brief The Spelling and Grammar panel: the issues of the text in front, what the writer
///        does with them, and when the panel shows

#include "kalahari/gui/spelling_panel_coordinator.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/grammar_check_service.h"
#include "kalahari/editor/spell_check_service.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/grammar_coordinator.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/spelling_panel.h"
#include "kalahari/gui/spelling_coordinator.h"

#include <QDockWidget>
#include <QHash>
#include <QKeySequence>
#include <QMetaObject>
#include <QScopedValueRollback>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextDocument>
#include <QTimer>

#include <algorithm>
#include <string>

namespace kalahari::gui {

namespace {

/// @brief How long after the last change of the issues the panel lists them again (ms):
///        once for the keys typed together
constexpr int REFRESH_DELAY_MS = 150;

/// @brief Paragraphs a kind checked through may wait for their check (the one typed in, the
///        one split by Enter) while the row under the counts still says the text is checked
constexpr int DONE_SLACK = 2;

/// @brief The settings of the panel
constexpr const char* FIX_BUTTONS_KEY = "spellingPanel.fixButtons";
constexpr const char* NEXT_ISSUE_KEY = "spellingPanel.nextIssue";
constexpr const char* GRAMMAR_SCOPE_KEY = "spellingPanel.grammarScope";

/// @brief The setting that turns the spelling on and off (SpellingCoordinator)
constexpr const char* SPELLING_ENABLED_KEY = "editor.spellCheck.enabled";

/// @brief The commands that turn the spelling and the grammar on and off, and the one going
///        to the next issue
constexpr const char* SPELLING_COMMAND = "tools.spellcheck";
constexpr const char* GRAMMAR_COMMAND = "tools.grammar";
constexpr const char* NEXT_ISSUE_COMMAND = "tools.nextMisspelling";

/// @brief Whether a setting of the panel or of the checking
bool isWatchedSetting(const std::string& key) {
    return key.rfind("spellingPanel.", 0) == 0 || key.rfind("editor.spellCheck.", 0) == 0 ||
           key.rfind("editor.grammarCheck.", 0) == 0;
}

/// @brief The index of a kind in the arrays of the kinds
std::size_t kindIndex(editor::TextIssueKind kind) {
    return kind == editor::TextIssueKind::Spelling ? 0 : 1;
}

}  // namespace

SpellingPanelCoordinator::SpellingPanelCoordinator(SpellingPanel* panel, QDockWidget* dock,
                                                   QTabWidget* centralTabs,
                                                   SpellingCoordinator* spelling,
                                                   GrammarCoordinator* grammar, QObject* parent)
    : QObject(parent)
    , m_panel(panel)
    , m_dock(dock)
    , m_centralTabs(centralTabs)
    , m_spelling(spelling)
    , m_grammar(grammar)
{
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(REFRESH_DELAY_MS);
    connect(m_refreshTimer, &QTimer::timeout, this, &SpellingPanelCoordinator::refresh);

    connect(&DocumentWindows::of(*m_centralTabs), &DocumentWindows::frontChanged, this,
            &SpellingPanelCoordinator::onCurrentTabChanged);

    // What the writer does in the panel
    connect(m_panel, &SpellingPanel::issueSelected, this,
            &SpellingPanelCoordinator::selectInEditor);
    connect(m_panel, &SpellingPanel::suggestionChosen, this,
            &SpellingPanelCoordinator::onSuggestionChosen);
    connect(m_panel, &SpellingPanel::ignoreWordRequested, this,
            [this](const SpellingPanelIssue& issue) { onIgnoreWord(issue, false); });
    connect(m_panel, &SpellingPanel::addToDictionaryRequested, this,
            [this](const SpellingPanelIssue& issue) { onIgnoreWord(issue, true); });
    connect(m_panel, &SpellingPanel::ignoreRuleRequested, this,
            &SpellingPanelCoordinator::onIgnoreRule);
    connect(m_panel, &SpellingPanel::menuRequested, this,
            &SpellingPanelCoordinator::onMenuRequested);
    connect(m_panel, &SpellingPanel::editorFocusRequested, this,
            &SpellingPanelCoordinator::onEditorFocusRequested);
    connect(m_panel, &SpellingPanel::turnOnCheckingRequested, this,
            &SpellingPanelCoordinator::turnOnChecking);

    // The suggestions for a misspelled word: from the dictionary of the text in front
    m_panel->setSuggestionProvider([this](const SpellingPanelIssue& issue) {
        const editor::BookEditor* editor = m_editor;
        const editor::SpellCheckService* service =
            editor != nullptr ? editor->spellCheckService() : nullptr;
        return service != nullptr ? service->suggestions(issue.issue.text) : QStringList();
    });

    // Why the text is not checked, and whether the grammar is checked now
    if (m_spelling != nullptr) {
        connect(m_spelling, &SpellingCoordinator::problemChanged, this,
                &SpellingPanelCoordinator::scheduleRefresh);
    }
    if (m_grammar != nullptr) {
        connect(m_grammar, &GrammarCoordinator::problemChanged, this,
                &SpellingPanelCoordinator::scheduleRefresh);
        connect(m_grammar->service(), &editor::GrammarCheckService::checkingChanged, this,
                &SpellingPanelCoordinator::scheduleRefresh);
    }

    // The list is made while the panel can be seen; one made old meanwhile is made again when
    // it comes into view
    connect(m_dock, &QDockWidget::visibilityChanged, this,
            &SpellingPanelCoordinator::onDockVisibilityChanged);

    // The keys of Next Spelling or Grammar Issue, in the row at the bottom
    connect(&CommandRegistry::getInstance(), &CommandRegistry::shortcutsChanged, this,
            &SpellingPanelCoordinator::updateNextIssueKeys);

    // The settings of the panel and of the checking, wherever they change: once for all those
    // written together (Apply in the Settings dialog writes several)
    m_settingsListener =
        core::SettingsManager::getInstance().subscribe([this](const std::string& key) {
            if (isWatchedSetting(key) && !m_settingsPending.exchange(true)) {
                QMetaObject::invokeMethod(this, "onSettingsChanged", Qt::QueuedConnection);
            }
        });

    if (m_spelling != nullptr) {
        m_spelling->setNextIssueHandler([this](editor::BookEditor* editor, bool found) {
            showNextIssue(editor, found);
        });
    }

    applySettings();
    onCurrentTabChanged();
}

SpellingPanelCoordinator::~SpellingPanelCoordinator() {
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
    if (m_spelling != nullptr) {
        m_spelling->setNextIssueHandler(nullptr);
    }
}

// =============================================================================
// The text in front
// =============================================================================

editor::BookEditor* SpellingPanelCoordinator::currentEditor() const {
    auto* panel = qobject_cast<EditorPanel*>(DocumentWindows::of(*m_centralTabs).frontPage());
    return panel != nullptr ? panel->getBookEditor() : nullptr;
}

void SpellingPanelCoordinator::onCurrentTabChanged() {
    editor::BookEditor* editor = currentEditor();
    if (editor != m_editor) {
        if (m_editor != nullptr) {
            disconnect(m_editor, nullptr, this, nullptr);
            m_editor->setWholeTextGrammar(false);  // the panel lists another text
        }
        m_editor = editor;
        m_done = {};
        m_wordsChanged = false;
        if (editor != nullptr) {
            connect(editor, &QObject::destroyed, this, [this]() { m_editor = nullptr; });
            connect(editor, &editor::BookEditor::issuesChanged, this,
                    &SpellingPanelCoordinator::scheduleRefresh);
            connect(editor, &editor::BookEditor::cursorPositionChanged, this,
                    &SpellingPanelCoordinator::onCursorMoved);
        }
        updateWholeTextGrammar();
    }
    if (m_shown) {
        refresh();
    } else {
        m_stale = true;
    }
}

void SpellingPanelCoordinator::onCursorMoved(const editor::CursorPosition& position) {
    // A click on an issue in the text selects its card; not the issue the panel selects
    if (m_selecting || !m_shown) {
        return;
    }
    for (const SpellingPanelIssue& issue : m_panel->shownIssues()) {
        if (issue.issue.paragraph == position.paragraph && position.offset >= issue.issue.start &&
            position.offset <= issue.issue.start + issue.issue.length) {
            if (issue.key != m_panel->selectedKey()) {
                m_panel->selectIssue(issue.key);
            }
            return;
        }
    }
}

void SpellingPanelCoordinator::scheduleRefresh() {
    if (m_shown) {
        m_refreshTimer->start();
    } else {
        m_stale = true;
    }
}

void SpellingPanelCoordinator::onWordsChanged() {
    m_wordsChanged = true;
    m_panel->forgetSuggestions();
    scheduleRefresh();
}

void SpellingPanelCoordinator::refresh() {
    m_refreshTimer->stop();
    m_stale = false;

    // The names of what the panel lists: the kind of the text in front ("this story"), the
    // word "file" for a file outside the book, and the main texts the writer opens in the book
    auto& projects = core::ProjectManager::getInstance();
    const bool bookOpen = projects.isProjectOpen();
    editor::BookEditor* editor = m_editor;
    std::optional<core::KindWords> inFront;
    if (editor != nullptr) {
        auto* panel = qobject_cast<EditorPanel*>(DocumentWindows::of(*m_centralTabs).frontPage());
        const QString elementId =
            panel != nullptr ? panel->property("elementId").toString() : QString();
        const core::ProjectElement* element =
            bookOpen && !elementId.isEmpty() ? projects.findElement(elementId) : nullptr;
        inFront = element != nullptr ? wordsOf(*element) : fileWords();
    }
    std::optional<core::KindWords> bookTexts;
    if (const core::KindRef mainKind = projects.mainTextKind()) {
        bookTexts = wordsOf(mainKind);
    }
    m_panel->setKindNames(inFront, bookTexts, bookOpen);

    if (editor == nullptr) {
        m_panel->setContent(SpellingPanel::Content::NoText);
        m_panel->setIssues({});
        m_panel->setStatus(QString(), false);
        return;
    }

    auto& settings = core::SettingsManager::getInstance();
    const bool spellingOn = settings.get<bool>(SPELLING_ENABLED_KEY, true);
    const bool grammarOn =
        m_grammar != nullptr && settings.get<bool>(GrammarCoordinator::ENABLED_KEY, false);
    if (!spellingOn && !grammarOn) {
        m_panel->setContent(SpellingPanel::Content::NotChecked);
        m_panel->setIssues({});
        m_panel->setStatus(QString(), false);
        return;
    }
    m_panel->setContent(SpellingPanel::Content::Issues);
    m_panel->setReadOnly(editor->isReadOnly());

    // The writer's words changed: the text is checked again, and the waves of the words right
    // now are left out meanwhile
    editor::SpellCheckService* spell = editor->spellCheckService();
    if (spell != nullptr) {
        connect(spell, &editor::SpellCheckService::wordsChanged, this,
                &SpellingPanelCoordinator::onWordsChanged, Qt::UniqueConnection);
        connect(spell, &editor::SpellCheckService::dictionaryLoaded, m_panel,
                &SpellingPanel::forgetSuggestions, Qt::UniqueConnection);
    }

    // A kind is listed while it is checked: on, and nothing in the way
    const QString spellingProblem =
        spellingOn && m_spelling != nullptr ? m_spelling->problem(editor) : QString();
    const QString grammarProblem = grammarOn ? m_grammar->problem() : QString();
    const editor::GrammarCheckService* grammarService = editor->grammarCheckService();
    const bool spellingListed =
        spellingOn && spellingProblem.isEmpty() && spell != nullptr && spell->isActive();
    const bool grammarListed = grammarOn && grammarProblem.isEmpty() &&
                               grammarService != nullptr && grammarService->isEnabled() &&
                               grammarService->isConfigured();
    m_panel->setKindChecked(editor::TextIssueKind::Spelling, spellingListed);
    m_panel->setKindChecked(editor::TextIssueKind::Grammar, grammarListed);

    // How far the text is checked
    const editor::BookEditor::CheckProgress progress = editor->checkProgress();
    if (progress.spellingChecked >= progress.paragraphs) {
        m_wordsChanged = false;
    }
    m_panel->setIssues(issuesOf(*editor, spellingListed, grammarListed));

    const bool wholeGrammar =
        settings.get<std::string>(GRAMMAR_SCOPE_KEY, "whole") != std::string("nearby");
    const bool spellingDone =
        !spellingListed || isDone(editor::TextIssueKind::Spelling, progress.spellingChecked,
                                  progress.paragraphs);
    const bool grammarDone =
        !grammarListed || (wholeGrammar && isDone(editor::TextIssueKind::Grammar,
                                                  progress.grammarChecked, progress.paragraphs));
    m_panel->setComplete(spellingDone && grammarDone);

    // The row under the counts: what is in the way, else how far the text is checked
    QStringList problems;
    for (const QString& problem : {spellingProblem, grammarProblem}) {
        if (!problem.isEmpty()) {
            problems.append(problem);
        }
    }
    if (!problems.isEmpty()) {
        m_panel->setStatus(problems.join(QLatin1Char(' ')), true);
        return;
    }
    QStringList status;
    const core::KindWords kind = inFront.value_or(fileWords());
    const QHash<QString, core::KindWords> nouns = {{QStringLiteral("kind"), kind}};
    if (spellingOn && !spellingListed) {
        status.append(tr("The dictionary is being loaded..."));
    } else if (!spellingDone) {
        //: %1 is the number of the paragraph, %2 of all the paragraphs with text
        status.append(tr("Checking the spelling: paragraph %1 of %2...")
                          .arg(std::min(progress.spellingChecked + 1, progress.paragraphs))
                          .arg(progress.paragraphs));
    } else if (grammarListed && wholeGrammar && !grammarDone) {
        //: %1 is the number of the paragraph, %2 of all the paragraphs with text
        status.append(tr("Checking the grammar: paragraph %1 of %2...")
                          .arg(std::min(progress.grammarChecked + 1, progress.paragraphs))
                          .arg(progress.paragraphs));
    } else if (grammarListed && !wholeGrammar) {
        status.append(
            spellingListed
                //: In Polish: Sprawdzono pisownię w {kind:m=całym|f=całej|n=całym|p=całych}
                //: {kind:locative}, a gramatykę tylko wokół widocznego fragmentu.
                ? core::fillWords(tr("The spelling of the whole {kind} is checked, the grammar "
                                     "only around the part in view."),
                                  nouns)
                : tr("The grammar is checked only around the part in view."));
    } else {
        //: In Polish: Sprawdzono {kind:m=cały|f=całą|n=całe|p=całe} {kind:accusative}.
        status.append(core::fillWords(tr("The whole {kind} is checked."), nouns));
    }
    if (!spellingOn) {
        status.append(tr("The spelling is not checked."));
    }
    if (!grammarOn) {
        status.append(tr("The grammar is not checked."));
    }
    m_panel->setStatus(status.join(QLatin1Char(' ')), false);
}

std::vector<SpellingPanelIssue> SpellingPanelCoordinator::issuesOf(editor::BookEditor& editor,
                                                                   bool spelling,
                                                                   bool grammar) const {
    std::vector<SpellingPanelIssue> issues;
    const QTextDocument* document = editor.textDocument();
    if (document == nullptr || (!spelling && !grammar)) {
        return issues;
    }
    const editor::SpellCheckService* spell = editor.spellCheckService();

    // The paragraphs with text before each issue's paragraph ("paragraph 3"), counted once as
    // the issues go through the text in its order
    QTextBlock block = document->begin();
    int number = block.isValid() && !block.text().trimmed().isEmpty() ? 1 : 0;

    // An issue's key: what it is, and which of the same it is in the text, so it stays the
    // same as the text around it changes
    QHash<QString, int> occurrences;
    for (const editor::TextIssue& issue : editor.textIssues()) {
        const bool isSpelling = issue.kind == editor::TextIssueKind::Spelling;
        if (isSpelling ? !spelling : !grammar) {
            continue;
        }
        // A word the writer made right (ignored, added to the dictionary) until the text is
        // checked again
        if (isSpelling && m_wordsChanged && spell != nullptr && spell->isCorrect(issue.text)) {
            continue;
        }
        while (block.isValid() && block.blockNumber() < issue.paragraph) {
            block = block.next();
            if (block.isValid() && !block.text().trimmed().isEmpty()) {
                ++number;
            }
        }
        if (!block.isValid()) {
            break;
        }

        SpellingPanelIssue listed;
        listed.issue = issue;
        listed.position = block.position() + issue.start;
        listed.number = std::max(1, number);
        listed.paragraphText = block.text();
        const QString what = (isSpelling ? QStringLiteral("S:") : QStringLiteral("G:")) +
                             issue.ruleId + QLatin1Char(':') + issue.text;
        listed.key = what + QLatin1Char('#') + QString::number(occurrences[what]++);
        issues.push_back(std::move(listed));
    }
    return issues;
}

bool SpellingPanelCoordinator::isDone(editor::TextIssueKind kind, int checked, int paragraphs) {
    bool& done = m_done.at(kindIndex(kind));
    const int waiting = paragraphs - checked;
    if (waiting <= 0) {
        done = true;
    } else if (waiting > DONE_SLACK) {
        done = false;
    }
    return done;
}

int SpellingPanelCoordinator::cursorPlace(const editor::BookEditor& editor) const {
    const QTextDocument* document = editor.textDocument();
    const editor::CursorPosition cursor = editor.cursorPosition();
    const QTextBlock block =
        document != nullptr ? document->findBlockByNumber(cursor.paragraph) : QTextBlock();
    return block.isValid() ? block.position() + cursor.offset : 0;
}

// =============================================================================
// What the writer does in the panel
// =============================================================================

void SpellingPanelCoordinator::selectInEditor(const SpellingPanelIssue& issue) {
    editor::BookEditor* editor = m_editor;
    if (editor == nullptr) {
        return;
    }
    bool selected = false;
    {
        const QScopedValueRollback<bool> selecting(m_selecting, true);
        selected = editor->selectIssue(issue.issue);
    }
    if (!selected) {
        refresh();  // the text there changed: the list is old
    }
}

void SpellingPanelCoordinator::selectFrom(int position) {
    const std::optional<SpellingPanelIssue> next = m_panel->issueFrom(position);
    m_panel->selectIssue(next ? next->key : QString());
    if (next) {
        selectInEditor(*next);
    }
}

void SpellingPanelCoordinator::onSuggestionChosen(const SpellingPanelIssue& issue,
                                                  const QString& suggestion) {
    editor::BookEditor* editor = m_editor;
    if (editor == nullptr) {
        return;
    }
    const bool replaced = editor->replaceIssue(issue.issue, suggestion);
    refresh();
    if (replaced) {
        selectFrom(cursorPlace(*editor));  // the issue after it, as the cursor is after it
    }
}

void SpellingPanelCoordinator::onIgnoreWord(const SpellingPanelIssue& issue,
                                            bool addToDictionary) {
    editor::BookEditor* editor = m_editor;
    editor::SpellCheckService* service = editor != nullptr ? editor->spellCheckService() : nullptr;
    if (service == nullptr) {
        return;
    }
    // In every document, as from the menu of the wave: the text is checked again
    m_wordsChanged = true;
    if (addToDictionary) {
        service->addToUserDictionary(issue.issue.text);
    } else {
        service->ignoreWord(issue.issue.text);
    }
    refresh();
    selectFrom(issue.position);
}

void SpellingPanelCoordinator::onIgnoreRule(const SpellingPanelIssue& issue) {
    editor::BookEditor* editor = m_editor;
    editor::GrammarCheckService* service =
        editor != nullptr ? editor->grammarCheckService() : nullptr;
    if (service == nullptr) {
        return;
    }
    // Its issues leave the text at once, in every document
    service->ignoreRule(issue.issue.ruleId, issue.issue.ruleDescription);
    refresh();
    selectFrom(issue.position);
}

void SpellingPanelCoordinator::onMenuRequested(const SpellingPanelIssue& issue,
                                               const QPoint& globalPos) {
    editor::BookEditor* editor = m_editor;
    if (editor == nullptr) {
        return;
    }
    selectInEditor(issue);  // the text shows the issue the menu is for
    if (!editor->showIssueMenu(issue.issue, globalPos)) {
        refresh();
        return;
    }
    if (m_editor != editor) {
        return;  // another text came to the front meanwhile
    }

    // A suggestion of the menu, or a word or rule ignored: the issue after it is selected
    refresh();
    const auto shown = m_panel->shownIssues();
    const auto same = std::find_if(shown.cbegin(), shown.cend(),
                                   [&issue](const SpellingPanelIssue& listed) {
                                       return listed.position == issue.position &&
                                              listed.issue.kind == issue.issue.kind &&
                                              listed.issue.text == issue.issue.text;
                                   });
    if (same != shown.cend()) {
        m_panel->selectIssue(same->key);  // the menu closed without a choice
    } else {
        selectFrom(issue.position);
    }
}

void SpellingPanelCoordinator::onEditorFocusRequested() {
    // Back to the text, with the issue selected there: it can be put right by hand; the panel
    // stays open
    if (editor::BookEditor* editor = m_editor) {
        editor->setFocus(Qt::OtherFocusReason);
    }
}

void SpellingPanelCoordinator::turnOnChecking() {
    // The commands of the Tools menu: a grammar not prepared yet offers to prepare it
    auto& settings = core::SettingsManager::getInstance();
    auto& registry = CommandRegistry::getInstance();
    if (!settings.get<bool>(SPELLING_ENABLED_KEY, true)) {
        registry.executeCommand(SPELLING_COMMAND);
    }
    if (m_grammar != nullptr && !settings.get<bool>(GrammarCoordinator::ENABLED_KEY, false)) {
        registry.executeCommand(GRAMMAR_COMMAND);
    }
}

// =============================================================================
// When the panel shows
// =============================================================================

void SpellingPanelCoordinator::showNextIssue(editor::BookEditor* editor, bool found) {
    if (editor == nullptr || editor != currentEditor()) {
        return;
    }
    showPanel();
    refresh();  // the list as the text is now

    // The issue the editor selected; shown, also when its kind was hidden in the panel
    if (found) {
        const editor::SelectionRange selection = editor->selection().normalized();
        const QTextDocument* document = editor->textDocument();
        const QTextBlock block = document != nullptr
                                     ? document->findBlockByNumber(selection.start.paragraph)
                                     : QTextBlock();
        const int start = block.isValid() ? block.position() + selection.start.offset : -1;
        const int length = selection.end.offset - selection.start.offset;
        const auto matches = [start, length](const SpellingPanelIssue& listed) {
            return listed.position == start && listed.issue.length == length;
        };
        const std::vector<SpellingPanelIssue>& all = m_panel->issues();
        const auto issue = std::find_if(all.cbegin(), all.cend(), matches);
        if (issue != all.cend()) {
            m_panel->setKindShown(issue->issue.kind, true);
            m_panel->selectIssue(issue->key);
        }
    }
    m_panel->focusList();
}

void SpellingPanelCoordinator::showPanel() {
    // Open until the writer closes it
    m_dock->show();
    m_dock->raise();  // a panel shown again in a tab group would lie under the current tab
}

void SpellingPanelCoordinator::onDockVisibilityChanged(bool visible) {
    m_shown = visible;
    updateWholeTextGrammar();
    if (visible && m_stale) {
        refresh();
    }
}

void SpellingPanelCoordinator::updateWholeTextGrammar() {
    if (editor::BookEditor* editor = m_editor) {
        const bool whole = m_shown && core::SettingsManager::getInstance().get<std::string>(
                                          GRAMMAR_SCOPE_KEY, "whole") != std::string("nearby");
        if (editor->isWholeTextGrammar() != whole) {
            editor->setWholeTextGrammar(whole);
        }
    }
}

void SpellingPanelCoordinator::updateNextIssueKeys() {
    const Command* command = CommandRegistry::getInstance().getCommand(NEXT_ISSUE_COMMAND);
    const QString keys = command != nullptr
                             ? command->shortcut.toQKeySequence().toString(QKeySequence::NativeText)
                             : QString();
    const bool toPanel = core::SettingsManager::getInstance().get<std::string>(
                             NEXT_ISSUE_KEY, "panel") != std::string("menu");
    m_panel->setNextIssueKeys(keys, toPanel);
}

void SpellingPanelCoordinator::applySettings() {
    auto& settings = core::SettingsManager::getInstance();
    m_panel->setButtonsOnEveryCard(settings.get<std::string>(FIX_BUTTONS_KEY, "selected") ==
                                   std::string("every"));
    updateNextIssueKeys();
    updateWholeTextGrammar();
}

void SpellingPanelCoordinator::onSettingsChanged() {
    m_settingsPending = false;
    applySettings();
    scheduleRefresh();
}

}  // namespace kalahari::gui
