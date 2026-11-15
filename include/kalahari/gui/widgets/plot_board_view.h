// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_view.h
/// @brief The cards of the plot board: the texts of the book in the order of the book, in
/// columns of the parts or in rows, moved with the keys or the mouse

#ifndef KALAHARI_GUI_WIDGETS_PLOT_BOARD_VIEW_H
#define KALAHARI_GUI_WIDGETS_PLOT_BOARD_VIEW_H

#include "kalahari/gui/plot_board.h"

#include <QAbstractScrollArea>
#include <QList>
#include <QPoint>
#include <QRect>
#include <QSet>
#include <QString>
#include <QStringList>

class QKeyEvent;

namespace kalahari::gui {

class PlotCardEditor;

/// @brief What the writer changed of a card in its editor (F2)
struct PlotCardEdit {
    QString title;            ///< Its title
    QString description;      ///< Its short description
    core::ElementPlot plot;   ///< Its point of view, place and time; the color stays as it was
};

/// @brief The cards of the plot board, painted, with the keys of the board
///
/// The cards are the texts of the main section of the book (PlotBand), each with its kind, its
/// words and state, its title, its description and its point of view, place and time of the
/// action, in a color as the board says (PlotColors::ofCard()). In columns the parts are side
/// by side, each over its texts; in a grid the texts are in rows under the title of their part.
/// The scenes of a text are under its card in columns, and in a drawer under its row in a grid,
/// which Space or a click on "3 scenes" opens and closes (setShowScenes() opens all of them).
///
/// The keys: the arrows go to the card in their direction, Home and End to the first and the
/// last card; Ctrl with an arrow moves the card (in a grid Ctrl+Up and Ctrl+Down by a row);
/// Enter opens it, F2 edits it on the board, Insert adds a text after it, Delete deletes it,
/// Ctrl+X marks it to move and Ctrl+V moves the card marked after it (Esc forgets the mark);
/// Ctrl+Z and Ctrl+Y undo and redo a change of the board; Alt+Enter shows the card in the
/// Properties panel; the menu key and Shift+F10 open its menu. The view only asks for the
/// changes: whoever shows it makes them and gives it the cards again.
///
/// A card dragged with the mouse goes before or after the card it is dropped on, or into the
/// empty part or at the start of the part whose title it is dropped on.
class PlotBoardView : public QAbstractScrollArea {
    Q_OBJECT

public:
    /// @brief How the cards are arranged
    enum class Arrangement {
        Columns,  ///< The parts side by side, each over its texts in a row
        Grid      ///< The texts in rows as wide as the board, under the titles of their parts
    };

    /// @brief How large the cards are
    enum class CardSize { Small, Medium, Large };

    /// @brief What the cards show besides their title
    struct Fields {
        bool description = true;  ///< The short description
        bool pov = true;          ///< The point of view
        bool place = true;        ///< The place of the action
        bool time = false;        ///< The time of the action
        bool words = true;        ///< The words of the text
        bool status = true;       ///< The state of the text
    };

    /// @brief The type of the drag of a card: its id
    static constexpr const char* CARD_MIME_TYPE = "application/x-kalahari-plot-card";

    explicit PlotBoardView(QWidget* parent = nullptr);
    ~PlotBoardView() override;

    PlotBoardView(const PlotBoardView&) = delete;
    PlotBoardView& operator=(const PlotBoardView&) = delete;

    /// @brief Show @p bands; the card chosen stays chosen while the board has it, else the
    /// card at its place is chosen
    void setBands(const QList<PlotBand>& bands);

    /// @brief The bands shown
    [[nodiscard]] const QList<PlotBand>& bands() const { return m_bands; }

    /// @brief Arrange the cards as @p arrangement says
    void setArrangement(Arrangement arrangement);
    [[nodiscard]] Arrangement arrangement() const { return m_arrangement; }

    /// @brief Make the cards @p size large
    void setCardSize(CardSize size);
    [[nodiscard]] CardSize cardSize() const { return m_cardSize; }

    /// @brief Show @p fields on the cards
    void setFields(const Fields& fields);

