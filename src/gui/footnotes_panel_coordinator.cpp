// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file footnotes_panel_coordinator.cpp
/// @brief The Footnotes panel and the texts

#include "kalahari/gui/footnotes_panel_coordinator.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/kind_words.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/book_texts.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/footnote_coordinator.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/editor_panel.h"

#include <QDockWidget>
#include <QPointer>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextDocument>

#include <algorithm>

namespace kalahari::gui {

namespace {

/// @brief After a change of a text, the list is made again once the writing pauses
constexpr int REFRESH_DELAY_MS = 400;

/// @brief The other kind
editor::NoteKind otherKind(editor::NoteKind kind) {
    return kind == editor::NoteKind::Endnote ? editor::NoteKind::Footnote
                                             : editor::NoteKind::Endnote;
}

/// @brief The numbers of the notes of a text not open, as the text numbers them
std::vector<QString> numbersOf(const std::vector<editor::NoteInText>& notes,
                               const editor::FootnoteContext& context) {
    std::vector<QString> numbers;
    numbers.reserve(notes.size());
    int footnotes = context.footnotesBefore;
    int endnotes = context.endnotesBefore;
    for (const editor::NoteInText& note : notes) {
        numbers.push_back(note.note.kind == editor::NoteKind::Endnote
                              ? editor::noteNumberText(++endnotes, context.endnoteFormat)
                              : editor::noteNumberText(++footnotes, context.footnoteFormat));
    }
    return numbers;
}

}  // namespace

FootnotesPanelCoordinator::FootnotesPanelCoordinator(core::ProjectManager& book,
                                                     FootnoteCoordinator& footnotes,
                                                     FootnotesPanel* panel, QDockWidget* dock,
                                                     QTabWidget* centralTabs,
                                                     ChapterOpener openChapter, QObject* parent)
    : QObject(parent)
    , m_book(book)
    , m_footnotes(footnotes)
    , m_panel(panel)
    , m_dock(dock)
    , m_centralTabs(centralTabs)
    , m_openChapter(std::move(openChapter))
{
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(REFRESH_DELAY_MS);
    connect(&m_refreshTimer, &QTimer::timeout, this, &FootnotesPanelCoordinator::refresh);

    connect(&DocumentWindows::of(*m_centralTabs), &DocumentWindows::frontChanged, this, [this]() {
        attachFront();
        refreshSoon();
    });
    const auto bookChanged = [this]() {
        m_saved.clear();
        refreshSoon();
    };
    connect(&m_book, &core::ProjectManager::projectOpened, this, bookChanged);
    connect(&m_book, &core::ProjectManager::projectClosed, this, bookChanged);
    // The titles and the order of the texts, the numbering of the book
    connect(&m_book, &core::ProjectManager::projectChanged, this,
            &FootnotesPanelCoordinator::refreshSoon);
    connect(&m_book, &core::ProjectManager::bookFootnotesChanged, this,
            &FootnotesPanelCoordinator::refreshSoon);

    connect(m_panel, &FootnotesPanel::entryActivated, this,
            [this](const FootnoteEntry& entry) { goToNote(entry); });
    connect(m_panel, &FootnotesPanel::editRequested, this,
            [this](const FootnoteEntry& entry) { editNote(entry); });
    connect(m_panel, &FootnotesPanel::convertRequested, this,
            [this](const FootnoteEntry& entry) { convertNote(entry); });
    connect(m_panel, &FootnotesPanel::deleteRequested, this,
            [this](const FootnoteEntry& entry) { deleteNote(entry); });
    connect(m_panel, &FootnotesPanel::editorFocusRequested, this, [this]() {
        if (editor::BookEditor* editor = frontEditor()) {
            editor->setFocus(Qt::OtherFocusReason);
        }
    });
    connect(m_panel, &FootnotesPanel::scopeChanged, this, &FootnotesPanelCoordinator::refresh);

    // The list is made while the panel can be seen; one made old meanwhile is made again
    // when it comes into view
    if (m_dock != nullptr) {
        connect(m_dock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
            if (visible && m_stale) {
                refresh();
            }
        });
    }
    attachFront();
    refresh();
}

void FootnotesPanelCoordinator::refreshSoon()
{
    m_stale = true;
    m_refreshTimer.start();
}

void FootnotesPanelCoordinator::attachFront()
{
    if (editor::BookEditor* editor = frontEditor()) {
        // A note added, changed or deleted, and the words before the numbers
        connect(editor, &editor::BookEditor::notesChanged, this,
                &FootnotesPanelCoordinator::refreshSoon, Qt::UniqueConnection);
        connect(editor, &editor::BookEditor::contentChanged, this,
                &FootnotesPanelCoordinator::refreshSoon, Qt::UniqueConnection);
    }
}

