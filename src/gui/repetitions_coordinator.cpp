/// @file repetitions_coordinator.cpp
/// @brief The repetitions of words: the panel Repetitions, the marks in the text and Next
///        Repetition

#include "kalahari/gui/repetitions_coordinator.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/chapter_document.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/render_context.h"
#include "kalahari/editor/spell_check_service.h"
#include "kalahari/editor/text_highlight.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/panels/annotation_colors.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/repetitions_panel.h"
#include "kalahari/gui/spelling_coordinator.h"
#include "kalahari/gui/utils/setting_toggle.h"

#include <QDockWidget>
#include <QFileInfo>
#include <QLocale>
#include <QMetaObject>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextDocument>
#include <QTimer>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>

namespace kalahari::gui {

namespace {

/// @brief The commands of the repetitions
constexpr const char* PANEL_COMMAND = "tools.repetitions";
constexpr const char* NEXT_COMMAND = "tools.nextRepetition";
constexpr const char* MARKS_COMMAND = "view.repetitionMarks";

/// @brief The settings of the repetitions start with it
constexpr const char* KEY_PREFIX = "editor.repetitions.";

/// @brief How long the writing pauses before the text in front is counted again (ms)
constexpr int COUNT_DELAY_MS = 700;

/// @brief The most frequent words the panel lists
constexpr int MAX_FREQUENT = 100;

/// @brief How long a message of the commands stays in the status bar (ms)
constexpr int MESSAGE_MS = 4000;

/// @brief Whether a setting changes what is counted or how it is shown: those of the
///        repetitions, and the colors of the themes
bool isWatchedSetting(const std::string& key) {
    return key.rfind(KEY_PREFIX, 0) == 0 || key.rfind("themes.", 0) == 0 ||
           key == core::ThemeManager::OPTIMIZE_COLORS_SETTING;
}

}  // namespace

RepetitionsCoordinator::RepetitionsCoordinator(RepetitionsPanel* panel, QDockWidget* dock,
                                               QTabWidget* centralTabs,
                                               SpellingCoordinator* spelling,
                                               ChapterOpener openChapter, QStatusBar* statusBar,
                                               QObject* parent)
    : QObject(parent)
    , m_panel(panel)
    , m_dock(dock)
    , m_centralTabs(centralTabs)
    , m_spelling(spelling)
    , m_openChapter(std::move(openChapter))
    , m_statusBar(statusBar)
{
    // The text in front is counted again a moment after the writing stops
    m_countTimer = new QTimer(this);
    m_countTimer->setSingleShot(true);
    m_countTimer->setInterval(COUNT_DELAY_MS);
    connect(m_countTimer, &QTimer::timeout, this, &RepetitionsCoordinator::refresh);

    connect(m_centralTabs, &QTabWidget::currentChanged, this,
            &RepetitionsCoordinator::onCurrentTabChanged);
    connect(m_dock, &QDockWidget::visibilityChanged, this,
            &RepetitionsCoordinator::onDockVisibilityChanged);

    // What the writer does in the panel
    connect(m_panel, &RepetitionsPanel::scopeChanged, this, [this]() { refresh(); });
    connect(m_panel, &RepetitionsPanel::recountRequested, this, [this]() { refresh(); });
    connect(m_panel, &RepetitionsPanel::pageChanged, this, [this]() { updateMarks(); });
    connect(m_panel, &RepetitionsPanel::cardSelected, this,
            &RepetitionsCoordinator::onCardSelected);
    connect(m_panel, &RepetitionsPanel::cardActivated, this,
            &RepetitionsCoordinator::onCardActivated);
    connect(m_panel, &RepetitionsPanel::skipWordRequested, this,
            &RepetitionsCoordinator::onSkipWord);
    connect(m_panel, &RepetitionsPanel::wordSelected, this, [this]() { updateMarks(); });
    connect(m_panel, &RepetitionsPanel::wordActivated, this,
            &RepetitionsCoordinator::onWordActivated);
    connect(m_panel, &RepetitionsPanel::editorFocusRequested, this, [this]() {
        if (editor::BookEditor* editor = currentEditor()) {
            editor->setFocus(Qt::OtherFocusReason);
        }
    });

    // The whole book can be counted while a book is open
    auto& projects = core::ProjectManager::getInstance();
    const auto updateBook = [this]() {
        const bool open = core::ProjectManager::getInstance().project() != nullptr;
        m_panel->setBookAvailable(open);
        if (!open && m_panel->scope() == RepetitionsPanel::Scope::Book) {
            m_panel->setScope(RepetitionsPanel::Scope::Chapter);
        }
        m_stale = true;
        if (m_shown) {
            refresh();
        }
    };
    connect(&projects, &core::ProjectManager::projectOpened, this, updateBook);
    connect(&projects, &core::ProjectManager::projectClosed, this, updateBook);
    m_panel->setBookAvailable(projects.project() != nullptr);

    // The settings, wherever they change: once for all those written together
    m_settingsListener =
        core::SettingsManager::getInstance().subscribe([this](const std::string& key) {
            if (isWatchedSetting(key) && !m_settingsPending.exchange(true)) {
                QMetaObject::invokeMethod(this, "onSettingsChanged", Qt::QueuedConnection);
            }
        });
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged, this,
            [this]() { onSettingsChanged(); });
    onSettingsChanged();
}

