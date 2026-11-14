// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_search_coordinator.cpp
/// @brief Edit > Find in Book...: the search in all the texts of the book in the Search panel,
///        and Replace All in Book with Undo

#include "kalahari/gui/book_search_coordinator.h"

#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/gui/book_texts.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/read_only_book.h"

#include <QApplication>
#include <QLocale>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextDocument>

#include <algorithm>
#include <utility>

namespace kalahari::gui {

namespace {

/// The command
constexpr const char* FIND_IN_BOOK_COMMAND = "edit.findInBook";

/// How long a short message stays in the status bar, in milliseconds
constexpr int MESSAGE_MS = 8000;

/// How often the panel shows the matches found while the search goes on, in milliseconds
constexpr int PUBLISH_MS = 200;

/// The most of the text before a match the panel shows, and of the text after it
constexpr int CONTEXT_BEFORE = 30;
constexpr int CONTEXT_AFTER = 80;

/// Where the text before a match may start later, to start with a whole word
constexpr int WORD_START_LOOKAHEAD = 12;

/// The character the panel shows where a context is cut
const QChar ELLIPSIS(0x2026);

/// A character of a paragraph the panel shows as a space: a line break, a tab, an object
bool shownAsSpace(QChar c) {
    return c == QChar::LineSeparator || c == QChar::ParagraphSeparator || c == u'\t' ||
           c == u'\n' || c == QChar::ObjectReplacementCharacter;
}

}  // namespace

// =============================================================================
// The line around a match
// =============================================================================

SearchPanelHit shownSearchHit(const QString& line, int offset, int length, const QString& note) {
    offset = std::clamp(offset, 0, static_cast<int>(line.size()));
    length = std::clamp(length, 0, static_cast<int>(line.size()) - offset);

    // Before the match: at most CONTEXT_BEFORE characters, from the start of a word
    int from = std::max(0, offset - CONTEXT_BEFORE);
    bool cutBefore = from > 0;
    if (cutBefore) {
        for (int i = from; i < std::min(offset, from + WORD_START_LOOKAHEAD); ++i) {
            if (line.at(i).isSpace() || shownAsSpace(line.at(i))) {
                from = i + 1;
                break;
            }
        }
    }
    // After it: at most CONTEXT_AFTER characters
    const int end = offset + length;
    const int to = std::min(static_cast<int>(line.size()), end + CONTEXT_AFTER);
    const bool cutAfter = to < line.size();

    QString context = line.mid(from, to - from);
    for (QChar& c : context) {
        if (shownAsSpace(c)) {
            c = u' ';
        }
    }
    int start = offset - from;
    // Without the spaces it starts with (the match keeps its own)
    qsizetype leading = 0;
    while (leading < start && context.at(leading).isSpace()) {
        ++leading;
    }
    context.remove(0, leading);
    start -= static_cast<int>(leading);
    if (cutBefore) {
        context.prepend(ELLIPSIS);
        ++start;
    }
    if (cutAfter) {
        context.append(ELLIPSIS);
    }
    return {context, start, length, note};
}

// =============================================================================
// Construction
// =============================================================================

BookSearchCoordinator::BookSearchCoordinator(core::ProjectManager& book, CommandRegistry& commands,
                                             QTabWidget* centralTabs, SearchPanel* panel,
                                             PanelShower showPanel, TextWithKeys textWithKeys,
                                             ChapterOpener openChapter, NoteContexts noteContexts,
                                             NoticeShower showNotice, QStatusBar* statusBar,
                                             QWidget* window, QObject* parent)
    : QObject(parent),
      m_book(book),
      m_commands(commands),
      m_centralTabs(centralTabs),
      m_panel(panel),
      m_showPanel(std::move(showPanel)),
      m_textWithKeys(std::move(textWithKeys)),
      m_openChapter(std::move(openChapter)),
      m_noteContexts(std::move(noteContexts)),
      m_showNotice(std::move(showNotice)),
      m_statusBar(statusBar),
      m_window(window)
{
    m_sliceTimer.setSingleShot(true);
    m_sliceTimer.setInterval(0);
    connect(&m_sliceTimer, &QTimer::timeout, this, &BookSearchCoordinator::searchSlice);
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(REFRESH_DELAY_MS);
    connect(&m_refreshTimer, &QTimer::timeout, this, &BookSearchCoordinator::refreshChanged);
    m_published.start();

    if (m_panel != nullptr) {
        connect(m_panel, &SearchPanel::searchRequested, this, &BookSearchCoordinator::search);
        // Chosen with the keys staying in the panel: an open text shows it
        connect(m_panel, &SearchPanel::hitChosen, this, [this](int text, int match) {
            if (text >= 0 && text < static_cast<int>(m_found.size()) &&
                openEditors().contains(m_found[static_cast<size_t>(text)].elementId)) {
                goToMatch(text, match, false);
            }
        });
        connect(m_panel, &SearchPanel::hitActivated, this,
                [this](int text, int match, bool keys) { goToMatch(text, match, keys); });
        connect(m_panel, &SearchPanel::replaceAllRequested, this,
                [this]() { replaceAllInBook(); });
        connect(m_panel, &SearchPanel::editorFocusRequested, this, [this]() {
            if (m_centralTabs == nullptr) {
                return;
            }
            if (auto* front =
                    qobject_cast<EditorPanel*>(DocumentWindows::of(*m_centralTabs).frontPage());
                front != nullptr && front->getBookEditor() != nullptr) {
                front->getBookEditor()->window()->activateWindow();
                front->getBookEditor()->setFocus(Qt::OtherFocusReason);
            }
        });
        updateMarkColor();
        publish();
    }

    // Another book, or none: what was found and read is of the old one
    auto& projects = m_book;
    connect(&projects, &core::ProjectManager::projectOpened, this, &BookSearchCoordinator::reset);
    connect(&projects, &core::ProjectManager::projectClosed, this, &BookSearchCoordinator::reset);
    // Insert > Footnote Layout...: other numbers of the notes the matches are in
    connect(&projects, &core::ProjectManager::bookFootnotesChanged, this,
            &BookSearchCoordinator::searchAgain);

    // The open texts' changes are followed from when they open (in front of their window)
    if (m_centralTabs != nullptr) {
        DocumentWindows& windows = DocumentWindows::of(*m_centralTabs);
        const auto followOpen = [this]() {
            const QHash<QString, editor::BookEditor*> open = openEditors();
            for (auto it = open.cbegin(); it != open.cend(); ++it) {
                follow(it.value(), it.key());
            }
        };
        connect(&windows, &DocumentWindows::windowsChanged, this, followOpen);
        connect(&windows, &DocumentWindows::frontChanged, this, followOpen);
    }

    // The color of the hits, as the editors show it
    auto& themes = core::ThemeManager::getInstance();
    connect(&themes, &core::ThemeManager::themeChanged, this,
            &BookSearchCoordinator::updateMarkColor);
    connect(&themes, &core::ThemeManager::editorColorsChanged, this,
            &BookSearchCoordinator::updateMarkColor);
}

void BookSearchCoordinator::connectCommands() {
    auto& registry = m_commands;
    if (Command* command = registry.getCommand(FIND_IN_BOOK_COMMAND)) {
        command->execute = [this]() { findInBook(); };
        command->isEnabled = [this]() { return m_book.isProjectOpen(); };
        //: Why Edit > Find in Book... is greyed out. In Polish: Żadna książka nie jest otwarta
        command->disabledReason = []() { return tr("No book is open"); };
        registry.updateActionState(FIND_IN_BOOK_COMMAND);
    }
    auto& projects = m_book;
    const auto updateCommand = [this]() { m_commands.updateActionState(FIND_IN_BOOK_COMMAND); };
    connect(&projects, &core::ProjectManager::projectOpened, this, updateCommand);
    connect(&projects, &core::ProjectManager::projectClosed, this, updateCommand);
}

// =============================================================================
// The search
// =============================================================================

void BookSearchCoordinator::findInBook() {
    if (m_panel == nullptr) {
        return;
    }
    if (m_showPanel) {
        m_showPanel();
    }
    // What is selected in the text with the keys, else what its find bar searches for, with
    // the bar's options
    if (editor::BookEditor* text = m_textWithKeys ? m_textWithKeys() : nullptr) {
        const editor::SearchEngine* bar = text->searchEngine();
        const QString selected = text->searchTextFromSelection();
        if (!selected.isEmpty()) {
            m_panel->setSearchText(selected);
        } else if (bar != nullptr && bar->isActive()) {
            m_panel->setSearchText(bar->searchText());
        }
        if (bar != nullptr && bar->isActive()) {
            m_panel->setOptions(bar->options());
        }
    }
    m_panel->focusSearchField();
    search();
}

void BookSearchCoordinator::search() {
    m_sliceTimer.stop();
    m_refreshTimer.stop();
    m_changed.clear();
    m_queue.clear();
    m_next = 0;
    m_found.clear();
    m_query.reset();

    const QString text = m_panel != nullptr ? m_panel->searchText() : QString();
    if (!m_book.isProjectOpen() || text.isEmpty()) {
        publish();
        return;
    }
    m_query = Query{text, m_panel->options()};
    m_engine.setSearchText(text);
    m_engine.setOptions(m_query->options);
    if (m_engine.canSearch()) {
        for (const BookTextElement& element : bookTextElements(m_book)) {
            m_queue.append({element.element->id, element.element->title});
        }
    }
    m_published.restart();
    searchSlice();  // the first texts at once
}

void BookSearchCoordinator::searchAgain() {
    if (m_query) {
        search();
    }
}

void BookSearchCoordinator::finishSearch() {
    m_sliceTimer.stop();
    while (isSearching()) {
        searchSlice();
    }
    m_sliceTimer.stop();
}

void BookSearchCoordinator::searchSlice() {
    QElapsedTimer clock;
    clock.start();
    const QHash<QString, editor::BookEditor*> open = openEditors();
    const size_t before = m_found.size();
    while (isSearching()) {
        const QueuedText& text = m_queue.at(m_next++);
        TextMatches matches =
            searchText(text.elementId, text.title, open.value(text.elementId, nullptr));
        if (!matches.matches.empty()) {
            m_found.push_back(std::move(matches));
        }
        if (clock.elapsed() >= SLICE_MS) {
            break;
        }
    }
    if (isSearching()) {
        // The matches so far now and then, how far it is always
        if (m_found.size() != before && m_published.elapsed() >= PUBLISH_MS) {
            publish();
        } else if (m_panel != nullptr) {
            m_panel->setStatus(tr("Searching... %1 of %n texts", nullptr,
                                  static_cast<int>(m_queue.size()))
                                   .arg(m_next));
        }
        m_sliceTimer.start();
        return;
    }
    publish();
    // The texts changed while the search went on: their matches as they are now
    if (!m_changed.isEmpty()) {
        m_refreshTimer.start();
    }
    emit searchFinished();
}

const std::vector<editor::SearchableParagraph>&
BookSearchCoordinator::paragraphsOf(const QString& elementId, editor::BookEditor* editor) {
    ReadText& read = m_read[elementId];
    if (editor != nullptr && editor->textDocument() != nullptr) {
        follow(editor, elementId);
        if (!read.current) {
            read.paragraphs = editor::searchableParagraphs(*editor->textDocument());
            read.current = true;
            read.signature.clear();  // read as it is kept again, once it is closed
        }
        return read.paragraphs;
    }
    read.current = false;
    if (const std::optional<QString> kml = savedTextIfChanged(m_book, elementId, read.signature)) {
        read.paragraphs = editor::searchableParagraphsOfKml(*kml);
    }
    return read.paragraphs;
}

BookSearchCoordinator::TextMatches BookSearchCoordinator::searchText(const QString& elementId,
                                                                     const QString& title,
                                                                     editor::BookEditor* editor) {
    TextMatches result{elementId, title, {}, {}};
    const std::vector<editor::SearchableParagraph>& paragraphs = paragraphsOf(elementId, editor);
    result.matches = m_engine.findIn(paragraphs);
    if (result.matches.empty()) {
        return result;
    }

    // The names of its notes, with their numbers, for the matches in them
    QHash<QString, QString> noteNames;
    if (std::any_of(result.matches.cbegin(), result.matches.cend(),
                    [](const editor::SearchMatch& match) { return match.inNote(); })) {
        const editor::FootnoteContext context =
            editor != nullptr ? editor->footnoteContext()
                              : (m_noteContexts ? m_noteContexts(elementId)
                                                : editor::FootnoteContext{});
        int footnotes = context.footnotesBefore;
        int endnotes = context.endnotesBefore;
        for (const editor::SearchableParagraph& paragraph : paragraphs) {
            for (const editor::SearchableNote& note : paragraph.notes) {
                if (note.kind == editor::NoteKind::Endnote) {
                    //: A match in an endnote in the Search panel. In Polish: przypis końcowy %1
                    noteNames.insert(note.id, tr("endnote %1").arg(editor::noteNumberText(
                                                  ++endnotes, context.endnoteFormat)));
                } else {
                    //: A match in a footnote in the Search panel. In Polish: przypis dolny %1
                    noteNames.insert(note.id, tr("footnote %1").arg(editor::noteNumberText(
                                                  ++footnotes, context.footnoteFormat)));
                }
            }
        }
    }

    // The lines around the first matches
    const size_t listed = std::min(result.matches.size(), static_cast<size_t>(MAX_LISTED_MATCHES));
    result.hits.reserve(static_cast<qsizetype>(listed));
    for (size_t i = 0; i < listed; ++i) {
        const editor::SearchMatch& match = result.matches[i];
        const auto length = static_cast<int>(match.length);
        const auto number = static_cast<size_t>(match.paragraph);
        if (number >= paragraphs.size()) {
            break;
        }
        const editor::SearchableParagraph& paragraph = paragraphs[number];
        if (!match.inNote()) {
            result.hits.append(shownSearchHit(paragraph.text, match.paragraphOffset, length));
            continue;
        }
        const auto note = std::find_if(
            paragraph.notes.cbegin(), paragraph.notes.cend(),
            [&match](const editor::SearchableNote& found) { return found.id == match.noteId; });
        if (note == paragraph.notes.cend()) {
            result.hits.append(SearchPanelHit{match.matchedText, 0, length, QString()});
            continue;
        }
        // The line of the note's text the match is in
        const int lineStart =
            match.noteOffset > 0
                ? static_cast<int>(note->text.lastIndexOf(u'\n', match.noteOffset - 1)) + 1
                : 0;
        qsizetype lineEnd = note->text.indexOf(u'\n', match.noteOffset);
        if (lineEnd < 0) {
            lineEnd = note->text.size();
        }
        result.hits.append(shownSearchHit(note->text.mid(lineStart, lineEnd - lineStart),
                                          match.noteOffset - lineStart, length,
                                          noteNames.value(match.noteId)));
    }
    return result;
}

void BookSearchCoordinator::refreshChanged() {
    if (isSearching()) {
        m_refreshTimer.start();  // once the search is through
        return;
    }
    const QSet<QString> changed = std::exchange(m_changed, {});
    if (!m_query || changed.isEmpty()) {
        return;
    }
    // The texts in the order of the book: those changed searched again, and those with
    // matches in their notes (the numbers of their notes can follow those of a text before)
    const QHash<QString, editor::BookEditor*> open = openEditors();
    QHash<QString, size_t> before;
    for (size_t i = 0; i < m_found.size(); ++i) {
        before.insert(m_found[i].elementId, i);
    }
    const auto inNotes = [this, &before](const QString& elementId) {
        const auto it = before.constFind(elementId);
        return it != before.cend() &&
               std::any_of(m_found[it.value()].matches.cbegin(), m_found[it.value()].matches.cend(),
                           [](const editor::SearchMatch& match) { return match.inNote(); });
    };
    std::vector<TextMatches> found;
    for (const QueuedText& text : std::as_const(m_queue)) {
        if (changed.contains(text.elementId) || inNotes(text.elementId)) {
            TextMatches matches =
                searchText(text.elementId, text.title, open.value(text.elementId, nullptr));
            if (!matches.matches.empty()) {
                found.push_back(std::move(matches));
            }
        } else if (const auto it = before.constFind(text.elementId); it != before.cend()) {
            found.push_back(std::move(m_found[it.value()]));
        }
    }
    m_found = std::move(found);
    publish();
}

void BookSearchCoordinator::publish() {
    m_published.restart();
    if (m_panel == nullptr) {
        return;
    }
    QList<SearchPanelText> texts;
    int listed = 0;
    int total = 0;
    for (const TextMatches& text : m_found) {
        SearchPanelText shown{text.elementId, text.title, static_cast<int>(text.matches.size()),
                              {}};
        shown.hits = text.hits.mid(0, std::max(0, MAX_LISTED_MATCHES - listed));
        listed += static_cast<int>(shown.hits.size());
        total += shown.count;
        texts.append(std::move(shown));
    }
    m_panel->setTexts(texts);

    QString status;
    if (!m_book.isProjectOpen()) {
        //: The Search panel without a book. In Polish: Żadna książka nie jest otwarta
        status = tr("No book is open");
    } else if (!m_query) {
        //: The Search panel before a search. In Polish: Wpisz, czego szukać we wszystkich
        //: tekstach książki.
        status = tr("Type what to find in all the texts of the book.");
    } else if (!m_engine.canSearch()) {
        //: In Polish: Wyrażenie regularne jest nieprawidłowe.
        status = tr("The regular expression is not valid.");
    } else if (isSearching()) {
        //: How far the search of the book is. In Polish: Szukanie... %1 z %n tekstu / z %n
        //: tekstów / z %n tekstów
        status = tr("Searching... %1 of %n texts", nullptr, static_cast<int>(m_queue.size()))
                     .arg(m_next);
    } else if (total == 0) {
        //: In Polish: W książce nie ma trafień.
        status = tr("No matches in the book.");
    } else {
        //: What the search of the book found: "12 matches" (%1) "in 4 texts" (%2). In Polish
        //: the same: %1 %2.
        status = tr("%1 %2.").arg(
            //: In Polish: %n trafienie / %n trafienia / %n trafień
            tr("%n matches", nullptr, total),
            //: In Polish: w %n tekście / w %n tekstach / w %n tekstach
            tr("in %n texts", nullptr, static_cast<int>(m_found.size())));
        if (listed < total) {
            //: In Polish: Lista pokazuje pierwsze %1.
            status += QLatin1Char(' ') +
                      tr("The list shows the first %1.").arg(QLocale().toString(listed));
        }
    }
    m_panel->setStatus(status);
    const QString whyNot = whyNotReplace();
    m_panel->setReplaceEnabled(whyNot.isEmpty(), whyNot);
}

// =============================================================================
// The texts
// =============================================================================

QHash<QString, editor::BookEditor*> BookSearchCoordinator::openEditors() const {
    QHash<QString, editor::BookEditor*> editors;
    if (m_centralTabs == nullptr) {
        return editors;
    }
    for (QWidget* page : DocumentWindows::of(*m_centralTabs).pages()) {
        auto* panel = qobject_cast<EditorPanel*>(page);
        const QString elementId = page->property("elementId").toString();
        if (panel != nullptr && panel->getBookEditor() != nullptr && !elementId.isEmpty()) {
            editors.insert(elementId, panel->getBookEditor());
        }
    }
    return editors;
}

EditorPanel* BookSearchCoordinator::panelOf(const editor::BookEditor* editor) const {
    if (m_centralTabs == nullptr || editor == nullptr) {
        return nullptr;
    }
    for (QWidget* page : DocumentWindows::of(*m_centralTabs).pages()) {
        auto* panel = qobject_cast<EditorPanel*>(page);
        if (panel != nullptr && panel->getBookEditor() == editor) {
            return panel;
        }
    }
    return nullptr;
}

void BookSearchCoordinator::follow(editor::BookEditor* editor, const QString& elementId) {
    if (editor == nullptr || elementId.isEmpty() || m_followed.contains(editor)) {
        return;
    }
    // Its document, and the next one it gets (another text read into it)
    const auto watchDocument = [this, editor, elementId]() {
        disconnect(m_followed.value(editor));
        QMetaObject::Connection connection;
        if (QTextDocument* document = editor->textDocument()) {
            connection = connect(document, &QTextDocument::contentsChanged, this,
                                 [this, elementId]() { onTextChanged(elementId); });
        }
        m_followed.insert(editor, connection);
    };
    m_followed.insert(editor, QMetaObject::Connection());
    connect(editor, &editor::BookEditor::documentChanged, this, [this, watchDocument, elementId]() {
        watchDocument();
        onTextChanged(elementId);
    });
    // Closed: its matches as the book keeps it (its changes saved or not)
    connect(editor, &QObject::destroyed, this, [this, editor, elementId]() {
        m_followed.remove(editor);
        onTextChanged(elementId);
    });
    watchDocument();
}

void BookSearchCoordinator::onTextChanged(const QString& elementId) {
    if (const auto it = m_read.find(elementId); it != m_read.end()) {
        it->current = false;
    }
    if (m_query) {
        m_changed.insert(elementId);
        m_refreshTimer.start();
    }
}

// =============================================================================
// Going to a match
// =============================================================================

bool BookSearchCoordinator::goToMatch(int text, int match, bool keys) {
    if (text < 0 || text >= static_cast<int>(m_found.size()) || match < 0 ||
        match >= static_cast<int>(m_found[static_cast<size_t>(text)].matches.size())) {
        return false;
    }
    const QString elementId = m_found[static_cast<size_t>(text)].elementId;
    // A click on a match leaves the keys in the panel (the text opening takes them)
    const bool panelHasKeys =
        m_panel != nullptr && m_panel->isAncestorOf(QApplication::focusWidget());

    // The text in front, opened if it is not
    editor::BookEditor* editor = openEditors().value(elementId, nullptr);
    if (editor == nullptr && m_openChapter) {
        m_openChapter(elementId);
        editor = openEditors().value(elementId, nullptr);
    } else if (editor != nullptr && m_centralTabs != nullptr) {
        if (EditorPanel* panel = panelOf(editor)) {
            DocumentWindows::of(*m_centralTabs).showPage(panel);
        }
    }
    if (editor == nullptr) {
        return false;
    }
    follow(editor, elementId);

    // A text changed since it was searched: its matches as they are now
    auto showMatch = [&]() {
        const TextMatches& found = m_found[static_cast<size_t>(text)];
        return match >= 0 && match < static_cast<int>(found.matches.size()) &&
               editor->goToSearchMatch(found.matches[static_cast<size_t>(match)]);
    };
    if (!showMatch()) {
        finishSearch();  // refreshChanged() waits for the search otherwise
        m_changed.insert(elementId);
        m_read[elementId].current = false;
        refreshChanged();
        const auto it = std::find_if(m_found.cbegin(), m_found.cend(),
                                     [&elementId](const TextMatches& found) {
                                         return found.elementId == elementId;
                                     });
        if (it == m_found.cend()) {
            //: In Polish: Tekst zmienił się i nie ma już tego trafienia.
            say(tr("The text changed and does not have this match any more."));
            return false;
        }
        text = static_cast<int>(it - m_found.cbegin());
        match = std::min(match, static_cast<int>(it->matches.size()) - 1);
        if (m_panel != nullptr) {
            m_panel->setCurrentHit({text, match});
        }
        if (!showMatch()) {
            return false;
        }
    }

    if (keys) {
        editor->window()->activateWindow();
        editor->setFocus(Qt::OtherFocusReason);
    } else if (panelHasKeys) {
        // Back to the widget of the panel they were in (the tree of the matches)
        m_panel->window()->activateWindow();
        QWidget* last = m_panel->focusWidget();
        (last != nullptr ? last : m_panel)->setFocus(Qt::OtherFocusReason);
    }
    return true;
}

// =============================================================================
// Replace All in Book
// =============================================================================

QString BookSearchCoordinator::whyNotReplace() const {
    const auto& projects = m_book;
    if (!projects.isProjectOpen()) {
        //: Why Replace All in Book is greyed out. In Polish: Żadna książka nie jest otwarta
        return tr("No book is open");
    }
    if (projects.isReadOnly()) {
        return readOnlyReason();
    }
    if (!m_query || !m_engine.canSearch()) {
        //: In Polish: Najpierw wpisz, czego szukać
        return tr("Type what to find first");
    }
    if (!isSearching() && m_found.empty()) {
        //: In Polish: Nie ma czego zamienić
        return tr("Nothing was found to replace");
    }
    return QString();
}

bool BookSearchCoordinator::confirm(int matches, int texts) const {
    if (m_confirm) {
        return m_confirm(matches, texts);
    }
    const QString replacement = m_panel != nullptr ? m_panel->replaceText() : QString();
    const QString found = m_query ? m_query->text : QString();
    //: In Polish: %n trafienie „%1” zmieni się na „%2”. / %n trafienia „%1” zmienią się na
    //: „%2”. / %n trafień „%1” zmieni się na „%2”.
    QString question = tr("%n matches of \"%1\" will change to \"%2\".", nullptr, matches)
                           .arg(found, replacement);
    if (replacement.isEmpty()) {
        //: With nothing to replace them with. In Polish: %n trafienie „%1” zostanie usunięte.
        //: / %n trafienia „%1” zostaną usunięte. / %n trafień „%1” zostanie usuniętych.
        question = tr("%n matches of \"%1\" will be deleted.", nullptr, matches).arg(found);
    }
    //: In Polish: Teksty z trafieniami: %1. W każdym otwartym tekście zmianę można cofnąć
    //: jednym krokiem (Ctrl+Z).
    question += QLatin1Char(' ') + tr("Texts with matches: %1. Each open text can undo the "
                                      "change in one step (Ctrl+Z).")
                                       .arg(texts);
    //: In Polish: Zamień wszystko w książce / &Zamień
    return dialogs::MessageDialog::confirm(m_window, tr("Replace All in Book"), question,
                                           tr("&Replace"), dialogs::MessageDialog::Kind::Question);
}

int BookSearchCoordinator::replaceAllInBook() {
    auto& projects = m_book;
    if (!projects.isProjectOpen() || projects.isReadOnly() || !m_query || !m_engine.canSearch()) {
        say(whyNotReplace());
        return -1;
    }
    // Every text searched, as it is now
    finishSearch();
    m_refreshTimer.stop();
    refreshChanged();

    const QString replacement = m_panel != nullptr ? m_panel->replaceText() : QString();
    const QHash<QString, editor::BookEditor*> open = openEditors();
    int matches = 0;
    int texts = 0;
    for (const TextMatches& text : m_found) {
        if (const editor::BookEditor* editor = open.value(text.elementId, nullptr);
            editor != nullptr && editor->isReadOnly()) {
            continue;
        }
        matches += static_cast<int>(text.matches.size());
        ++texts;
    }
    if (matches == 0) {
        say(tr("Nothing was found to replace"));
        return 0;
    }
    if (!confirm(matches, texts) || !m_query) {
        return 0;
    }

    // The change, kept so that it can be undone; the texts as they are after the question
    forgetUndo();
    m_refreshTimer.stop();
    refreshChanged();
    const QHash<QString, editor::BookEditor*> openNow = openEditors();
    const Query query = *m_query;
    Step step;
    int changed = 0;
    const std::vector<QString> dirty = projects.getDirtyElements();
    for (const TextMatches& text : m_found) {
        if (editor::BookEditor* editor = openNow.value(text.elementId, nullptr)) {
            // An open text: one step of its undo history, saved with the rest of it (the book
            // keeps it as a version first, as before Replace All of the find bar)
            const int count = editor->replaceAllInText(query.text, replacement, query.options);
            if (count > 0) {
                step.editors.append(editor);
                changed += count;
            }
            continue;
        }
        // A text not open: its file, unless it has other changes not saved yet
        const QString kml = projects.chapterText(text.elementId);
        int count = 0;
        const QString replaced =
            editor::replacedInKml(kml, query.text, replacement, query.options, &count);
        if (count == 0) {
            continue;
        }
        keepVersionBeforeReplaceAll(m_book, text.elementId, kml, query.text, replacement, count);
        const bool clean = std::find(dirty.cbegin(), dirty.cend(), text.elementId) == dirty.cend();
        projects.setChapterContent(text.elementId, replaced);
        const bool saved = clean && projects.saveChapterContent(text.elementId);
        if (clean && !saved) {
            core::Logger::getInstance().warn(
                "BookSearchCoordinator: the text of {} with its matches replaced could not be "
                "saved",
                text.elementId.toStdString());
        }
        step.savedTexts.append({text.elementId, kml, saved});
        changed += count;
    }
    m_undo = step;
    // Changed again or closed, a text cannot go back to the text before
    for (editor::BookEditor* editor : std::as_const(step.editors)) {
        m_undoWatch.append(connect(editor, &editor::BookEditor::contentChanged, this,
                                   &BookSearchCoordinator::forgetUndo));
        m_undoWatch.append(
            connect(editor, &QObject::destroyed, this, &BookSearchCoordinator::forgetUndo));
    }

    //: In Polish: Zamieniono %n trafienie w książce. / Zamieniono %n trafienia w książce. /
    //: Zamieniono %n trafień w książce.
    const QString message = tr("%n matches replaced in the book.", nullptr, changed);
    EditorPanel* front = m_centralTabs != nullptr
                             ? qobject_cast<EditorPanel*>(DocumentWindows::of(*m_centralTabs).frontPage())
                             : nullptr;
    if (m_showNotice && front != nullptr) {
        //: In Polish: &Cofnij / Cofnij zamianę w książce
        m_showNotice(front, message, tr("&Undo"), tr("Undo the replacement in the book"),
                     [this]() { undo(); });
    } else {
        say(message);
    }

    // The matches as they are now
    search();
    return changed;
}

bool BookSearchCoordinator::canUndo() const {
    if (!m_undo || m_book.isReadOnly()) {
        return false;
    }
    // Every open text it changed is still open, and no text it changed in its file is open now
    const QHash<QString, editor::BookEditor*> open = openEditors();
    const QList<editor::BookEditor*> openNow = open.values();
    const auto& editors = m_undo->editors;
    if (std::any_of(editors.cbegin(), editors.cend(), [&openNow](editor::BookEditor* editor) {
            return !openNow.contains(editor);
        })) {
        return false;
    }
    const auto& saved = m_undo->savedTexts;
    return std::none_of(saved.cbegin(), saved.cend(),
                        [&open](const SavedText& text) { return open.contains(text.elementId); });
}

bool BookSearchCoordinator::undo() {
    if (!m_undo || !canUndo()) {
        forgetUndo();
        //: In Polish: Tego nie da się już cofnąć: teksty zmieniły się od tamtej pory.
        say(tr("This can no longer be undone: the texts changed since."));
        return false;
    }
    const Step step = *m_undo;
    forgetUndo();
    for (editor::BookEditor* editor : step.editors) {
        editor->undo();
    }
    auto& projects = m_book;
    for (const SavedText& text : step.savedTexts) {
        projects.setChapterContent(text.elementId, text.kml);
        if (text.saved) {
            projects.saveChapterContent(text.elementId);
        }
    }
    //: In Polish: Teksty są takie jak przed zamianą.
    say(tr("The texts are as they were before the replacement."));
    if (m_query) {
        search();
    }
    return true;
}

void BookSearchCoordinator::forgetUndo() {
    for (const QMetaObject::Connection& connection : std::as_const(m_undoWatch)) {
        disconnect(connection);
    }
    m_undoWatch.clear();
    m_undo.reset();
}

// =============================================================================
// The rest
// =============================================================================

void BookSearchCoordinator::updateMarkColor() {
    if (m_panel == nullptr) {
        return;
    }
    const editor::EditorColors defaults;
    m_panel->setMarkColor(editor::underTextColor(
        core::ThemeManager::getInstance().editorColor(editor::SEARCH_MATCH_COLOR_KEY,
                                                      defaults.searchMatch),
        editor::SEARCH_MATCH_OPACITY));
}

void BookSearchCoordinator::reset() {
    forgetUndo();
    m_sliceTimer.stop();
    m_refreshTimer.stop();
    m_queue.clear();
    m_next = 0;
    m_found.clear();
    m_read.clear();
    m_changed.clear();
    m_query.reset();
    if (m_panel != nullptr && !m_panel->searchText().isEmpty() &&
        m_book.isProjectOpen()) {
        search();  // the new book
    } else {
        publish();
    }
}

void BookSearchCoordinator::say(const QString& message) const {
    if (m_statusBar != nullptr && !message.isEmpty()) {
        m_statusBar->showMessage(message, MESSAGE_MS);
    }
}

}  // namespace kalahari::gui