    /// @brief Show the scenes of the texts: under their cards in columns; in a grid with the
    /// drawers of all texts open (each one opens and closes on its own then)
    void setShowScenes(bool show);
    [[nodiscard]] bool showsScenes() const { return m_showScenes; }

    /// @brief Whether the board shows the scenes of text @p textId (in a grid: its drawer is
    /// open)
    [[nodiscard]] bool scenesShown(const QString& textId) const;

    /// @brief In a grid open the drawer of the scenes of text @p textId, or close it
    /// @return false in columns, and for a text without scenes
    bool toggleScenes(const QString& textId);

    /// @brief Color the cards as @p colorBy says (PlotBoard::COLOR_STATUS...) with @p colors
    void setColoring(const QString& colorBy, const PlotColors& colors);

    /// @brief Whether the book is open read-only: the keys and the mouse change nothing
    void setReadOnly(bool readOnly);

    /// @brief Card @p elementId marked to move (Ctrl+X); empty: none
    void setMarkedCard(const QString& elementId);
    [[nodiscard]] QString markedCard() const { return m_marked; }

    /// @brief The card chosen; empty: none
    [[nodiscard]] QString currentCard() const { return m_current; }

    /// @brief Choose card @p elementId and scroll to it
    /// @return false when the board has no such card
    bool setCurrentCard(const QString& elementId);

    /// @brief Where card @p elementId is on the screen, in the coordinates of the viewport;
    /// empty when the board has no such card
    [[nodiscard]] QRect cardRect(const QString& elementId) const;

    /// @brief The cards in a row of the grid; in columns, all of them
    [[nodiscard]] int cardsPerRow() const;

    /// @brief Edit card @p elementId on the board: its title, description, point of view,
    /// place and time, with the points of view @p povs and places @p places to choose from
    /// @return false when the board has no such card or the book is read-only
    bool editCard(const QString& elementId, const QStringList& povs, const QStringList& places);

    /// @brief The editor of a card open on the board; nullptr: none
    [[nodiscard]] PlotCardEditor* cardEditor() const { return m_editor; }

    /// @brief The card's text for the screen reader: kind, title, state, words, description
    [[nodiscard]] QString spokenText(const QString& elementId) const;