RepetitionsCoordinator::~RepetitionsCoordinator() {
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);

    // The commands outlive the coordinator
    auto& registry = CommandRegistry::getInstance();
    for (const char* id : {PANEL_COMMAND, NEXT_COMMAND, MARKS_COMMAND}) {
        if (Command* command = registry.getCommand(id)) {
            command->execute = nullptr;
            command->isEnabled = nullptr;
            command->disabledReason = nullptr;
        }
    }
}

void RepetitionsCoordinator::connectCommands() {
    auto& registry = CommandRegistry::getInstance();
    if (Command* command = registry.getCommand(PANEL_COMMAND)) {
        command->execute = [this]() { showPanel(); };
        registry.updateActionState(PANEL_COMMAND);
    }
    if (Command* command = registry.getCommand(NEXT_COMMAND)) {
        command->execute = [this]() { goToNextRepetition(); };
        command->isEnabled = [this]() { return currentEditor() != nullptr; };
        command->disabledReason = []() {
            //: Why Next Repetition is greyed out. In Polish: Na wierzchu nie ma tekstu
            return tr("No text is in front");
        };
        registry.updateActionState(NEXT_COMMAND);
    }
    if (Command* command = registry.getCommand(MARKS_COMMAND)) {
        command->execute = []() {
            auto& settings = core::SettingsManager::getInstance();
            settings.set<bool>(HIGHLIGHT_KEY, !settings.get<bool>(HIGHLIGHT_KEY));
        };
        registry.updateActionState(MARKS_COMMAND);
    }
    // Checked while the repetitions are marked, also after the Settings dialog changes it
    utils::followSetting(registry.getAction(std::string(MARKS_COMMAND)), HIGHLIGHT_KEY);
    connect(m_centralTabs, &QTabWidget::currentChanged, this,
            []() { CommandRegistry::getInstance().updateActionState(NEXT_COMMAND); });
}

// =============================================================================
// The commands
// =============================================================================

void RepetitionsCoordinator::showPanel() {
    // Open until the writer closes it; a panel in a group of tabs comes on top
    m_dock->show();
    m_dock->raise();
    if (m_stale) {
        refresh();
    }
    m_panel->focusList();
}

