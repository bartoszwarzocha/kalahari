// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file footnotes_panel_coordinator.h
/// @brief The Footnotes panel and the texts: what it lists, and what its keys and its menu do

#ifndef KALAHARI_GUI_FOOTNOTES_PANEL_COORDINATOR_H
#define KALAHARI_GUI_FOOTNOTES_PANEL_COORDINATOR_H

#include "kalahari/editor/text_footnotes.h"
#include "kalahari/gui/panels/footnotes_panel.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>
#include <vector>

class QDockWidget;
class QTabWidget;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::editor {
class BookEditor;
}  // namespace kalahari::editor

namespace kalahari::gui {

class EditorPanel;
class FootnoteCoordinator;

/// @brief Gives the Footnotes panel the notes of the text in front or of the book, and does
///        what the writer asks for in it
///
/// The text in front gives its notes as its editor has them; the scope Book gives those of
/// every text of the book (of a series: of the volume of the text in front) in reading order,
/// the open texts as their editors have them and the others as saved, numbered as in their
/// texts. The list is made again shortly after a text changes, when another text comes to the
/// front, when the book changes and when the panel comes into view after a change.
///
/// A note selected goes to its number in the text in front. Edit opens the note's frame in its
/// text (the text opens first when it is closed; in a text only read, the cursor goes after the
/// number); Convert and Delete change the text, its tab in front, as one undo step: Ctrl+Z
/// there, or Undo in the bar under the text after a deletion, brings the note back.
class FootnotesPanelCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Opens a text of the book in its tab, or brings its tab to the front
    using ChapterOpener = std::function<void(const QString& elementId)>;

    /// @brief Says something under the text of a tab, with a button
    ///        (DocumentCoordinator::showNotice())
    using NoticeShower = std::function<void(EditorPanel* editor, const QString& message,
                                            const QString& actionText,
                                            const QString& actionToolTip,
                                            std::function<void()> action)>;

    /// @param book The book of its window
    /// @param footnotes The numbers of the notes in the book and the frame of a note
    /// @param panel The Footnotes panel
    /// @param dock Its dock (the list is made while it can be seen); may be null
    /// @param centralTabs The tabs of the book window
    /// @param openChapter Opens a text of the book
    /// @param parent Parent object
    FootnotesPanelCoordinator(core::ProjectManager& book, FootnoteCoordinator& footnotes,
                              FootnotesPanel* panel, QDockWidget* dock, QTabWidget* centralTabs,
                              ChapterOpener openChapter, QObject* parent = nullptr);

    FootnotesPanelCoordinator(const FootnotesPanelCoordinator&) = delete;
    FootnotesPanelCoordinator& operator=(const FootnotesPanelCoordinator&) = delete;
    FootnotesPanelCoordinator(FootnotesPanelCoordinator&&) = delete;
    FootnotesPanelCoordinator& operator=(FootnotesPanelCoordinator&&) = delete;
    ~FootnotesPanelCoordinator() override = default;

    /// @brief Where the notice of a note deleted shows, with Undo
    void setNoticeShower(NoticeShower shower) { m_showNotice = std::move(shower); }

    /// @brief Give the panel the notes now
    void refresh();

    /// @brief Give the panel the notes shortly (after a change of a text)
    void refreshSoon();

    /// @brief The notes of the scope of the panel, as it is given them
    [[nodiscard]] std::vector<FootnoteEntry> entries() const;

    /// @brief What the panel asks for: go to a note's number in the text in front
    bool goToNote(const FootnoteEntry& entry);
    /// @brief Open the frame of a note (its text opened first)
    bool editNote(const FootnoteEntry& entry);
    /// @brief Make a footnote an endnote, or an endnote a footnote
    bool convertNote(const FootnoteEntry& entry);
    /// @brief Delete a note; the bar under its text says so, with Undo
    bool deleteNote(const FootnoteEntry& entry);

private:
    /// @brief The notes of a saved text with the words before them, kept until it changes
    struct SavedNotes {
        QString signature;
        std::vector<editor::NoteInText> notes;
    };

    [[nodiscard]] EditorPanel* frontPanel() const;
    [[nodiscard]] editor::BookEditor* frontEditor() const;
    /// @brief The tab of an open text (empty: the text in front, outside the book)
    [[nodiscard]] EditorPanel* openPanel(const QString& elementId) const;
    /// @brief The editor of a note's text, its tab in front, the text opened when it is closed
    [[nodiscard]] editor::BookEditor* editorFor(const FootnoteEntry& entry);
    /// @brief The notes of an editor, as the panel lists them
    [[nodiscard]] std::vector<FootnoteEntry> entriesOf(const editor::BookEditor& editor,
                                                       const QString& elementId,
                                                       const QString& title, int order) const;
    /// @brief The notes of the book (of the volume of the text in front)
    [[nodiscard]] std::vector<FootnoteEntry> bookEntries() const;
    /// @brief The title of the text of a tab
    [[nodiscard]] QString titleOf(const EditorPanel* panel) const;
    /// @brief Connect to the editor in front (its notes changing)
    void attachFront();

    core::ProjectManager& m_book;
    FootnoteCoordinator& m_footnotes;
    FootnotesPanel* m_panel;
    QDockWidget* m_dock;
    QTabWidget* m_centralTabs;
    ChapterOpener m_openChapter;
    NoticeShower m_showNotice;
    QTimer m_refreshTimer;
    mutable QHash<QString, SavedNotes> m_saved;
    bool m_stale = true;  ///< Changed while the panel could not be seen
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_FOOTNOTES_PANEL_COORDINATOR_H