void FootnotesPanelCoordinator::refresh()
{
    m_refreshTimer.stop();
    const bool bookOpen = m_book.isProjectOpen();
    m_panel->setBookScopeAvailable(bookOpen);
    if (m_dock != nullptr && !m_dock->isVisible()) {
        // Closed, or behind another tab: made when it comes into view
        m_stale = true;
        return;
    }
    m_stale = false;

    const EditorPanel* panel = frontPanel();
    const editor::BookEditor* editor = frontEditor();
    m_panel->setDocumentState(editor != nullptr, editor == nullptr || editor->annotationsAllowed());
    // The name of the text in front on the button of the scope ("Chapter", "Story")
    QString name;
    if (const core::ProjectElement* element =
            panel != nullptr && bookOpen
                ? m_book.findElement(panel->property("elementId").toString())
                : nullptr) {
        name = core::fillWords(QStringLiteral("{Kind}"),
                               {{QStringLiteral("kind"), wordsOf(m_book, *element)}});
    }
    m_panel->setTextName(name);
    const bool book = m_panel->scope() == FootnoteScope::Book && bookOpen;
    m_panel->setReadOnly(book ? m_book.isReadOnly() : editor != nullptr && editor->isReadOnly());
    m_panel->setEntries(entries());
}

std::vector<FootnoteEntry> FootnotesPanelCoordinator::entries() const
{
    if (m_panel->scope() == FootnoteScope::Book && m_book.isProjectOpen()) {
        return bookEntries();
    }
    const EditorPanel* panel = frontPanel();
    const editor::BookEditor* editor = frontEditor();
    if (editor == nullptr) {
        return {};
    }
    return entriesOf(*editor, panel->property("elementId").toString(), titleOf(panel), 0);
}

std::vector<FootnoteEntry> FootnotesPanelCoordinator::entriesOf(const editor::BookEditor& editor,
                                                                const QString& elementId,
                                                                const QString& title,
                                                                int order) const
{
    std::vector<FootnoteEntry> entries;
    const QTextDocument* document = editor.textDocument();
    if (document == nullptr) {
        return entries;
    }
    for (const editor::NotePlace& place : editor::notesIn(*document)) {
        FootnoteEntry entry;
        entry.elementId = elementId;
        entry.textTitle = title;
        entry.textOrder = order;
        entry.id = place.note.id;
        entry.kind = place.note.kind;
        entry.number = editor.noteNumber(place.note.id);
        entry.text = editor::notePlainText(place.note.content).simplified();
        const QTextBlock block = document->findBlock(place.position);
        entry.before = editor::lastWords(block.text().left(place.position - block.position()),
                                         FootnotesPanel::BEFORE_WORDS);
        entries.push_back(std::move(entry));
    }
    return entries;
}

std::vector<FootnoteEntry> FootnotesPanelCoordinator::bookEntries() const
{
    std::vector<FootnoteEntry> entries;
    const EditorPanel* front = frontPanel();
    const core::ProjectBook* volume =
        front != nullptr ? volumeOfText(m_book, front->property("elementId").toString())
                         : nullptr;
    int order = 0;
    for (const BookTextElement& text : bookTextElements(m_book, volume)) {
        const QString elementId = text.element->id;
        const QString title = text.element->title;
        if (const EditorPanel* panel = openPanel(elementId);
            panel != nullptr && panel->getBookEditor() != nullptr) {
            std::vector<FootnoteEntry> open =
                entriesOf(*panel->getBookEditor(), elementId, title, order);
            entries.insert(entries.end(), open.begin(), open.end());
        } else {
            SavedNotes& saved = m_saved[elementId];
            if (const std::optional<QString> kml =
                    savedTextIfChanged(m_book, elementId, saved.signature)) {
                saved.notes = editor::notesInKml(*kml, FootnotesPanel::BEFORE_WORDS);
            }
            if (!saved.notes.empty()) {
                const std::vector<QString> numbers =
                    numbersOf(saved.notes, m_footnotes.contextFor(elementId));
                for (size_t i = 0; i < saved.notes.size(); ++i) {
                    const editor::NoteInText& note = saved.notes.at(i);
                    FootnoteEntry entry;
                    entry.elementId = elementId;
                    entry.textTitle = title;
                    entry.textOrder = order;
                    entry.id = note.note.id;
                    entry.kind = note.note.kind;
                    entry.number = numbers.at(i);
                    entry.text = editor::notePlainText(note.note.content).simplified();
                    entry.before = note.before;
                    entries.push_back(std::move(entry));
                }
            }
        }
        ++order;
    }
    return entries;
}

// =============================================================================
// The texts
// =============================================================================

EditorPanel* FootnotesPanelCoordinator::frontPanel() const
{
    return qobject_cast<EditorPanel*>(DocumentWindows::of(*m_centralTabs).frontPage());
}