bool RepetitionsCoordinator::goToNextRepetition() {
    EditorPanel* panel = currentEditorPanel();
    if (panel == nullptr) {
        return false;
    }
    editor::BookEditor* editor = panel->getBookEditor();

    // The words of the text as it is now: the panel's when they are of it
    int text = textOf(editor);
    if (m_panel->scope() == RepetitionsPanel::Scope::Chapter &&
        (text < 0 || m_stale || m_countTimer->isActive())) {
        countChapter();
        text = textOf(editor);
    }
    editor::RepetitionAnalysis local;
    const editor::RepetitionAnalysis* analysis = &m_analysis;
    if (text < 0 || m_stale || m_changedTexts.contains(text)) {
        // The book counted without this text, before it changed or with other settings: this
        // text alone
        local = analyze({Text{elementIdOf(panel), titleOf(panel), paragraphsOf(*editor), editor}},
                        editor->spellCheckService());
        analysis = &local;
        text = 0;
    }

    // The first cluster of the text whose first word is after the cursor (after the word
    // selected, the one found before); else its first one
    const editor::CursorPosition cursor = after(*editor);
    int found = -1;
    int first = -1;
    for (int i = 0; i < analysis->clusters.size(); ++i) {
        const editor::TextWord& word = analysis->words.at(analysis->clusters.at(i).words.first());
        if (word.text != text) {
            continue;
        }
        if (first < 0) {
            first = i;
        }
        if (word.paragraph > cursor.paragraph ||
            (word.paragraph == cursor.paragraph && word.start >= cursor.offset)) {
            found = i;
            break;
        }
    }
    if (found < 0) {
        found = first;
    }
    if (found < 0) {
        //: Next Repetition found none. In Polish: Żadne słowo nie wraca tu zbyt blisko
        showMessage(tr("No word comes back too close here"));
        return false;
    }
    const editor::RepetitionCluster& cluster = analysis->clusters.at(found);
    selectWord(*editor, analysis->words.at(cluster.words.first()));
    //: Next Repetition: the word it found, how many times and in how many words. In Polish:
    //: Powtórzenie: „%1”, %n razy w %2 słowach
    showMessage(tr("Repetition: “%1”, %n time(s) in %2 words", "",
                   static_cast<int>(cluster.words.size()))
                    .arg(cluster.key, QLocale().toString(cluster.span)));
    if (analysis == &m_analysis) {
        m_panel->setPage(RepetitionsPanel::Page::Close);
        m_panel->setCurrentCard(found);
        updateMarks();
    }
    return true;
}

void RepetitionsCoordinator::refresh() {
    m_countTimer->stop();
    if (m_panel->scope() == RepetitionsPanel::Scope::Book) {
        countBook();
    } else {
        countChapter();
    }
}

editor::RepetitionOptions RepetitionsCoordinator::options(const QString& language) {
    auto& settings = core::SettingsManager::getInstance();
    editor::RepetitionOptions options;
    options.distance = std::max(1, settings.get<int>(DISTANCE_KEY));
    options.minLength = std::max(1, settings.get<int>(MIN_LENGTH_KEY));
    options.mergeForms = settings.get<bool>(MERGE_FORMS_KEY);
    options.skipCommon = settings.get<bool>(SKIP_COMMON_KEY);
    for (const std::string& word : settings.get<std::vector<std::string>>(SKIPPED_KEY)) {
        options.skipped.insert(QString::fromStdString(word).toLower());
    }
    options.language = language;
    return options;
}

void RepetitionsCoordinator::skipWord(const QString& word) {
    auto& settings = core::SettingsManager::getInstance();
    std::vector<std::string> skipped = settings.get<std::vector<std::string>>(SKIPPED_KEY);
    const std::string lower = word.toLower().toStdString();
    if (std::find(skipped.begin(), skipped.end(), lower) == skipped.end()) {
        skipped.push_back(lower);
        settings.set<std::vector<std::string>>(SKIPPED_KEY, skipped);
    }
}

void RepetitionsCoordinator::onSettingsChanged() {
    m_settingsPending = false;

    // The color of the repeated words on the cards: the theme's, as in the text
    const auto& themes = core::ThemeManager::getInstance();
    m_panel->setMarkColor(shownColor(
        themes.editorColor(editor::REPETITION_COLOR_KEY, editor::RenderColors().repetition),
        m_panel->palette().color(QPalette::Base), themes.optimizesEditorColors()));

    // What is counted may be other
    m_baseForms.clear();
    m_stale = true;
    if (m_shown) {
        refresh();
    } else {
        updateMarks();
    }
}

// =============================================================================
// The texts
// =============================================================================

EditorPanel* RepetitionsCoordinator::currentEditorPanel() const {
    return qobject_cast<EditorPanel*>(m_centralTabs->currentWidget());
}

editor::BookEditor* RepetitionsCoordinator::currentEditor() const {
    EditorPanel* panel = currentEditorPanel();
    return panel != nullptr ? panel->getBookEditor() : nullptr;
}

EditorPanel* RepetitionsCoordinator::openPanel(const QString& elementId) const {
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        auto* panel = qobject_cast<EditorPanel*>(m_centralTabs->widget(i));
        if (panel != nullptr && !elementId.isEmpty() && elementIdOf(panel) == elementId) {
            return panel;
        }
    }
    return nullptr;
}

