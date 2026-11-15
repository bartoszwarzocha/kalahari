// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_coordinator.h
/// @brief The plot board of the book on its tab in the middle of the window, and the changes
/// of the book it asks for

#ifndef KALAHARI_GUI_PLOT_BOARD_COORDINATOR_H
#define KALAHARI_GUI_PLOT_BOARD_COORDINATOR_H

#include "kalahari/core/book_project.h"
#include "kalahari/core/chapter_document.h"

#include <QHash>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>

class QStatusBar;
class QTabWidget;
class QTimer;
class QUndoStack;
class QWidget;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::editor {
class BookEditor;
}  // namespace kalahari::editor

namespace kalahari::gui {

class NavigatorCoordinator;
class PlotBoardPanel;
struct PlotCardEdit;

/// @brief Opens the plot board (PlotBoardPanel) on a tab of its own and makes the changes of
/// the book it asks for
///
/// Book > Plot Board and Book > Outline show the one tab of the board with the board of the
/// cards or with the outline. The board follows the book: a text added, moved, renamed or
/// deleted anywhere, its state, its words; a board behind another tab is filled again when it
/// comes to the front.
///
/// What the writer changes on the board is undone with Ctrl+Z there, and done again with
/// Ctrl+Y: a card moved, a card edited, its state and its color. A change of the book made
/// elsewhere (the Navigator, the Properties panel), a text added or deleted, forgets what can
/// be undone, since the places the board remembers may be no more. Opening a text, a text
/// added or deleted and the properties are those of the Navigator (NavigatorCoordinator).
///
/// The cards of the scenes of a text show what its scene breaks say, also in a text open with
/// changes not saved; what the board changes of a scene goes to the scene break in the editor
/// of an open text (to be saved with the text), else to its file, and of the first scene to
/// the .klh file (core::ProjectManager::setScenePlot()). Enter on a scene opens its text at the
/// scene; a scene changed in the editor since is not undone on the board.
///
/// A scene moves among the scenes of its text, a new one comes after a scene or at the end of
/// a text, and one is deleted with its text or with its break only: the text changes at once,
/// in its editor when it is open (one step of its Ctrl+Z), and one Ctrl+Z on the board undoes
/// it. A scene moves to another text in the editor: its annotations and notes go with it.
class PlotBoardCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Brings the Properties panel to the front; @p keys: with the keys in its first
    /// field (Alt+Enter)
    using PropertiesShower = std::function<void(bool keys)>;

    /// @brief Constructor
    /// @param book The book of its window
    /// @param centralTabs The tabs in the middle of the window
    /// @param navigator The commands of the elements of the book
    /// @param statusBar Where the board says what needs a book
    /// @param showProperties Brings the Properties panel to the front
    /// @param parent Parent QObject
    PlotBoardCoordinator(core::ProjectManager& book, QTabWidget* centralTabs,
                         NavigatorCoordinator* navigator, QStatusBar* statusBar,
                         PropertiesShower showProperties, QObject* parent = nullptr);
    ~PlotBoardCoordinator() override;

    PlotBoardCoordinator(const PlotBoardCoordinator&) = delete;
    PlotBoardCoordinator& operator=(const PlotBoardCoordinator&) = delete;

    /// @brief Show the board on its tab, in front
    /// @param view PlotBoard::VIEW_BOARD or VIEW_OUTLINE, kept as the one shown last; empty:
    ///        the one shown last
    /// @param keys The board or the outline gets the keys
    /// @return The board; none without an open book
    PlotBoardPanel* openBoard(const QString& view, bool keys);

    /// @brief The board open on its tab; none
    [[nodiscard]] PlotBoardPanel* board() const;

    /// @brief What can be undone and done again on the board
    [[nodiscard]] QUndoStack* undoStack() const { return m_undo; }

    /// @brief The card marked to move (Ctrl+X), a text or a scene; empty: none
    [[nodiscard]] QString markedCard() const { return m_marked; }

public slots:
    /// @brief Fill the board again from the open book: at once when it is in front, else when
    /// it comes to the front
    void refresh();

private:
    /// @brief What a card shows of its text that the writer can change: its title, its
    /// description and its plot
    struct CardText {
        QString title;
        QString description;
        core::ElementPlot plot;

        bool operator==(const CardText&) const = default;
    };

    void refreshNow();
    void connectBoard(PlotBoardPanel* panel);
    void updateTab();
    void onCurrentTabChanged();
    /// @brief The book was saved: a change made elsewhere forgets what can be undone
    void onProjectChanged();
    /// @brief Another book, or none: the board forgets what it remembers
    void forget();

    void open(const QString& elementId);
    void edit(const QString& elementId, const PlotCardEdit& edit);
    void setStatus(const QString& elementId, const QString& status);
    void setColor(const QString& elementId, const QString& color);
    void move(const QString& elementId, int steps);
    void moveToText(const QString& cardId, int direction);
    void dropBeside(const QString& elementId, const QString& anchorId, bool after);
    void dropInPart(const QString& elementId, const QString& partId);
    void add(const QString& afterId);
    void addScene(const QString& cardId);
    void remove(const QString& elementId);
    void cut(const QString& elementId);
    void paste(const QString& afterId);
    void cancelCut();
    void undo();
    void redo();
    void showProperties(const QString& elementId, bool keys);

