/// @file panel_tab_bar.h
/// @brief The tabs of panels docked one over another, and of the middle of the window in more
/// rows: icons, names or both, in one row or in as many rows as the width needs

#pragma once

#include <QIcon>
#include <QList>
#include <QRect>
#include <QString>
#include <QStyle>
#include <QStyleOptionTab>
#include <QWidget>

class QPainter;

namespace kalahari {
namespace gui {

/// @brief The tabs of a group of panels docked one over another, in place of Qt's (PanelTabs)
///
/// Qt gives such a group one row of names, which it shortens to a few letters when the
/// panels are many. These tabs show the panels' icons, their names or both (Look), in one row
/// whose names are shortened where they do not fit (next to the icons, they give way to them
/// where even two letters do not fit), or in as many rows as the width needs (Rows). The rows
/// stay in place: the tab of the panel in front is only marked, with a line on its side toward
/// the panel (Side).
///
/// A click chooses a tab (tabChosen()). With the focus on the tabs (the Tab key), the arrow
/// keys, Home and End choose one too. A tab dragged to another place asks to move there
/// (tabMoveRequested()); a line between the tabs shows where it goes. The tabs take every
/// click, so a click or a drag on them never floats or moves the panel.
///
/// The tabs in the middle of the window, in more rows (CentralTabs), are these tabs drawn by
/// the style as Qt's tab bar draws its own (Drawing::Style), each with its close button.
class PanelTabBar : public QWidget {
    Q_OBJECT

public:
    /// @brief What the tabs show
    enum class Look { Icons, IconsAndNames, Names };

    /// @brief Tabs that do not fit the width: in more rows, or in one row with shorter names
    enum class Rows { Wrap, Single };

    /// @brief The side of the panel the tabs are on
    enum class Side { Above, Below };

    /// @brief Who draws the tabs
    enum class Drawing {
        Own,   ///< The tabs of the panels: a frame, a line on the tab in front
        Style  ///< The style, as Qt's tab bar: the tabs in the middle of the window
    };

    /// @brief One tab
    struct Tab {
        const void* key = nullptr;  ///< What the tab stands for: it keeps a drag over new tabs
        QIcon icon;
        QString name;
        QString toolTip;            ///< The name and what else helps (the panel's keys)
    };

    /// @brief Tabs without tabs yet
    /// @param parent Parent widget
    explicit PanelTabBar(QWidget* parent = nullptr);

    /// @brief Show these tabs, @p current marked as the panel in front
    void setTabs(const QList<Tab>& tabs, int current);

    /// @brief The number of tabs
    [[nodiscard]] int count() const { return static_cast<int>(m_tabs.size()); }

    /// @brief The tab of the panel in front, -1 without tabs
    [[nodiscard]] int currentIndex() const { return m_current; }

    /// @brief Mark tab @p index as the one in front, the tabs staying as they are
    void setCurrentIndex(int index);

    /// @brief Tab @p index
    [[nodiscard]] const Tab& tab(int index) const { return m_tabs.at(index); }

    /// @brief Show icons, names or both
    void setLook(Look look);
    [[nodiscard]] Look look() const { return m_look; }

    /// @brief Wrap the tabs into more rows, or keep one row
    void setRows(Rows rows);
    [[nodiscard]] Rows rows() const { return m_rows; }

    /// @brief The side of the panel the tabs are on: the mark of the tab in front faces the
    ///        panel
    void setSide(Side side);
    [[nodiscard]] Side side() const { return m_side; }

    /// @brief The edge of the icons in pixels
    void setIconSize(int size);
    [[nodiscard]] int iconSize() const { return m_iconSize; }

    /// @brief Draw the tabs of the panels, or let the style draw them as Qt's tabs
    void setDrawing(Drawing drawing);
    [[nodiscard]] Drawing drawing() const { return m_drawing; }

    /// @brief A close button on every tab the style draws (tabCloseRequested())
    void setClosable(bool closable);
    [[nodiscard]] bool isClosable() const { return m_closable; }

    /// @brief Where the close button of tab @p index is, empty without close buttons
    [[nodiscard]] QRect closeRect(int index) const;

    /// @brief Where tab @p index is at the current width
    [[nodiscard]] QRect tabRect(int index) const;

    /// @brief The name tab @p index shows: shortened with an ellipsis where it does not fit;
    ///        empty with icons alone, and next to an icon without room for two letters
    [[nodiscard]] QString shownName(int index) const;

    /// @brief The tooltip of tab @p index: empty when the tab shows all of it
    [[nodiscard]] QString toolTipOf(int index) const;

    /// @brief The rows of tabs at the current width
    [[nodiscard]] int rowCount() const;

    /// @brief The tab at @p pos, -1 for none
    [[nodiscard]] int tabAt(const QPoint& pos) const;