QString RepetitionsCoordinator::elementIdOf(const EditorPanel* panel) {
    return panel != nullptr ? panel->property("elementId").toString() : QString();
}

QString RepetitionsCoordinator::titleOf(const EditorPanel* panel) const {
    const QString elementId = elementIdOf(panel);
    if (!elementId.isEmpty()) {
        if (const core::ProjectElement* element =
                core::ProjectManager::getInstance().findElement(elementId)) {
            return element->title;
        }
    }
    // A document outside the book: its tab's text, without the mark of changes
    QString title = m_centralTabs->tabText(m_centralTabs->indexOf(panel));
    if (title.startsWith(QLatin1Char('*'))) {
        title.remove(0, 1);
    }
    return title;
}

int RepetitionsCoordinator::textOf(const editor::BookEditor* editor) const {
    if (editor == nullptr) {
        return -1;
    }
    for (int i = 0; i < m_texts.size(); ++i) {
        if (m_texts.at(i).editor == editor) {
            return i;
        }
    }
    // A chapter counted from its file, opened since
    for (int tab = 0; tab < m_centralTabs->count(); ++tab) {
        auto* panel = qobject_cast<EditorPanel*>(m_centralTabs->widget(tab));
        if (panel == nullptr || panel->getBookEditor() != editor) {
            continue;
        }
        const QString elementId = elementIdOf(panel);
        for (int i = 0; i < m_texts.size(); ++i) {
            if (!elementId.isEmpty() && m_texts.at(i).elementId == elementId) {
                return i;
            }
        }
    }
    return -1;
}

QStringList RepetitionsCoordinator::paragraphsOf(const editor::BookEditor& editor) {
    QStringList paragraphs;
    const auto count = editor.paragraphCount();
    paragraphs.reserve(static_cast<qsizetype>(count));
    for (std::size_t i = 0; i < count; ++i) {
        paragraphs.append(editor.paragraphPlainText(i));
    }
    return paragraphs;
}

QList<RepetitionsCoordinator::Text> RepetitionsCoordinator::bookTexts() const {
    QList<Text> texts;
    auto& projects = core::ProjectManager::getInstance();
    const core::BookProject* project = projects.project();
    if (project == nullptr) {
        return texts;
    }
    // The text elements of the book, in reading order: those open as they are in the editor
    for (const core::ProjectElement* element : project->readingOrder()) {
        if (projects.formOf(*element) != core::ElementForm::Text) {
            continue;
        }
        Text text{element->id, element->title, {}, nullptr};
        if (EditorPanel* panel = openPanel(element->id)) {
            text.editor = panel->getBookEditor();
            text.paragraphs = paragraphsOf(*text.editor);
        } else {
            const QString path = projects.filePathOf(*element);
            if (!path.isEmpty() && QFileInfo::exists(path)) {
                if (const std::optional<core::ChapterDocument> chapter =
                        core::ChapterDocument::load(path)) {
                    text.paragraphs = core::ChapterDocument::kmlToPlainText(chapter->kml())
                                          .split(QLatin1Char('\n'));
                }
            }
        }
        texts.append(text);
    }
    return texts;
}

void RepetitionsCoordinator::follow(editor::BookEditor* editor) {
    if (editor == nullptr || m_followed.contains(editor)) {
        return;
    }
    m_followed.insert(editor);
    connect(editor, &editor::BookEditor::contentChanged, this,
            [this, editor]() { onTextChanged(editor); });
    connect(editor, &editor::BookEditor::documentChanged, this,
            [this, editor]() { onTextChanged(editor); });
    connect(editor, &QObject::destroyed, this, [this, editor]() {
        m_followed.remove(editor);
        for (Text& text : m_texts) {
            if (text.editor == editor) {
                text.editor = nullptr;
            }
        }
    });
}

// =============================================================================
// Counting
// =============================================================================

editor::RepetitionAnalysis RepetitionsCoordinator::analyze(const QList<Text>& texts,
                                                           editor::SpellCheckService* service) {
    QList<QStringList> paragraphs;
    paragraphs.reserve(texts.size());
    for (const Text& text : texts) {
        paragraphs.append(text.paragraphs);
    }
    // The language of the dictionary: its very common words
    const QString dictionary = service != nullptr ? service->currentDictionary() : QString();
    const QString language = !dictionary.isEmpty() ? dictionary : SpellingCoordinator::wantedLanguage();
    return editor::analyzeRepetitions(
        paragraphs, options(language),
        [this, service](const QString& word) { return baseFormOf(word, service); });
}

