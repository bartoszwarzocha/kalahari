// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file figures_panel_coordinator.cpp
/// @brief The panel Images and Tables and the texts

#include "kalahari/gui/figures_panel_coordinator.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/kind_words.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/text_places.h"
#include "kalahari/gui/book_texts.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/editor_panel.h"

#include <QDockWidget>
#include <QTabWidget>
#include <QTextDocument>

#include <optional>
#include <utility>

namespace kalahari::gui {

namespace {

/// @brief After a change of a text, the list is made again once the writing pauses
constexpr int REFRESH_DELAY_MS = 400;

/// @brief The kind of Edit > Go To of a figure
editor::PlaceKind placeKindOf(const FigureEntry& entry) {
    return entry.figure.kind == editor::FigureKind::Table ? editor::PlaceKind::Table
                                                          : editor::PlaceKind::Image;
}

/// @brief The command of the menus a figure's action runs
std::string commandOf(const FigureEntry& entry, FigureAction action) {
    const bool table = entry.figure.kind == editor::FigureKind::Table;
    if (action == FigureAction::Properties) {
        return table ? "table.properties" : "image.properties";
    }
    return table ? "table.caption" : "image.addCaption";
}

}  // namespace

FiguresPanelCoordinator::FiguresPanelCoordinator(core::ProjectManager& book,
                                                 CommandRegistry& commands, FiguresPanel* panel,
                                                 QDockWidget* dock, QTabWidget* centralTabs,
                                                 ChapterOpener openChapter,
                                                 CaptionContextOf contextOf, QObject* parent)
    : QObject(parent)
    , m_book(book)
    , m_commands(commands)
    , m_panel(panel)
    , m_dock(dock)
    , m_centralTabs(centralTabs)
    , m_openChapter(std::move(openChapter))
    , m_contextOf(std::move(contextOf))
{
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(REFRESH_DELAY_MS);
    connect(&m_refreshTimer, &QTimer::timeout, this, &FiguresPanelCoordinator::refresh);

    connect(&DocumentWindows::of(*m_centralTabs), &DocumentWindows::frontChanged, this, [this]() {
        attachFront();
        refreshSoon();
    });
    const auto bookChanged = [this]() {
        m_saved.clear();
        m_images.setFolder(m_book.isProjectOpen() ? m_book.imagesBaseFolder() : QString());
        refreshSoon();
    };
    connect(&m_book, &core::ProjectManager::projectOpened, this, bookChanged);
    connect(&m_book, &core::ProjectManager::projectClosed, this, bookChanged);
    // The titles and the order of the texts, the numbering of the book
    connect(&m_book, &core::ProjectManager::projectChanged, this,
            &FiguresPanelCoordinator::refreshSoon);

    m_panel->setPictureSource([this](const FigureEntry& entry, const QSize& pixels) {
        return pictureOf(entry, pixels);
    });
    connect(m_panel, &FiguresPanel::entryActivated, this,
            [this](const FigureEntry& entry) { goToFigure(entry); });
    connect(m_panel, &FiguresPanel::openRequested, this,
            [this](const FigureEntry& entry) { openFigure(entry); });
    connect(m_panel, &FiguresPanel::actionRequested, this,
            [this](const FigureEntry& entry, FigureAction action) { runAction(entry, action); });
    connect(m_panel, &FiguresPanel::editorFocusRequested, this, [this]() {
        if (editor::BookEditor* editor = frontEditor()) {
            editor->setFocus(Qt::OtherFocusReason);
        }
    });
    connect(m_panel, &FiguresPanel::scopeChanged, this, &FiguresPanelCoordinator::refresh);

    // The list is made while the panel can be seen; one made old meanwhile is made again
    // when it comes into view
    if (m_dock != nullptr) {
        connect(m_dock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
            if (visible && m_stale) {
                refresh();
            }
        });
    }
    if (m_book.isProjectOpen()) {
        m_images.setFolder(m_book.imagesBaseFolder());
    }
    attachFront();
    refresh();
}

void FiguresPanelCoordinator::refreshSoon()
{
    m_stale = true;
    m_refreshTimer.start();
}

void FiguresPanelCoordinator::attachFront()
{
    if (editor::BookEditor* editor = frontEditor()) {
        // A figure added, changed or deleted, and the numbers of its captions
        connect(editor, &editor::BookEditor::contentChanged, this,
                &FiguresPanelCoordinator::refreshSoon, Qt::UniqueConnection);
        connect(editor, &editor::BookEditor::captionsChanged, this,
                &FiguresPanelCoordinator::refreshSoon, Qt::UniqueConnection);
    }
}

void FiguresPanelCoordinator::refresh()
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
    m_panel->setDocumentState(editor != nullptr);
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
    const bool book = m_panel->scope() == FigureScope::Book && bookOpen;
    m_panel->setReadOnly(book ? m_book.isReadOnly() : editor != nullptr && editor->isReadOnly());
    m_panel->setEntries(entries());
}

std::vector<FigureEntry> FiguresPanelCoordinator::entries() const
{
    if (m_panel->scope() == FigureScope::Book && m_book.isProjectOpen()) {
        return bookEntries();
    }
    const EditorPanel* panel = frontPanel();
    const editor::BookEditor* editor = frontEditor();
    if (editor == nullptr) {
        return {};
    }
    return entriesOf(*editor, panel->property("elementId").toString(), titleOf(panel), 0);
}

