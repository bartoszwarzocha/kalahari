// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_coordinator.h
/// @brief The plot board of the book on its tab in the middle of the window, and the changes
/// of the book it asks for

#ifndef KALAHARI_GUI_PLOT_BOARD_COORDINATOR_H
#define KALAHARI_GUI_PLOT_BOARD_COORDINATOR_H

#include "kalahari/core/book_project.h"

#include <QObject>
#include <QString>

#include <functional>

class QStatusBar;
class QTabWidget;
class QTimer;
class QUndoStack;
class QWidget;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

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

    /// @brief The text marked to move (Ctrl+X); empty: none
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
    void dropBeside(const QString& elementId, const QString& anchorId, bool after);
    void dropInPart(const QString& elementId, const QString& partId);
    void add(const QString& afterId);
    void remove(const QString& elementId);
    void cut(const QString& elementId);
    void paste(const QString& afterId);
    void cancelCut();
    void undo();
    void redo();
    void showProperties(const QString& elementId, bool keys);

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
    [[nodiscard]] QString titleOf(const QString& elementId) const;
    /// @brief Say @p message under the board; @p canUndo: with Undo
    void say(const QString& message, bool canUndo = false);
    /// @brief Tell the writer that the book could not take a change
    void notSaved();
    [[nodiscard]] QWidget* window() const;

    core::ProjectManager& m_book;  ///< The book of its window
    QTabWidget* m_centralTabs;
    NavigatorCoordinator* m_navigator;
    QStatusBar* m_statusBar;
    PropertiesShower m_showProperties;
    PlotBoardPanel* m_panel = nullptr;  ///< The board on its tab; none once it is deleted
    QUndoStack* m_undo = nullptr;
    QTimer* m_refreshTimer = nullptr;
    QString m_marked;     ///< The text marked to move (Ctrl+X)
    bool m_stale = false;  ///< The board behind another tab missed a change of the book
    int m_ownChanges = 0;  ///< > 0 while the board changes the book itself
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PLOT_BOARD_COORDINATOR_H