QString RepetitionsCoordinator::baseFormOf(const QString& word,
                                           editor::SpellCheckService* service) {
    if (service == nullptr) {
        return {};
    }
    // Asked once for each word of the dictionary
    const QString dictionary = service->currentDictionary();
    if (dictionary != m_baseFormsDictionary) {
        m_baseForms.clear();
        m_baseFormsDictionary = dictionary;
    }
    const auto known = m_baseForms.constFind(word);
    if (known != m_baseForms.constEnd()) {
        return known.value();
    }
    const QStringList stems = service->stems(word);
    const QString base = stems.isEmpty() ? QString() : editor::chooseBaseForm(word, stems, dictionary);
    m_baseForms.insert(word, base);
    return base;
}

void RepetitionsCoordinator::countChapter() {
    m_countTimer->stop();
    m_changedTexts.clear();
    EditorPanel* panel = currentEditorPanel();
    if (panel == nullptr) {
        m_texts.clear();
        m_analysis = {};
        m_stale = false;
        //: The panel Repetitions without a text in front. In Polish: Na wierzchu nie ma tekstu.
        //: Otwórz rozdział, żeby zobaczyć słowa, które wracają zbyt blisko.
        m_panel->setMessage(tr("No text is in front. Open a chapter to see the words that come "
                               "back too close."));
        updateMarks();
        return;
    }
    editor::BookEditor* editor = panel->getBookEditor();
    follow(editor);
    m_texts = {Text{elementIdOf(panel), titleOf(panel), paragraphsOf(*editor), editor}};
    m_analysis = analyze(m_texts, editor->spellCheckService());
    m_stale = false;
    showResults();
}

void RepetitionsCoordinator::countBook() {
    m_countTimer->stop();
    m_changedTexts.clear();
    m_texts = bookTexts();
    m_stale = false;
    if (m_texts.isEmpty()) {
        m_analysis = {};
        //: The panel Repetitions, the whole book, without texts. In Polish: Książka nie ma
        //: jeszcze tekstu.
        m_panel->setMessage(core::ProjectManager::getInstance().project() != nullptr
                                ? tr("The book has no text yet.")
                                : tr("No book is open."));
        updateMarks();
        return;
    }
    for (const Text& text : m_texts) {
        follow(text.editor);
    }
    m_analysis = analyze(m_texts, m_spelling != nullptr ? m_spelling->service() : nullptr);
    core::Logger::getInstance().debug("Repetitions: {} texts, {} words, {} close together",
                                      m_texts.size(), m_analysis.totalWords,
                                      m_analysis.clusters.size());
    showResults();
}

void RepetitionsCoordinator::onTextChanged(editor::BookEditor* editor) {
    const int text = textOf(editor);
    if (text < 0) {
        return;
    }
    // The marks of the paragraph written in move with its words: they go until it is counted
    // again; all of them when paragraphs came or went
    std::vector<editor::RepetitionMark> kept;
    if (static_cast<qsizetype>(editor->paragraphCount()) == m_texts.at(text).paragraphs.size()) {
        const int written = editor->cursorPosition().paragraph;
        for (const editor::RepetitionMark& mark : editor->repetitionMarks()) {
            if (mark.paragraph != written) {
                kept.push_back(mark);
            }
        }
    }
    if (kept.size() != editor->repetitionMarks().size()) {
        editor->setRepetitionMarks(kept, core::SettingsManager::getInstance().get<std::string>(
                                             STYLE_KEY) == "wave"
                                             ? editor::RepetitionMarkStyle::Wave
                                             : editor::RepetitionMarkStyle::Background);
    }

    if (m_panel->scope() == RepetitionsPanel::Scope::Chapter) {
        // Counted again when the writing stops, while the panel shows
        if (m_shown) {
            m_countTimer->start();
        } else {
            m_stale = true;
        }
        return;
    }
    // The whole book: counted again when the writer asks
    if (!m_changedTexts.contains(text)) {
        m_changedTexts.insert(text);
        showResults();
    }
}