std::vector<FigureEntry> FiguresPanelCoordinator::entriesOf(const editor::BookEditor& editor,
                                                            const QString& elementId,
                                                            const QString& title,
                                                            int order) const
{
    std::vector<FigureEntry> entries;
    const QTextDocument* document = editor.textDocument();
    if (document == nullptr) {
        return entries;
    }
    // A text of the book: its numbers after the captions of the texts before it, as its editor
    // gets them shortly after it opens (CaptionCoordinator::refresh()), not before
    const bool inBook =
        !elementId.isEmpty() && m_contextOf && m_book.findElement(elementId) != nullptr;
    const editor::CaptionNumbers numbers =
        inBook ? editor::CaptionNumbers(*document, m_contextOf(elementId)) : editor.captionNumbers();
    for (editor::FigureInText& figure : editor::figuresIn(*document, numbers)) {
        FigureEntry entry;
        entry.elementId = elementId;
        entry.textTitle = title;
        entry.textOrder = order;
        entry.figure = std::move(figure);
        entries.push_back(std::move(entry));
    }
    return entries;
}

std::vector<FigureEntry> FiguresPanelCoordinator::bookEntries() const
{
    std::vector<FigureEntry> entries;
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
            std::vector<FigureEntry> open =
                entriesOf(*panel->getBookEditor(), elementId, title, order);
            entries.insert(entries.end(), open.begin(), open.end());
        } else {
            SavedFigures& saved = m_saved[elementId];
            std::optional<QString> kml = savedTextIfChanged(m_book, elementId, saved.signature);
            // The numbers of its captions follow the texts before it: read again when they
            // changed, though the text did not
            const bool numbered =
                kml ? kml->contains(QLatin1String("<caption-number")) ||
                          kml->contains(QLatin1String("<cross-reference"))
                    : saved.numbered;
            editor::CaptionContext context;
            if (numbered && m_contextOf) {
                context = m_contextOf(elementId);
            }
            if (!kml && numbered && context != saved.context) {
                saved.signature.clear();
                kml = savedTextIfChanged(m_book, elementId, saved.signature);
            }
            if (kml) {
                saved.numbered = numbered;
                saved.context = context;
                saved.figures = editor::figuresInKml(*kml, context);
            }
            for (const editor::FigureInText& figure : saved.figures) {
                FigureEntry entry;
                entry.elementId = elementId;
                entry.textTitle = title;
                entry.textOrder = order;
                entry.figure = figure;
                entries.push_back(std::move(entry));
            }
        }
        ++order;
    }
    return entries;
}

// =============================================================================
// The texts
// =============================================================================

EditorPanel* FiguresPanelCoordinator::frontPanel() const
{
    return qobject_cast<EditorPanel*>(DocumentWindows::of(*m_centralTabs).frontPage());
}

editor::BookEditor* FiguresPanelCoordinator::frontEditor() const
{
    EditorPanel* panel = frontPanel();
    return panel != nullptr ? panel->getBookEditor() : nullptr;
}

EditorPanel* FiguresPanelCoordinator::openPanel(const QString& elementId) const
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

editor::BookEditor* FiguresPanelCoordinator::editorFor(const FigureEntry& entry)
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

QString FiguresPanelCoordinator::titleOf(const EditorPanel* panel) const
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

bool FiguresPanelCoordinator::goToFigure(const FigureEntry& entry)
{
    // Only in the text in front: the arrows go over the figures of the book without opening
    // their texts
    editor::BookEditor* editor = frontEditor();
    if (editor == nullptr || openPanel(entry.elementId) != frontPanel()) {
        return false;
    }
    return editor->goToPlace(placeKindOf(entry), QString::number(entry.figure.ordinal)).result ==
           editor::GoToResult::Done;
}

bool FiguresPanelCoordinator::openFigure(const FigureEntry& entry)
{
    editor::BookEditor* editor = editorFor(entry);
    if (editor == nullptr) {
        return false;
    }
    editor->setFocus(Qt::OtherFocusReason);
    return editor->goToPlace(placeKindOf(entry), QString::number(entry.figure.ordinal)).result ==
           editor::GoToResult::Done;
}

bool FiguresPanelCoordinator::runAction(const FigureEntry& entry, FigureAction action)
{
    if (!openFigure(entry)) {
        return false;
    }
    const editor::BookEditor* editor = frontEditor();
    if (editor == nullptr || editor->isReadOnly()) {
        return false;
    }
    // The command of the menus, on the figure just selected in the text with the keys
    return m_commands.executeCommand(commandOf(entry, action)) == CommandExecutionResult::Success;
}

QImage FiguresPanelCoordinator::pictureOf(const FigureEntry& entry, const QSize& pixels)
{
    if (entry.figure.kind != editor::FigureKind::Image) {
        return {};
    }
    // An open text shows its images from its own files (a document outside the book too)
    if (const EditorPanel* panel = openPanel(entry.elementId);
        panel != nullptr && panel->getBookEditor() != nullptr &&
        panel->getBookEditor()->imageStore()) {
        return panel->getBookEditor()->imageStore()->shownImage(entry.figure.image, pixels);
    }
    return m_images.hasFolder() ? m_images.shownImage(entry.figure.image, pixels) : QImage();
}

}  // namespace kalahari::gui