    /// @name The scenes of the texts
    /// @{
    /// @brief Give scene @p cardId (PlotBoard::sceneId()) @p plot and remember it to be undone;
    /// @p message says what was done
    void changeSceneWithUndo(const QString& cardId, const core::ScenePlot& plot,
                             const QString& message, const QString& undoText);
    /// @brief Give scene @p scene of text @p textId @p plot when it still says @p before: in
    /// the editor of the text when it is open, else in the book
    /// @return false when the scene says something else now (m_changedElsewhere), or the book
    ///         could not take the change
    bool changeScene(const QString& textId, int scene, const core::ScenePlot& before,
                     const core::ScenePlot& plot);
    /// @brief What scene @p scene of text @p textId says now, also in its open editor;
    /// nullopt: the text has no such scene
    [[nodiscard]] std::optional<core::ScenePlot> scenePlotOf(const QString& textId,
                                                             int scene) const;
    /// @brief Tell the writer that a scene moves to another text in the editor
    void sceneInEditor();
    /// @brief Move scene @p from of text @p textId to place @p to of its scenes without it
    void moveScene(const QString& textId, int from, int to);
    /// @brief Add a scene at place @p at of the scenes of text @p textId and edit its card;
    /// @p message says where it is
    void addSceneAt(const QString& textId, int at, const QString& message);
    /// @brief Ask whether to delete scene @p scene of text @p textId with its text or its break
    /// only, and do it
    void removeScene(const QString& textId, int scene);
    /// @brief Give text @p textId @p wanted, remember it to be undone and choose card
    /// @p cardId; @p message says what was done
    void changeScenes(const QString& textId, const core::SceneText& wanted,
                      const QString& cardId, const QString& message, const QString& undoText);
    /// @brief Give text @p textId @p wanted when it is still @p expected: in its editor when it
    /// is open, else in the book; @p now gets what it is then
    /// @return false when it is something else now (m_changedElsewhere), or the book could not
    ///         take the change
    bool replaceSceneText(const QString& textId, const core::SceneText& expected,
                          const core::SceneText& wanted, core::SceneText* now);
    /// @brief Text @p textId and its first scene as they are now, also in its open editor
    [[nodiscard]] std::optional<core::SceneText> sceneTextOf(const QString& textId) const;
    /// @brief How many scenes text @p textId has now
    [[nodiscard]] int sceneCount(const QString& textId) const;
    /// @brief The KML of the open texts with changes not saved, by the ids of their elements
    [[nodiscard]] QHash<QString, QString> unsavedTexts() const;
    /// @brief The editor of text @p elementId open in a tab; nullptr: none
    [[nodiscard]] editor::BookEditor* editorOf(const QString& elementId) const;
    /// @}

    /// @brief Move text @p elementId to @p place (in its list without it) and remember it to
    /// be undone
    void moveWithUndo(const QString& elementId, const core::ElementPlace& place);
    /// @brief Give the card of text @p elementId @p text and remember it to be undone;
    /// @p message says what was done
    void changeWithUndo(const QString& elementId, const CardText& text, const QString& message,
                        const QString& undoText);

    /// @name The changes of the book, done and undone
    /// @return false when the book could not take the change
    /// @{
    bool moveTo(const QString& elementId, const core::ElementPlace& place);
    bool changeCard(const QString& elementId, const CardText& text);
    bool changeStatus(const QString& elementId, const QString& status);
    /// @}

    /// @brief What a card of text @p elementId shows that the writer can change
    [[nodiscard]] CardText cardTextOf(const QString& elementId) const;
    /// @brief The title of text @p elementId; of a scene card, its title or "The Storm, Scene 2"
    [[nodiscard]] QString titleOf(const QString& elementId) const;
    /// @brief Say @p message under the board; @p canUndo: with Undo
    void say(const QString& message, bool canUndo = false);
    /// @brief Tell the writer that the book could not take a change
    void notSaved();
    /// @brief Tell the writer that a scene undone or done again changed in the editor since,
    /// else that the book could not take the change
    void changedOrNotSaved();
    [[nodiscard]] QWidget* window() const;

    core::ProjectManager& m_book;  ///< The book of its window
    QTabWidget* m_centralTabs;
    NavigatorCoordinator* m_navigator;
    QStatusBar* m_statusBar;
    PropertiesShower m_showProperties;
    PlotBoardPanel* m_panel = nullptr;  ///< The board on its tab; none once it is deleted
    QUndoStack* m_undo = nullptr;
    QTimer* m_refreshTimer = nullptr;
    QString m_marked;     ///< The card marked to move (Ctrl+X): a text or a scene
    bool m_stale = false;  ///< The board behind another tab missed a change of the book
    int m_ownChanges = 0;  ///< > 0 while the board changes the book itself
    bool m_changedElsewhere = false;  ///< The scene undone last was changed in the editor
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PLOT_BOARD_COORDINATOR_H