void RepetitionsCoordinator::onCurrentTabChanged() {
    if (m_panel->scope() == RepetitionsPanel::Scope::Book) {
        updateMarks();
        return;
    }
    if (m_shown) {
        refresh();
    } else {
        m_stale = true;
    }
}

// =============================================================================
// The panel and the texts
// =============================================================================

void RepetitionsCoordinator::showResults() {
    const QLocale locale;
    const bool book = m_panel->scope() == RepetitionsPanel::Scope::Book;

    QList<RepetitionCard> cards;
    cards.reserve(m_analysis.clusters.size());
    for (const editor::RepetitionCluster& cluster : m_analysis.clusters) {
        QList<editor::TextWord> words;
        for (const int i : cluster.words) {
            words.append(m_analysis.words.at(i));
        }
        const int text = words.first().text;
        const editor::RepetitionContext context =
            editor::repetitionContext(m_texts.at(text).paragraphs, words);
        QStringList detail;
        if (book) {
            detail << m_texts.at(text).title;
        }
        if (cluster.forms.size() > 1 || cluster.forms.first() != cluster.key) {
            detail << cluster.forms.join(QStringLiteral(", "));
        }
        //: How many times a word comes back, and in how many words: "3 times in 19 words". In
        //: Polish: %n razy w %1 słowach
        detail << tr("%n time(s) in %1 words", "", static_cast<int>(cluster.words.size()))
                      .arg(locale.toString(cluster.span));
        cards.append({cluster.key, detail.join(QStringLiteral(" · ")), context.text,
                      context.marks});
    }

    QList<FrequentWord> frequent;
    const int shown = std::min(static_cast<int>(m_analysis.counts.size()), MAX_FREQUENT);
    for (int i = 0; i < shown; ++i) {
        const editor::WordCount& count = m_analysis.counts.at(i);
        QStringList forms;
        if (count.forms.size() > 1 || count.forms.first().first != count.key) {
            for (const auto& [form, times] : count.forms) {
                forms << form + QLatin1Char(' ') + locale.toString(times);
            }
        }
        const double perThousand =
            m_analysis.totalWords > 0 ? 1000.0 * count.count / m_analysis.totalWords : 0.0;
        frequent.append({count.key, count.count, perThousand, forms.join(QStringLiteral(", "))});
    }

    //: The panel Repetitions: what is counted, the whole book. In Polish: Cała książka
    const QString name = book ? tr("Whole book")
                              : (m_texts.isEmpty() ? QString() : m_texts.first().title);
    //: The words counted. In Polish: %Ln słów (słowo, słowa)
    const QString words = tr("%Ln word(s)", "", m_analysis.totalWords);
    //: In Polish: %Ln powtórzeń blisko siebie (powtórzenie, powtórzenia)
    const QString close = tr("%Ln repetition(s) close together", "",
                             static_cast<int>(m_analysis.clusters.size()));
    //: The words counted that are not the same. In Polish: %Ln różnych (różne, różnych)
    const QString different = tr("%Ln different", "", static_cast<int>(m_analysis.counts.size()));
    QString closeSummary = QStringLiteral("%1: %2 · %3").arg(name, words, close);
    QString frequentSummary = QStringLiteral("%1: %2, %3").arg(name, words, different);
    if (!m_changedTexts.isEmpty()) {
        //: The whole book was counted before a chapter changed. In Polish: Tekst zmienił się
        //: po przeliczeniu – kliknij „Przelicz”.
        const QString changed = tr("The text changed after it was counted: click Recount.");
        closeSummary += QLatin1Char('\n') + changed;
        frequentSummary += QLatin1Char('\n') + changed;
    }
    m_panel->setMessage({});
    m_panel->setResults(cards, frequent, closeSummary, frequentSummary);
    updateMarks();
}

void RepetitionsCoordinator::updateMarks() {
    const editor::RepetitionMarkStyle style =
        core::SettingsManager::getInstance().get<std::string>(STYLE_KEY) == "wave"
            ? editor::RepetitionMarkStyle::Wave
            : editor::RepetitionMarkStyle::Background;
    for (int tab = 0; tab < m_centralTabs->count(); ++tab) {
        auto* panel = qobject_cast<EditorPanel*>(m_centralTabs->widget(tab));
        if (panel == nullptr) {
            continue;
        }
        editor::BookEditor* editor = panel->getBookEditor();
        const int text = m_shown ? textOf(editor) : -1;
        editor->setRepetitionMarks(text >= 0 ? marksOf(text) : std::vector<editor::RepetitionMark>{},
                                   style);
    }
}