editor::BookEditor* FootnotesPanelCoordinator::frontEditor() const
{
    EditorPanel* panel = frontPanel();
    return panel != nullptr ? panel->getBookEditor() : nullptr;
}

EditorPanel* FootnotesPanelCoordinator::openPanel(const QString& elementId) const
{
    if (elementId.isEmpty()) {
        return frontPanel();
    }
    for (QWidget* page : DocumentWindows::of(*m_centralTabs).pages()) {
        auto* panel = qobject_cast<EditorPanel*>(page);
        if (panel != nullptr && panel->property("elementId").toString() == elementId) {
            return panel;
        }
    }
    return nullptr;
}

editor::BookEditor* FootnotesPanelCoordinator::editorFor(const FootnoteEntry& entry)
{
    if (EditorPanel* panel = openPanel(entry.elementId)) {
        // Its tab in front: what is done there is seen, and Ctrl+Z undoes it there
        DocumentWindows::of(*m_centralTabs).showPage(panel);
        return panel->getBookEditor();
    }
    if (entry.elementId.isEmpty() || !m_openChapter) {
        return nullptr;
    }
    m_openChapter(entry.elementId);
    EditorPanel* panel = openPanel(entry.elementId);
    return panel != nullptr ? panel->getBookEditor() : nullptr;
}

QString FootnotesPanelCoordinator::titleOf(const EditorPanel* panel) const
{
    const QString elementId = panel != nullptr ? panel->property("elementId").toString() : QString();
    if (const core::ProjectElement* element =
            elementId.isEmpty() ? nullptr : m_book.findElement(elementId)) {
        return element->title;
    }
    QString title = panel != nullptr ? DocumentWindows::tabText(panel) : QString();
    if (title.startsWith(QLatin1Char('*'))) {
        title.remove(0, 1);  // without the mark of changes
    }
    return title;
}

// =============================================================================
// What the panel asks for
// =============================================================================

bool FootnotesPanelCoordinator::goToNote(const FootnoteEntry& entry)
{
    // Only in the text in front: the arrows go over the notes of the book without opening
    // their texts
    editor::BookEditor* editor = frontEditor();
    if (editor == nullptr || openPanel(entry.elementId) != frontPanel()) {
        return false;
    }
    return editor->goToNote(entry.id);
}

bool FootnotesPanelCoordinator::editNote(const FootnoteEntry& entry)
{
    editor::BookEditor* editor = editorFor(entry);
    if (editor == nullptr) {
        return false;
    }
    if (editor->isReadOnly()) {
        editor->setFocus(Qt::OtherFocusReason);
        return editor->goToNote(entry.id);
    }
    return m_footnotes.editNote(*editor, entry.id);
}

bool FootnotesPanelCoordinator::convertNote(const FootnoteEntry& entry)
{
    editor::BookEditor* editor = editorFor(entry);
    if (editor == nullptr || editor->isReadOnly() ||
        !editor->setNoteKind(entry.id, otherKind(entry.kind))) {
        return false;
    }
    refresh();
    m_panel->focusList(entry.key());
    return true;
}

bool FootnotesPanelCoordinator::deleteNote(const FootnoteEntry& entry)
{
    // The card after it, else the one before it, is selected next
    const std::vector<FootnoteEntry> shown = m_panel->shownEntries();
    const auto at = std::find_if(shown.cbegin(), shown.cend(), [&entry](const FootnoteEntry& e) {
        return e.key() == entry.key();
    });
    QString next;
    if (at != shown.cend() && std::next(at) != shown.cend()) {
        next = std::next(at)->key();
    } else if (at != shown.cend() && at != shown.cbegin()) {
        next = std::prev(at)->key();
    }

    editor::BookEditor* editor = editorFor(entry);
    if (editor == nullptr || editor->isReadOnly()) {
        return false;
    }
    const QString title = editor::BookEditor::noteTitle(entry.kind, editor->noteNumber(entry.id));
    if (!editor->removeNote(entry.id)) {
        return false;
    }
    refresh();
    m_panel->focusList(next);
    if (m_showNotice) {
        const QPointer<editor::BookEditor> text(editor);
        const int steps = editor->textDocument()->availableUndoSteps();
        //: The bar under the text after a note was deleted in the Footnotes panel; %1: "Footnote
        //: 3". In Polish: Usunięto: %1.
        m_showNotice(openPanel(entry.elementId), tr("Deleted: %1.").arg(title),
                     //: In Polish: Cofn&ij
                     tr("&Undo"),
                     //: In Polish: Przywróć przypis
                     tr("Bring the note back"), [this, text, steps, key = entry.key()]() {
                         // Only while its deletion is the last step of the text
                         if (text && text->textDocument()->availableUndoSteps() == steps) {
                             text->undo();
                             refresh();
                             m_panel->selectEntry(key);
                         }
                     });
    }
    return true;
}

}  // namespace kalahari::gui
