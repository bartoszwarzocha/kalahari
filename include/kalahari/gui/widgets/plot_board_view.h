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
#include <QSize>
#include <QString>
#include <QStringList>

#include <cstdint>

class QKeyEvent;
class QPainter;

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
/// Alt+Insert adds a scene at the end of its text; Ctrl+X marks it to move and Ctrl+V moves the
/// card marked after it (Esc forgets the mark);
/// Ctrl+Z and Ctrl+Y undo and redo a change of the board; Alt+Enter shows the card in the
/// Properties panel; the menu key and Shift+F10 open its menu. The view only asks for the
/// changes: whoever shows it makes them and gives it the cards again.
///
/// A card dragged with the mouse goes before or after the card it is dropped on, or into the
/// empty part or at the start of the part whose title it is dropped on; a scene goes before or
/// after another scene of its text. On a scene the keys move, add and delete scenes of its
/// text: Ctrl with the arrows along the scenes moves it among them, with the other arrows to
/// the text beside (moveToTextRequested()), Insert adds a scene after it, Delete deletes it.
class PlotBoardView : public QAbstractScrollArea {
    Q_OBJECT

public:
    /// @brief How the cards are arranged
    enum class Arrangement {
        Columns,  ///< The parts side by side, each over its texts in a row
        Grid      ///< The texts in rows as wide as the board, under the titles of their parts
    };

    /// @brief How large the cards are
    enum class CardSize {
        Small,   ///< Narrow, with two lines of the description
        Medium,  ///< Three lines of the description
        Large    ///< Wide, with two lines of the title and five of the description
    };

    /// @brief What a key does on the board and in the outline (boardKey()), whatever the
    /// board shows: the steps first, then the moves, then the rest
    enum class Key : std::uint8_t {
        None,         ///< A key of others
        Left,         ///< To the card on the left (Left)
        Right,        ///< To the card on the right (Right)
        Up,           ///< To the card above (Up)
        Down,         ///< To the card below (Down)
        First,        ///< To the first card (Home)
        Last,         ///< To the last card (End)
        MoveBack,     ///< Move the card back (Ctrl+Left)
        MoveForward,  ///< Move the card forward (Ctrl+Right)
        MoveUp,       ///< Move the card up (Ctrl+Up)
        MoveDown,     ///< Move the card down (Ctrl+Down)
        Open,         ///< Open its text in the editor (Enter)
        Edit,         ///< Edit the card on the board (F2)
        Add,          ///< Add a text after it (Insert)
        AddScene,     ///< Add a scene at the end of its text (Alt+Insert)
        Delete,       ///< Delete it (Delete)
        Cut,          ///< Mark it to move (Ctrl+X)
        Paste,        ///< Move the card marked after it (Ctrl+V)
        CancelCut,    ///< Forget the card marked (Esc)
        Undo,         ///< Undo a change of the board (Ctrl+Z)
        Redo,         ///< Redo a change of the board (Ctrl+Y, Ctrl+Shift+Z)
        Properties,   ///< Show it in the Properties panel (Alt+Enter)
        Menu,         ///< Open its menu (the menu key, Shift+F10)
        Scenes        ///< Open or close the drawer of its scenes (Space)
    };

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

    /// @brief An empty board, with the colors of the cards from the settings
    explicit PlotBoardView(QWidget* parent = nullptr);
    ~PlotBoardView() override;

    PlotBoardView(const PlotBoardView&) = delete;
    PlotBoardView& operator=(const PlotBoardView&) = delete;
    PlotBoardView(PlotBoardView&&) = delete;
    PlotBoardView& operator=(PlotBoardView&&) = delete;

    /// @brief What @p event does on the board, whatever the board shows; Key::None for a key
    /// of others
    [[nodiscard]] static Key boardKey(const QKeyEvent* event);

    /// @brief Whether @p key may ask for what it does now: Enter and Alt+Enter need card
    /// @p cardId chosen; F2, Alt+Insert, Delete and Ctrl+X need it in a book that can be changed
    /// (not @p readOnly); Insert needs such a book, and Ctrl+V a card @p marked too; the other
    /// keys may always
    [[nodiscard]] static bool keyAllowed(Key key, bool readOnly, const QString& cardId,
                                         const QString& marked);

    /// @brief Show @p bands; the card chosen stays chosen while the board has it, else the
    /// card at its place is chosen
    void setBands(const QList<PlotBand>& bands);

    /// @brief The bands shown
    [[nodiscard]] const QList<PlotBand>& bands() const { return m_bands; }