std::vector<editor::RepetitionMark> RepetitionsCoordinator::marksOf(int text) const {
    std::vector<editor::RepetitionMark> marks;
    if (m_changedTexts.contains(text)) {
        return marks;  // the words are not where they were counted
    }
    // The words of the card or of the word chosen: framed
    QSet<int> chosen;
    if (m_panel->page() == RepetitionsPanel::Page::Close) {
        const int card = m_panel->currentCard();
        if (card >= 0 && card < m_analysis.clusters.size()) {
            for (const int i : m_analysis.clusters.at(card).words) {
                chosen.insert(i);
            }
        }
    } else {
        const int word = m_panel->currentWord();
        if (word >= 0 && word < m_analysis.counts.size()) {
            for (const int i : m_analysis.counts.at(word).words) {
                chosen.insert(i);
            }
        }
    }
    const auto add = [this, text, &marks](int i, bool current) {
        const editor::TextWord& word = m_analysis.words.at(i);
        if (word.text == text) {
            marks.push_back({word.paragraph, word.start, word.length, current});
        }
    };
    if (core::SettingsManager::getInstance().get<bool>(HIGHLIGHT_KEY)) {
        for (const editor::RepetitionCluster& cluster : m_analysis.clusters) {
            for (const int i : cluster.words) {
                add(i, chosen.remove(i));
            }
        }
    }
    for (const int i : std::as_const(chosen)) {
        add(i, true);
    }
    return marks;
}

editor::BookEditor* RepetitionsCoordinator::editorOf(int text, bool open) {
    if (text < 0 || text >= m_texts.size()) {
        return nullptr;
    }
    const Text& counted = m_texts.at(text);
    if (counted.editor != nullptr) {
        return counted.editor;
    }
    if (EditorPanel* panel = openPanel(counted.elementId)) {
        return panel->getBookEditor();
    }
    if (!open || counted.elementId.isEmpty() || !m_openChapter) {
        return nullptr;
    }
    m_openChapter(counted.elementId);
    EditorPanel* panel = openPanel(counted.elementId);
    if (panel == nullptr) {
        return nullptr;
    }
    follow(panel->getBookEditor());
    updateMarks();
    return panel->getBookEditor();
}

bool RepetitionsCoordinator::wordsInPlace(const editor::BookEditor& editor,
                                          const QList<int>& words) const {
    for (const int i : words) {
        const editor::TextWord& word = m_analysis.words.at(i);
        if (static_cast<std::size_t>(word.paragraph) >= editor.paragraphCount() ||
            editor.paragraphPlainText(static_cast<std::size_t>(word.paragraph))
                    .mid(word.start, word.length)
                    .toLower() != word.form) {
            return false;
        }
    }
    return true;
}

void RepetitionsCoordinator::scrollTo(editor::BookEditor& editor, const editor::TextWord& word) {
    // Into view, the cursor where it is
    const QTextDocument* document = editor.textDocument();
    if (document == nullptr) {
        return;
    }
    const QTextBlock block = document->findBlockByNumber(word.paragraph);
    if (!block.isValid()) {
        return;
    }
    const QRectF place = editor.placeRect(block.position() + word.start, false);
    if (place.top() < 0.0 || place.bottom() > editor.height()) {
        editor.scrollBy(place.center().y() - editor.height() / 2.0);
    }
}

editor::CursorPosition RepetitionsCoordinator::after(const editor::BookEditor& editor) {
    return editor.hasSelection() ? editor.selection().normalized().end : editor.cursorPosition();
}

void RepetitionsCoordinator::bringToFront(const editor::BookEditor* editor) {
    for (int tab = 0; tab < m_centralTabs->count(); ++tab) {
        auto* panel = qobject_cast<EditorPanel*>(m_centralTabs->widget(tab));
        if (panel != nullptr && panel->getBookEditor() == editor) {
            m_centralTabs->setCurrentIndex(tab);
            return;
        }
    }
}

