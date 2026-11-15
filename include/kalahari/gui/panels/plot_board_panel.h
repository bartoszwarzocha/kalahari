// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_panel.h
/// @brief The tab of the plot board in the middle of the window: the board of the cards or the
/// outline, the switches of their look, and what was just done

#ifndef KALAHARI_GUI_PANELS_PLOT_BOARD_PANEL_H
#define KALAHARI_GUI_PANELS_PLOT_BOARD_PANEL_H

#include "kalahari/gui/plot_board.h"
#include "kalahari/gui/widgets/plot_board_view.h"

#include <QPoint>
#include <QString>
#include <QStringList>
#include <QWidget>

class QComboBox;
class QLabel;
class QMenu;
class QStackedWidget;
class QTableView;
class QToolButton;

namespace kalahari::gui {

class InfoBar;
class PlotOutlineDelegate;
class PlotOutlineModel;

/// @brief The plot board on its tab: the cards of the texts of the book (PlotBoardView) or
/// the outline, the same texts in a table (PlotOutlineModel)
///
/// Over them: Board | Outline, the arrangement of the cards (Columns | Grid), their size, what
/// colors them, and how many texts and words the book has; the choices are kept in the
/// settings (PlotBoard::LAYOUT_SETTING...). Under them a bar says what was just done, with Undo.
///
/// The keys of the outline are those of the board where a table allows them: the arrows go
/// from cell to cell, F2 or a letter edits a cell, Tab goes to the next one, Enter keeps it
/// and Esc leaves it; Enter on a cell not edited opens the text, Ctrl+Up and Ctrl+Down move
/// its row. The panel asks for every change of the book: PlotBoardCoordinator makes it.
class PlotBoardPanel : public QWidget {
    Q_OBJECT

public:
    explicit PlotBoardPanel(QWidget* parent = nullptr);
    ~PlotBoardPanel() override;

    PlotBoardPanel(const PlotBoardPanel&) = delete;
    PlotBoardPanel& operator=(const PlotBoardPanel&) = delete;

    /// @brief Show @p bands on the board and in the outline
    void setBands(const QList<PlotBand>& bands);

    /// @brief Whether the book is open read-only: nothing can be changed
    void setReadOnly(bool readOnly);
    [[nodiscard]] bool isReadOnly() const { return m_readOnly; }

    /// @brief The points of view and the places the editors suggest
    void setSuggestions(const QStringList& povs, const QStringList& places);

    /// @brief Show the board (PlotBoard::VIEW_BOARD) or the outline (VIEW_OUTLINE)
    void showView(const QString& view);

    /// @brief PlotBoard::VIEW_BOARD or VIEW_OUTLINE, whichever is shown
    [[nodiscard]] QString view() const;

    /// @brief Give the keys to the board or the outline, whichever is shown
    void focusView();

    /// @brief The text chosen on the board or in the outline; empty: none
    [[nodiscard]] QString currentCard() const;

    /// @brief Choose text @p elementId on the board and in the outline
    bool setCurrentCard(const QString& elementId);

    /// @brief Card @p elementId marked to move (Ctrl+X); empty: none
    void setMarkedCard(const QString& elementId);

    /// @brief Edit text @p elementId: its card on the board (F2), its title in the outline
    bool editCard(const QString& elementId);

    /// @brief Say @p message under the board; @p canUndo: with an Undo button
    void showMessage(const QString& message, bool canUndo);

    /// @brief What the bar under the board says; empty when it is hidden
    [[nodiscard]] QString message() const;

    /// @brief Read the look of the board from the settings again, with the theme's colors
    void applySettings();

    /// @brief The menu of text @p elementId, as the menu key opens it
    [[nodiscard]] QMenu* cardMenu(const QString& elementId);

    /// @brief The board, the outline and its model
    [[nodiscard]] PlotBoardView* board() const { return m_board; }
    [[nodiscard]] QTableView* outline() const { return m_outline; }
    [[nodiscard]] PlotOutlineModel* outlineModel() const { return m_model; }

signals:
    /// @brief The text chosen on the board or in the outline changed; empty: none
    void currentCardChanged(const QString& elementId);
    /// @brief Open the text in the editor
    void openRequested(const QString& elementId);
    /// @brief Keep what was written on a card (F2) or in a cell of the outline
    void editFinished(const QString& elementId, const kalahari::gui::PlotCardEdit& edit);
    /// @brief Give the text state @p status ("draft"...)
    void statusRequested(const QString& elementId, const QString& status);
    /// @brief Give the card its own color @p color (PlotColors::ownNames()); empty: none
    void colorRequested(const QString& elementId, const QString& color);
    /// @brief Move the text @p steps places back (less than 0) or forward
    void moveRequested(const QString& elementId, int steps);
    /// @brief Move the text before or after text @p anchorId
    void dropBesideRequested(const QString& elementId, const QString& anchorId, bool after);
    /// @brief Move the text to the start of part @p partId
    void dropInPartRequested(const QString& elementId, const QString& partId);
    /// @brief Add a text after text @p afterId; empty: at the end of the main section
    void addRequested(const QString& afterId);
    /// @brief Delete the text from the book
    void deleteRequested(const QString& elementId);
    /// @brief Mark the text to move (Ctrl+X)
    void cutRequested(const QString& elementId);
    /// @brief Move the text marked after text @p afterId (Ctrl+V)
    void pasteRequested(const QString& afterId);
    /// @brief Forget the text marked (Esc)
    void cutCancelled();
    /// @brief Undo or redo the last change of the board
    void undoRequested();
    void redoRequested();
    /// @brief Show the text in the Properties panel with the keys there (Alt+Enter)
    void propertiesRequested(const QString& elementId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void buildHeader(QWidget* header);
    void updateTotals();
    void updateSwitches();
    void onOutlineEdited(const QString& elementId, int column, const QString& value);
    void showMenu(const QString& elementId, const QPoint& globalPos);
    /// @brief Whether a key of the outline is the board's (false: the table's own key), and
    /// with @p run, do what it does
    bool outlineKey(const QKeyEvent* key, bool run);

    QList<PlotBand> m_bands;
    bool m_readOnly = false;
    QStackedWidget* m_stack = nullptr;
    PlotBoardView* m_board = nullptr;
    QTableView* m_outline = nullptr;
    PlotOutlineModel* m_model = nullptr;
    PlotOutlineDelegate* m_delegate = nullptr;
    InfoBar* m_infoBar = nullptr;
    QToolButton* m_boardButton = nullptr;
    QToolButton* m_outlineButton = nullptr;
    QLabel* m_layoutLabel = nullptr;
    QToolButton* m_columnsButton = nullptr;
    QToolButton* m_gridButton = nullptr;
    QLabel* m_sizeLabel = nullptr;
    QToolButton* m_smallButton = nullptr;
    QToolButton* m_mediumButton = nullptr;
    QToolButton* m_largeButton = nullptr;
    QComboBox* m_colorBy = nullptr;
    QLabel* m_totals = nullptr;
    QMenu* m_menu = nullptr;
    QStringList m_povs;
    QStringList m_places;
    int m_settingsListener = 0;  ///< core::SettingsManager::subscribe() id
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_PLOT_BOARD_PANEL_H