    /// @brief As narrow as the tabs can be, so they never widen the panel; as high as the rows
    ///        need at the current width
    ///
    /// The height follows the width here, not by heightForWidth(): a panel's title bar takes
    /// the height of its sizeHint(), and a layout with a height for its width measures that
    /// height at the width it would like, not at its own.
    [[nodiscard]] QSize sizeHint() const override;

    /// @brief One row: every tab at its narrowest; more rows: the widest tab at its narrowest
    ///        (narrowestWidth())
    [[nodiscard]] QSize minimumSizeHint() const override;

    /// @brief The height of the rows the tabs take at @p width
    [[nodiscard]] int heightForWidth(int width) const override;

signals:
    /// @brief A tab other than the current one was chosen with a click or a key
    /// @param index The tab
    void tabChosen(int index);

    /// @brief A tab was dragged to another place
    /// @param from Its index
    /// @param to Its new index
    void tabMoveRequested(int from, int to);

    /// @brief A tab was clicked, also the current one, after tabChosen()
    /// @param index The tab
    void tabClicked(int index);

    /// @brief The close button of a tab was clicked
    /// @param index The tab
    void tabCloseRequested(int index);

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    /// @brief A tab placed at a width
    struct Placed {
        QRect rect;
        int row = 0;
        QString shown;  ///< The name as it fits
    };

    /// @brief Place the tabs at @p width
    [[nodiscard]] QList<Placed> arrange(int width) const;

    /// @brief Width of tab @p index with all its name and room around it
    [[nodiscard]] int naturalWidth(int index) const;

    /// @brief Width of tab @p index at its narrowest: its icon alone, or without icons two
    ///        letters of its name and an ellipsis
    [[nodiscard]] int narrowestWidth(int index) const;

    /// @brief Height of one tab
    [[nodiscard]] int tabHeight() const;

    /// @brief Between the tabs and the left and right of the bar, the top and bottom, and
    ///        between the tabs: none for the style's tabs, which touch
    [[nodiscard]] int marginX() const;
    [[nodiscard]] int marginY() const;
    [[nodiscard]] int spacing() const;

    /// @brief The style's option of tab @p index, as Qt's tab bar fills it, without its place
    [[nodiscard]] QStyleOptionTab styleOption(int index) const;

    /// @brief The size the style gives tab @p index showing @p text, as QTabBar::tabSizeHint()
    [[nodiscard]] QSize styleTabSize(int index, const QString& text) const;

    /// @brief The size of a close button
    [[nodiscard]] QSize closeSize() const;

    /// @brief Where the style puts part @p element of tab @p index at @p rect
    [[nodiscard]] QRect styleRect(QStyle::SubElement element, int index, const QRect& rect) const;

    /// @brief The tab whose close button is at @p pos, -1 for none
    [[nodiscard]] int closeAt(const QPoint& pos) const;

    /// @brief Draw the tabs as the style draws Qt's
    void paintStyleTabs(QPainter& painter);

    [[nodiscard]] bool showsIcons() const { return m_look != Look::Names; }
    [[nodiscard]] bool showsNames() const { return m_look != Look::Icons; }

    /// @brief The width of all the tabs in one row, each with its whole name
    [[nodiscard]] int oneRowWidth() const;

    /// @brief The width the tabs are placed at: their own once laid out, else their parent's
    ///        (they span the title bar or the panel), else one row
    [[nodiscard]] int placedWidth() const;

    /// @brief Place the tabs again, and ask for another height when the rows change
    void relayout();

    /// @brief Mark the tab under the mouse, and the close button under it
    void setHover(int index, int close = -1);

    /// @brief Where a dragged tab goes when dropped at @p pos: the gap before tab @p gap
    ///        (count() after the last), with the line that shows it; -1 outside the tabs
    void updateDrop(const QPoint& pos);

    /// @brief Forget a drag in progress
    void cancelDrag();

    QList<Tab> m_tabs;
    QList<Placed> m_placed;
    int m_current = -1;
    int m_hover = -1;
    int m_hoverClose = -1;    ///< The tab whose close button the mouse is over
    int m_pressedClose = -1;  ///< The tab whose close button the left button went down on
    int m_pressed = -1;       ///< The tab the left button went down on
    const void* m_pressedKey = nullptr;
    QPoint m_pressPos;
    bool m_dragging = false;
    int m_dropGap = -1;
    QRect m_dropLine;
    Look m_look = Look::Icons;
    Rows m_rows = Rows::Wrap;
    Side m_side = Side::Above;
    Drawing m_drawing = Drawing::Own;
    bool m_closable = false;
    int m_iconSize = 16;
};

} // namespace gui
} // namespace kalahari