    [[nodiscard]] QSize sizeHint() const override;

signals:
    /// @brief Another card was chosen; empty: none
    void currentCardChanged(const QString& elementId);
    /// @brief Open the text of the card in the editor (Enter, a double-click)
    void openRequested(const QString& elementId);
    /// @brief Edit the card on the board (F2)
    void editRequested(const QString& elementId);
    /// @brief The writer finished editing the card on the board (Ctrl+Enter)
    void editFinished(const QString& elementId, const kalahari::gui::PlotCardEdit& edit);
    /// @brief Move the card @p steps places back (less than 0) or forward
    void moveRequested(const QString& elementId, int steps);
    /// @brief Move a card dragged before or after card @p anchorId
    void dropBesideRequested(const QString& elementId, const QString& anchorId, bool after);
    /// @brief Move a card dragged to the start of part @p partId
    void dropInPartRequested(const QString& elementId, const QString& partId);
    /// @brief Add a text after the card (Insert); empty: at the end of the board
    void addRequested(const QString& afterId);
    /// @brief Delete the card's text (Delete)
    void deleteRequested(const QString& elementId);
    /// @brief Mark the card to move (Ctrl+X)
    void cutRequested(const QString& elementId);
    /// @brief Move the card marked after card @p afterId (Ctrl+V)
    void pasteRequested(const QString& afterId);
    /// @brief Forget the card marked (Esc)
    void cutCancelled();
    /// @brief Undo (Ctrl+Z) or redo (Ctrl+Y) a change of the board
    void undoRequested();
    void redoRequested();
    /// @brief Show the card in the Properties panel, with the keys there (Alt+Enter)
    void propertiesRequested(const QString& elementId);
    /// @brief Open the menu of the card (empty: of the board) at @p globalPos
    void menuRequested(const QString& elementId, const QPoint& globalPos);

protected:
    bool event(QEvent* event) override;
    bool viewportEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void changeEvent(QEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    /// @brief A card where it is on the board, in the coordinates of the contents
    struct CardPlace {
        QString id;
        int band = 0;    ///< Its band in m_bands
        int index = 0;   ///< Its place in the band; of a scene, its text's
        int scene = -1;  ///< Of a scene: its place among the scenes of its text
        int column = 0;  ///< In columns: the column of its text, from 0
        QRect rect;
    };
    /// @brief The drawer of the scenes of a text in a grid
    struct DrawerPlace {
        QString textId;
        QString title;  ///< Of its text
        QRect rect;
    };
    /// @brief A band where it is on the board
    struct BandPlace {
        int band = 0;
        QRect header;  ///< Its title; empty for texts outside the parts
        QRect area;    ///< Its cards, or the place of an empty part
    };
    /// @brief Where a card dragged goes
    struct DropTarget {
        QString anchorId;  ///< Before or after this card
        bool after = false;
        QString partId;    ///< Else at the start of this part
        QRect marker;      ///< Where the mark is drawn, in the coordinates of the contents
        [[nodiscard]] bool isValid() const { return !anchorId.isEmpty() || !partId.isEmpty(); }
    };

    /// @brief The keys of the board: what a key does, or nothing for a key of others
    enum class Key {
        None, Left, Right, Up, Down, First, Last, MoveBack, MoveForward, MoveUp, MoveDown,
        Open, Edit, Add, Delete, Cut, Paste, CancelCut, Undo, Redo, Properties, Menu, Scenes
    };
    [[nodiscard]] Key keyOf(const QKeyEvent* event) const;
    void doKey(Key key);

    void relayout();
    [[nodiscard]] QSize cardSizePixels() const;
    [[nodiscard]] const CardPlace* placeOf(const QString& elementId) const;
    [[nodiscard]] const PlotCard* cardOf(const QString& elementId) const;
    [[nodiscard]] const PlotCard& cardOfPlace(const CardPlace& place) const;
    /// @brief In a grid, where "3 scenes" is on the card of a text with scenes; empty: nowhere
    [[nodiscard]] QRect scenesNoteRect(const CardPlace& place) const;
    [[nodiscard]] QString scenesNote(const PlotCard& card) const;
    void layOutColumns(const QSize& card, int header);
    void layOutGrid(const QSize& card, int header);
    void paintDrawer(QPainter& painter, const DrawerPlace& drawer) const;
    [[nodiscard]] const CardPlace* cardAt(const QPoint& contentsPos) const;
    [[nodiscard]] QPoint contentsPos(const QPoint& viewportPos) const;
    [[nodiscard]] QString neighbour(Key key) const;
    void choose(const QString& elementId);
    void ensureVisible(const QRect& rect);
    void updateScrollBars();
    void paintCard(QPainter& painter, const CardPlace& place) const;
    void paintBandHeader(QPainter& painter, const BandPlace& band) const;
    [[nodiscard]] DropTarget dropTargetAt(const QPoint& contentsPos, const QString& dragged) const;
    void startDrag();
    void closeEditor(bool save);
    void placeEditor();
    void tellScreenReader();

    QList<PlotBand> m_bands;
    QList<CardPlace> m_cards;
    QList<BandPlace> m_bandPlaces;
    QList<DrawerPlace> m_drawers;
    bool m_showScenes = true;
    QSet<QString> m_toggled;  ///< In a grid: the texts whose drawers are not as m_showScenes
    QSize m_contentsSize;
    Arrangement m_arrangement = Arrangement::Columns;
    CardSize m_cardSize = CardSize::Medium;
    Fields m_fields;
    QString m_colorBy = QString::fromLatin1(PlotBoard::COLOR_STATUS);
    PlotColors m_colors;
    QStringList m_povs;  ///< The points of view of the cards, for their colors
    bool m_readOnly = false;
    QString m_current;
    QString m_marked;
    QPoint m_pressPos;            ///< Where the left button was pressed, in the viewport
    QString m_pressed;            ///< The card it was pressed on
    DropTarget m_drop;            ///< Where a card dragged over the board goes
    PlotCardEditor* m_editor = nullptr;
};

}  // namespace kalahari::gui

Q_DECLARE_METATYPE(kalahari::gui::PlotCardEdit)

#endif  // KALAHARI_GUI_WIDGETS_PLOT_BOARD_VIEW_H