void RepetitionsCoordinator::selectWord(editor::BookEditor& editor, const editor::TextWord& word) {
    editor.setSelection({{word.paragraph, word.start}, {word.paragraph, word.start + word.length}});
    editor.ensureCursorVisible();
}

void RepetitionsCoordinator::onCardSelected(int index) {
    updateMarks();
    if (index < 0 || index >= m_analysis.clusters.size()) {
        return;
    }
    const editor::RepetitionCluster& cluster = m_analysis.clusters.at(index);
    const editor::TextWord& first = m_analysis.words.at(cluster.words.first());
    // The text of the card in front, when it is open; the keys stay in the panel
    editor::BookEditor* editor = editorOf(first.text, false);
    if (editor == nullptr) {
        return;
    }
    if (m_panel->scope() == RepetitionsPanel::Scope::Book) {
        bringToFront(editor);
    }
    if (!m_changedTexts.contains(first.text)) {
        scrollTo(*editor, first);
    }
}

void RepetitionsCoordinator::onCardActivated(int index) {
    if (index < 0 || index >= m_analysis.clusters.size()) {
        return;
    }
    const editor::RepetitionCluster& cluster = m_analysis.clusters.at(index);
    const editor::TextWord& first = m_analysis.words.at(cluster.words.first());
    // A chapter of the book not open opens
    editor::BookEditor* editor = editorOf(first.text, true);
    if (editor == nullptr) {
        return;
    }
    if (m_panel->scope() == RepetitionsPanel::Scope::Book) {
        bringToFront(editor);
    }
    if (!wordsInPlace(*editor, cluster.words)) {
        //: The words of a card are not where they were counted. In Polish: Tekst zmienił się:
        //: powtórzenia są liczone od nowa
        showMessage(tr("The text changed: the repetitions are counted again"));
        refresh();
        return;
    }
    selectWord(*editor, first);
    editor->setFocus(Qt::OtherFocusReason);
}

void RepetitionsCoordinator::onSkipWord(int index) {
    if (index < 0 || index >= m_analysis.clusters.size()) {
        return;
    }
    const QString word = m_analysis.clusters.at(index).key;
    skipWord(word);  // counted again: the settings changed
    //: Skip This Word on a card of the panel Repetitions. In Polish: „%1” nie jest już liczone.
    //: Pomijane słowa są w Ustawieniach > Edytor > Powtórzenia
    showMessage(tr("“%1” is not counted any more. The skipped words are in Settings > "
                   "Editor > Repetitions")
                    .arg(word));
}

void RepetitionsCoordinator::onWordActivated(int index) {
    if (index < 0 || index >= m_analysis.counts.size()) {
        return;
    }
    const QList<int>& words = m_analysis.counts.at(index).words;
    // The next use after the cursor in the text in front
    editor::BookEditor* editor = currentEditor();
    const int text = textOf(editor);
    if (editor != nullptr && text >= 0 && !m_changedTexts.contains(text)) {
        const editor::CursorPosition cursor = after(*editor);
        for (const int i : words) {
            const editor::TextWord& word = m_analysis.words.at(i);
            if (word.text == text &&
                (word.paragraph > cursor.paragraph ||
                 (word.paragraph == cursor.paragraph && word.start >= cursor.offset))) {
                selectWord(*editor, word);
                return;
            }
        }
    }
    // Else the first use in a later text (its chapter opened), else the first of all
    const auto goTo = [this](const editor::TextWord& word) {
        if (m_changedTexts.contains(word.text)) {
            return false;  // the words are not where they were counted
        }
        editor::BookEditor* next = editorOf(word.text, true);
        if (next == nullptr) {
            return false;
        }
        bringToFront(next);
        selectWord(*next, word);
        return true;
    };
    for (const int i : words) {
        if (m_analysis.words.at(i).text > text && goTo(m_analysis.words.at(i))) {
            return;
        }
    }
    for (const int i : words) {
        if (goTo(m_analysis.words.at(i))) {
            return;
        }
    }
}

void RepetitionsCoordinator::onDockVisibilityChanged(bool visible) {
    m_shown = visible;
    if (visible && m_stale) {
        refresh();
    } else {
        updateMarks();
    }
}

void RepetitionsCoordinator::showMessage(const QString& message) {
    if (m_statusBar != nullptr) {
        m_statusBar->showMessage(message, MESSAGE_MS);
    }
}

}  // namespace kalahari::gui