    /// @brief Arrange the cards as @p arrangement says
    void setArrangement(Arrangement arrangement);
    /// @brief How the cards are arranged
    [[nodiscard]] Arrangement arrangement() const { return m_arrangement; }

    /// @brief Make the cards @p size large
    void setCardSize(CardSize size);
    /// @brief How large the cards are
    [[nodiscard]] CardSize cardSize() const { return m_cardSize; }

    /// @brief Show @p fields on the cards
    void setFields(const Fields& fields);

    /// @brief Show the scenes of the texts: under their cards in columns; in a grid with the
    /// drawers of all texts open (each one opens and closes on its own then)
    void setShowScenes(bool show);
    /// @brief Whether the board shows the scenes of the texts (setShowScenes())
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
    /// @brief The card marked to move (Ctrl+X); empty: none
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
    /// @brief Move the card @p steps places back (less than 0) or forward; a scene among the
    /// scenes of its text
    void moveRequested(const QString& elementId, int steps);
    /// @brief Move scene @p cardId to the text before (@p direction less than 0) or after its
    /// text: Ctrl+Left and Ctrl+Right in columns, Ctrl+Up and Ctrl+Down in a drawer
    void moveToTextRequested(const QString& cardId, int direction);
    /// @brief Move a card dragged before or after card @p anchorId
    void dropBesideRequested(const QString& elementId, const QString& anchorId, bool after);
    /// @brief Move a card dragged to the start of part @p partId
    void dropInPartRequested(const QString& elementId, const QString& partId);
    /// @brief Add a text after the card (Insert), a scene after the card of a scene; empty: a
    /// text at the end of the board
    void addRequested(const QString& afterId);
    /// @brief Add a scene at the end of the text of the card (Alt+Insert)
    void addSceneRequested(const QString& cardId);
    /// @brief Delete the card's text (Delete)
    void deleteRequested(const QString& elementId);
    /// @brief Mark the card to move (Ctrl+X)
    void cutRequested(const QString& elementId);
    /// @brief Move the card marked after card @p afterId (Ctrl+V)
    void pasteRequested(const QString& afterId);
    /// @brief Forget the card marked (Esc)
    void cutCancelled();
    /// @brief Undo a change of the board (Ctrl+Z)
    void undoRequested();
    /// @brief Redo a change of the board (Ctrl+Y)
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
        QString id;      ///< The id of the card
        int band = 0;    ///< Its band in m_bands
        int index = 0;   ///< Its place in the band; of a scene, its text's
        int scene = -1;  ///< Of a scene: its place among the scenes of its text
        int column = 0;  ///< In columns: the column of its text, from 0
        QRect rect;      ///< Where it is
    };
    /// @brief The drawer of the scenes of a text in a grid
    struct DrawerPlace {
        QString textId;  ///< Its text
        QString title;   ///< Of its text
        QRect rect;      ///< Where it is
    };
    /// @brief A band where it is on the board
    struct BandPlace {
        int band = 0;  ///< Its band in m_bands
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
    /// @brief The rows of a grid
    struct GridRows {
        QSize card;            ///< The size of a card
        int header = 0;        ///< The height of the title of a part
        int width = 0;         ///< The width of the rows
        int perRow = 1;        ///< The cards in a row
        int perDrawerRow = 1;  ///< The cards of the scenes in a row of a drawer
        int label = 0;         ///< The height of the label of a drawer
    };

    // The keys and the mouse (plot_board_view_input.cpp)
    [[nodiscard]] Key keyOf(const QKeyEvent* event) const;
    void doKey(Key key);
    /// @brief Move card @p cardId with Ctrl and an arrow
    void moveByKey(Key key, const QString& cardId);
    /// @brief The keys for the whole board: the drawers, the menu, the mark, undo and redo
    void doBoardKey(Key key, const QString& cardId);
    [[nodiscard]] QString neighbour(Key key) const;
    /// @brief The card on the left (@p step less than 0) or on the right of @p current
    [[nodiscard]] QString sideNeighbour(const CardPlace& current, int step) const;
    /// @brief The card above (@p step less than 0) or below @p current
    [[nodiscard]] QString rowNeighbour(const CardPlace& current, int step) const;
    void startDrag();
    /// @brief The first card of a text (@p scene false) or of a scene near @p pos
    [[nodiscard]] const CardPlace* cardNear(const QPoint& pos, bool scene) const;
    /// @brief Whether a card dropped at @p pos goes to the start of the part of @p band
    [[nodiscard]] bool dropsInPart(const BandPlace& band, const QPoint& pos) const;
    /// @brief Before or after @p place: on its left or right, or above or below (@p vertical)
    [[nodiscard]] static DropTarget besideCard(const CardPlace& place, bool after, bool vertical);
    [[nodiscard]] DropTarget dropTargetAt(const QPoint& pos, const QString& dragged) const;
    /// @brief Where scene @p dragged of text @p textId goes: beside another scene of its text
    [[nodiscard]] DropTarget sceneDropTargetAt(const QPoint& pos, const QString& dragged,
                                               const QString& textId) const;
    /// @brief Where a text dropped on a part or beyond the last card of a band goes
    [[nodiscard]] DropTarget bandDropTargetAt(const QPoint& pos, const QString& dragged) const;
    /// @brief Scroll the board while a card is dragged near its edges
    void scrollNear(const QPoint& pos);

    // Where the cards are (plot_board_view_layout.cpp)
    void relayout();
    [[nodiscard]] QSize cardSizePixels() const;
    void layOutColumns(const QSize& card, int header);
    /// @brief Put text @p text and its scenes under it in a column
    /// @return The bottom of the column
    int layOutColumn(const CardPlace& text, const QSize& card);
    void layOutGrid(const QSize& card, int header);
    /// @brief Put the title and the cards of band @p band in rows from @p top
    /// @return The bottom of the band
    int layOutGridBand(int band, int top, const GridRows& rows);
    /// @brief Put the drawer of the scenes of text @p text from @p top
    /// @return The bottom of the drawer
    int layOutDrawer(const CardPlace& text, int top, const GridRows& rows);
    void updateScrollBars();
    [[nodiscard]] const CardPlace* placeOf(const QString& elementId) const;
    [[nodiscard]] const PlotCard* cardOf(const QString& elementId) const;
    [[nodiscard]] const PlotCard& cardOfPlace(const CardPlace& place) const;
    /// @brief In a grid, where "3 scenes" is on the card of a text with scenes; empty: nowhere
    [[nodiscard]] QRect scenesNoteRect(const CardPlace& place) const;
    [[nodiscard]] QString scenesNote(const PlotCard& card) const;
    [[nodiscard]] const CardPlace* cardAt(const QPoint& pos) const;
    [[nodiscard]] QPoint contentsPos(const QPoint& viewportPos) const;

    // The cards painted (plot_board_view_paint.cpp)
    void paintEmptyBoard(QPainter& painter, const QRect& shown) const;
    void paintBands(QPainter& painter, const QRect& shown) const;
    void paintBandHeader(QPainter& painter, const BandPlace& place) const;
    void paintPartTitle(QPainter& painter, const QRect& header, const PlotBand& band) const;
    void paintEmptyPart(QPainter& painter, const QRect& area) const;
    void paintDrawer(QPainter& painter, const DrawerPlace& drawer) const;
    void paintCard(QPainter& painter, const CardPlace& place) const;
    void paintCardFrame(QPainter& painter, const PlotCard& card, const QRect& rect) const;
    /// @return Where the title goes
    int paintCardHead(QPainter& painter, const PlotCard& card, const QRect& inner) const;
    /// @return The width of the state
    int paintCardState(QPainter& painter, const PlotCard& card, const QRect& inner) const;
    /// @return Where the description goes
    int paintCardTitle(QPainter& painter, const PlotCard& card, const QRect& inner,
                       int top) const;
    void paintCardDescription(QPainter& painter, const PlotCard& card, const QRect& inner,
                              int top) const;
    /// @return Where the line of the plot ends: before "3 scenes", else at @p right
    int paintScenesNote(QPainter& painter, const CardPlace& place, int right) const;
    void paintPlotLine(QPainter& painter, const PlotCard& card, const QRect& inner,
                       int right) const;
    void paintCurrentRing(QPainter& painter, const QRect& rect) const;

    // The rest (plot_board_view.cpp)
    /// @brief The place of text @p textId among the texts of the board; -1: none
    [[nodiscard]] qsizetype indexOfText(const QString& textId) const;
    /// @brief The text at place @p at among the texts of the board, or the nearest one
    [[nodiscard]] QString textAt(qsizetype at) const;
    /// @brief Make card @p elementId the card chosen, and say so
    void setCurrent(const QString& elementId);
    /// @brief The card chosen on the screen and the editor at its card, after a change
    void showCurrent();
    /// @brief The point of view, the place and the time of @p card in words
    [[nodiscard]] static QStringList plotTexts(const PlotCard& card);
    [[nodiscard]] static QString toolTipOf(const PlotCard& card);
    void choose(const QString& elementId);
    void ensureVisible(const QRect& rect);
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
