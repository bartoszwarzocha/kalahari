// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file figures_panel_coordinator.h
/// @brief The panel Images and Tables and the texts: what it lists, and what its keys and its
///        menu do

#ifndef KALAHARI_GUI_FIGURES_PANEL_COORDINATOR_H
#define KALAHARI_GUI_FIGURES_PANEL_COORDINATOR_H

#include "kalahari/editor/image_store.h"
#include "kalahari/editor/text_captions.h"
#include "kalahari/editor/text_figures.h"
#include "kalahari/gui/panels/figures_panel.h"

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

class CommandRegistry;
class EditorPanel;

/// @brief Gives the panel Images and Tables the figures of the text in front or of the book,
///        and does what the writer asks for in it
///
/// The text in front gives its images and tables as its editor has them; the scope Book gives
/// those of every text of the book (of a series: of the volume of the text in front) in
/// reading order, the open texts as their editors have them and the others as saved, their
/// captions numbered as in their texts. The list is made again shortly after a text changes,
/// when another text comes to the front, when the book changes and when the panel comes into
/// view after a change.
///
/// A figure selected goes to it in the text in front (the image selected, the cursor in the
/// table's first cell). Go To opens its text when it is closed, brings its tab to the front
/// and gives it the keys; Properties and Insert Caption go there too and then run Image
/// Properties... or Table Properties... and Insert Caption... of the menus.
class FiguresPanelCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Opens a text of the book in its tab, or brings its tab to the front
    using ChapterOpener = std::function<void(const QString& elementId)>;

    /// @brief The context of the captions of a text of the book (CaptionCoordinator::contextFor())
    using CaptionContextOf = std::function<editor::CaptionContext(const QString& elementId)>;

    /// @param book The book of its window
    /// @param commands The commands of its window (Image Properties..., Insert Caption...)
    /// @param panel The panel Images and Tables
    /// @param dock Its dock (the list is made while it can be seen); may be null
    /// @param centralTabs The tabs of the book window
    /// @param openChapter Opens a text of the book
    /// @param contextOf The context of the captions of a closed text
    /// @param parent Parent object
    FiguresPanelCoordinator(core::ProjectManager& book, CommandRegistry& commands,
                            FiguresPanel* panel, QDockWidget* dock, QTabWidget* centralTabs,
                            ChapterOpener openChapter, CaptionContextOf contextOf,
                            QObject* parent = nullptr);

    FiguresPanelCoordinator(const FiguresPanelCoordinator&) = delete;
    FiguresPanelCoordinator& operator=(const FiguresPanelCoordinator&) = delete;
    FiguresPanelCoordinator(FiguresPanelCoordinator&&) = delete;
    FiguresPanelCoordinator& operator=(FiguresPanelCoordinator&&) = delete;
    ~FiguresPanelCoordinator() override = default;

    /// @brief Give the panel the figures now
    void refresh();

    /// @brief Give the panel the figures shortly (after a change of a text)
    void refreshSoon();

    /// @brief The figures of the scope of the panel, as it is given them
    [[nodiscard]] std::vector<FigureEntry> entries() const;

    /// @brief What the panel asks for: go to a figure in the text in front
    bool goToFigure(const FigureEntry& entry);
    /// @brief Go to a figure in its text (opened first), with the keys there
    bool openFigure(const FigureEntry& entry);
    /// @brief Go to a figure in its text and run its Properties or Insert Caption there
    bool runAction(const FigureEntry& entry, FigureAction action);

    /// @brief The picture of an image as it is shown, in these pixels (null: its file missing)
    [[nodiscard]] QImage pictureOf(const FigureEntry& entry, const QSize& pixels);

private:
    /// @brief The figures of a saved text, kept until it or its context changes
    struct SavedFigures {
        QString signature;
        bool numbered = false;  ///< It has numbered captions: its numbers follow the book
        editor::CaptionContext context;
        std::vector<editor::FigureInText> figures;
    };

    [[nodiscard]] EditorPanel* frontPanel() const;
    [[nodiscard]] editor::BookEditor* frontEditor() const;
    /// @brief The tab of an open text (empty: the text in front, outside the book)
    [[nodiscard]] EditorPanel* openPanel(const QString& elementId) const;
    /// @brief The editor of a figure's text, its tab in front, the text opened when it is closed
    [[nodiscard]] editor::BookEditor* editorFor(const FigureEntry& entry);
    /// @brief The figures of an editor, as the panel lists them: a text of the book numbered
    ///        in the book
    [[nodiscard]] std::vector<FigureEntry> entriesOf(const editor::BookEditor& editor,
                                                     const QString& elementId,
                                                     const QString& title, int order) const;
    /// @brief The figures of the book (of the volume of the text in front)
    [[nodiscard]] std::vector<FigureEntry> bookEntries() const;
    /// @brief The title of the text of a tab
    [[nodiscard]] QString titleOf(const EditorPanel* panel) const;
    /// @brief Connect to the editor in front (its figures changing)
    void attachFront();

    core::ProjectManager& m_book;
    CommandRegistry& m_commands;
    FiguresPanel* m_panel;
    QDockWidget* m_dock;
    QTabWidget* m_centralTabs;
    ChapterOpener m_openChapter;
    CaptionContextOf m_contextOf;
    QTimer m_refreshTimer;
    mutable QHash<QString, SavedFigures> m_saved;
    editor::ImageStore m_images;  ///< The images of the closed texts of the book
    bool m_stale = true;  ///< Changed while the panel could not be seen
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_FIGURES_PANEL_COORDINATOR_H
